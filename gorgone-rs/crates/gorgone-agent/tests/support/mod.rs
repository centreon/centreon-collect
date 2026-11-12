//! Fake `AgentGateway` driven by the tests, and helpers to run an agent
//! against it.

// Shared by several test binaries, none of which uses every helper.
#![allow(dead_code)]

use std::collections::HashMap;
use std::net::SocketAddr;
use std::path::Path;
use std::sync::{Arc, Mutex};
use std::time::Duration;

use gorgone_agent::{AgentConfig, AgentError};
use gorgone_proto::NODE_ID_METADATA;
use gorgone_proto::agent::agent_gateway_server::{AgentGateway, AgentGatewayServer};
use gorgone_proto::agent::job_event::Body;
use gorgone_proto::agent::job_finished::Outcome;
use gorgone_proto::agent::service_control::Operation;
use gorgone_proto::agent::{
    Action, AgentFrame, ApplyConfig, BlobChunk, Cancel, Command, CommandResult, Dispatch,
    EngineCommands, EventAck, FetchBlobRequest, Heartbeat, Hello, Job, JobEvent, JobFinished,
    RunCommands, ServerFrame, ServiceControl, Welcome, action, agent_frame, server_frame,
};
use tokio::sync::{mpsc, oneshot};
use tokio::task::JoinHandle;
use tokio_stream::wrappers::{ReceiverStream, TcpListenerStream};
use tonic::metadata::MetadataMap;
use tonic::transport::{Server, ServerTlsConfig};
use tonic::{Request, Response, Status, Streaming};

/// Upper bound of every wait, far above what a healthy run needs.
pub const PATIENCE: Duration = Duration::from_secs(20);

/// One `FetchBlob` call seen by the fake server.
#[derive(Debug, Clone)]
pub struct FetchRecord {
    pub digest: String,
    pub node_id: Option<String>,
    pub authorization: Option<String>,
}

#[derive(Default)]
struct State {
    blobs: Mutex<HashMap<String, Vec<u8>>>,
    fetches: Mutex<Vec<FetchRecord>>,
    fetch_delay: Mutex<Duration>,
}

struct Gateway {
    state: Arc<State>,
    sessions: mpsc::UnboundedSender<SessionHandle>,
}

fn metadata(map: &MetadataMap, key: &str) -> Option<String> {
    map.get(key)
        .and_then(|value| value.to_str().ok())
        .map(str::to_owned)
}

#[tonic::async_trait]
impl AgentGateway for Gateway {
    type SessionStream = ReceiverStream<Result<ServerFrame, Status>>;
    type FetchBlobStream = ReceiverStream<Result<BlobChunk, Status>>;

    async fn session(
        &self,
        request: Request<Streaming<AgentFrame>>,
    ) -> Result<Response<Self::SessionStream>, Status> {
        let authorization = metadata(request.metadata(), "authorization");
        let node_id = metadata(request.metadata(), NODE_ID_METADATA);
        let mut inbound = request.into_inner();
        let Some(AgentFrame {
            frame: Some(agent_frame::Frame::Hello(hello)),
        }) = inbound.message().await?
        else {
            return Err(Status::invalid_argument("hello expected"));
        };

        let (to_agent, outbound) = mpsc::channel(64);
        let (frames, from_agent) = mpsc::unbounded_channel();
        tokio::spawn(async move {
            while let Ok(Some(frame)) = inbound.message().await {
                if frames.send(frame).is_err() {
                    break;
                }
            }
        });
        let welcome = Welcome {
            session_id: "fake-session".to_owned(),
            heartbeat_interval_s: 1,
            server_version: "fake".to_owned(),
        };
        to_agent
            .send(Ok(ServerFrame {
                frame: Some(server_frame::Frame::Welcome(welcome)),
            }))
            .await
            .map_err(|_| Status::internal("welcome"))?;
        let handle = SessionHandle {
            hello,
            authorization,
            node_id,
            to_agent,
            from_agent,
            auto_ack: true,
            arrivals: Vec::new(),
        };
        self.sessions
            .send(handle)
            .map_err(|_| Status::unavailable("test over"))?;
        Ok(Response::new(ReceiverStream::new(outbound)))
    }

    async fn fetch_blob(
        &self,
        request: Request<FetchBlobRequest>,
    ) -> Result<Response<Self::FetchBlobStream>, Status> {
        let record = FetchRecord {
            digest: request.get_ref().digest.clone(),
            node_id: metadata(request.metadata(), NODE_ID_METADATA),
            authorization: metadata(request.metadata(), "authorization"),
        };
        let digest = record.digest.clone();
        self.state.fetches.lock().unwrap().push(record);
        let delay = *self.state.fetch_delay.lock().unwrap();
        tokio::time::sleep(delay).await;
        let blob = self
            .state
            .blobs
            .lock()
            .unwrap()
            .get(&digest)
            .cloned()
            .ok_or_else(|| Status::not_found("unknown blob"))?;
        let (chunks, stream) = mpsc::channel(4);
        tokio::spawn(async move {
            // Small chunks, so that a bundle always spans several messages.
            for data in blob.chunks(64) {
                let chunk = BlobChunk {
                    data: data.to_vec(),
                };
                if chunks.send(Ok(chunk)).await.is_err() {
                    break;
                }
            }
        });
        Ok(Response::new(ReceiverStream::new(stream)))
    }
}

/// Fake gateway listening on a local port.
pub struct FakeServer {
    pub addr: SocketAddr,
    state: Arc<State>,
    sessions: mpsc::UnboundedReceiver<SessionHandle>,
}

impl FakeServer {
    /// Starts a clear-text server.
    pub async fn start() -> Self {
        Self::start_with(None).await
    }

    /// Starts a server, with TLS when `tls` is given.
    pub async fn start_with(tls: Option<ServerTlsConfig>) -> Self {
        let listener = tokio::net::TcpListener::bind("127.0.0.1:0").await.unwrap();
        let addr = listener.local_addr().unwrap();
        let state = Arc::new(State::default());
        let (sessions_tx, sessions) = mpsc::unbounded_channel();
        let gateway = Gateway {
            state: state.clone(),
            sessions: sessions_tx,
        };
        let mut builder = Server::builder();
        if let Some(tls) = tls {
            builder = builder.tls_config(tls).unwrap();
        }
        let router = builder.add_service(AgentGatewayServer::new(gateway));
        tokio::spawn(router.serve_with_incoming(TcpListenerStream::new(listener)));
        Self {
            addr,
            state,
            sessions,
        }
    }

    /// Next session opened by an agent.
    pub async fn next_session(&mut self) -> SessionHandle {
        tokio::time::timeout(PATIENCE, self.sessions.recv())
            .await
            .expect("no agent session in time")
            .expect("server stopped")
    }

    pub fn add_blob(&self, digest: &str, blob: Vec<u8>) {
        self.state
            .blobs
            .lock()
            .unwrap()
            .insert(digest.to_owned(), blob);
    }

    pub fn fetches(&self) -> Vec<FetchRecord> {
        self.state.fetches.lock().unwrap().clone()
    }

    pub fn set_fetch_delay(&self, delay: Duration) {
        *self.state.fetch_delay.lock().unwrap() = delay;
    }
}

/// Server side of one agent session.
pub struct SessionHandle {
    pub hello: Hello,
    pub authorization: Option<String>,
    pub node_id: Option<String>,
    /// Acknowledge every event as it is read.
    pub auto_ack: bool,
    /// Every event read so far, in arrival order.
    pub arrivals: Vec<JobEvent>,
    to_agent: mpsc::Sender<Result<ServerFrame, Status>>,
    from_agent: mpsc::UnboundedReceiver<AgentFrame>,
}

impl SessionHandle {
    async fn send(&self, frame: server_frame::Frame) {
        self.to_agent
            .send(Ok(ServerFrame { frame: Some(frame) }))
            .await
            .expect("agent stream closed");
    }

    pub async fn dispatch(&self, job: Job) {
        self.send(server_frame::Frame::Dispatch(Dispatch { job: Some(job) }))
            .await;
    }

    pub async fn cancel(&self, job_id: &str) {
        self.send(server_frame::Frame::Cancel(Cancel {
            job_id: job_id.to_owned(),
        }))
        .await;
    }

    pub async fn ack(&self, event: &JobEvent) {
        self.send(server_frame::Frame::Ack(EventAck {
            job_id: event.job_id.clone(),
            seq: event.seq,
        }))
        .await;
    }

    /// Next frame of the agent; `None` once it closed its stream.
    async fn next_frame(&mut self) -> Option<agent_frame::Frame> {
        tokio::time::timeout(PATIENCE, self.from_agent.recv())
            .await
            .expect("no frame from the agent in time")
            .and_then(|frame| frame.frame)
    }

    /// Next job event, skipping heartbeats; `None` once the stream closed.
    pub async fn try_next_event(&mut self) -> Option<JobEvent> {
        loop {
            match self.next_frame().await? {
                agent_frame::Frame::Event(event) => {
                    if self.auto_ack {
                        self.ack(&event).await;
                    }
                    self.arrivals.push(event.clone());
                    return Some(event);
                }
                _ => continue,
            }
        }
    }

    pub async fn next_event(&mut self) -> JobEvent {
        self.try_next_event().await.expect("agent stream closed")
    }

    /// Reads until the terminal event of `job_id`; returns the events of that job.
    pub async fn job_events(&mut self, job_id: &str) -> Vec<JobEvent> {
        let mut events = Vec::new();
        loop {
            let event = self.next_event().await;
            if event.job_id != job_id {
                continue;
            }
            let terminal = matches!(event.body, Some(Body::Finished(_)));
            events.push(event);
            if terminal {
                return events;
            }
        }
    }

    pub async fn next_heartbeat(&mut self) -> Heartbeat {
        loop {
            match self.next_frame().await.expect("agent stream closed") {
                agent_frame::Frame::Heartbeat(heartbeat) => return heartbeat,
                agent_frame::Frame::Event(event) => self.arrivals.push(event),
                agent_frame::Frame::Hello(_) => {}
            }
        }
    }

    /// Asserts that no job event arrives for `quiet`.
    pub async fn expect_no_event(&mut self, quiet: Duration) {
        let deadline = tokio::time::Instant::now() + quiet;
        loop {
            match tokio::time::timeout_at(deadline, self.from_agent.recv()).await {
                Err(_) | Ok(None) => return,
                Ok(Some(AgentFrame {
                    frame: Some(agent_frame::Frame::Event(event)),
                })) => panic!("unexpected event {event:?}"),
                Ok(Some(_)) => {}
            }
        }
    }
}

/// Agent running in the test process.
pub struct Agent {
    task: JoinHandle<Result<(), AgentError>>,
    shutdown: Option<oneshot::Sender<()>>,
}

impl Agent {
    /// Starts an agent with the TOML `config`.
    pub fn start(config: &str) -> Self {
        let config = AgentConfig::from_toml(config).expect("valid test configuration");
        let (shutdown, stopped) = oneshot::channel::<()>();
        let task = tokio::spawn(gorgone_agent::run(config, async move {
            let _ = stopped.await;
        }));
        Self {
            task,
            shutdown: Some(shutdown),
        }
    }

    /// Asks the agent to stop, without waiting.
    pub fn request_stop(&mut self) {
        if let Some(shutdown) = self.shutdown.take() {
            let _ = shutdown.send(());
        }
    }

    /// Stops the agent and returns what `run` returned.
    pub async fn stop(mut self) -> Result<(), AgentError> {
        self.request_stop();
        tokio::time::timeout(PATIENCE, &mut self.task)
            .await
            .expect("agent did not stop in time")
            .expect("agent task panicked")
    }
}

/// Agent in a runtime of its own, so that it can die like a killed process:
/// every task is dropped where it stands and no shutdown code runs.
pub struct CrashingAgent {
    kill: oneshot::Sender<()>,
    thread: std::thread::JoinHandle<()>,
}

impl CrashingAgent {
    pub fn start(config: &str) -> Self {
        let config = AgentConfig::from_toml(config).expect("valid test configuration");
        let (kill, killed) = oneshot::channel::<()>();
        let thread = std::thread::spawn(move || {
            let runtime = tokio::runtime::Builder::new_multi_thread()
                .worker_threads(2)
                .enable_all()
                .build()
                .expect("agent runtime");
            runtime.block_on(async {
                tokio::select! {
                    ended = gorgone_agent::run(config, std::future::pending()) => {
                        panic!("agent stopped by itself: {ended:?}");
                    }
                    _ = killed => {}
                }
            });
            // Returns once every task of the agent is gone.
            drop(runtime);
        });
        Self { kill, thread }
    }

    pub async fn crash(self) {
        let _ = self.kill.send(());
        tokio::task::spawn_blocking(move || self.thread.join().expect("agent thread"))
            .await
            .expect("join task");
    }
}

/// Top-level keys of a test configuration; append tables after it.
pub fn base_config(server_url: &str) -> String {
    format!(
        r#"
node_id = "7"
server_url = "{server_url}"
token = "test-token"
default_command_timeout_s = 10
shutdown_grace_s = 10
max_concurrent_jobs = 4
max_output_bytes = 4096

[reconnect]
min_ms = 20
max_ms = 100
"#
    )
}

pub fn path_str(path: &Path) -> &str {
    path.to_str().expect("UTF-8 test path")
}

pub fn job(id: &str, kind: action::Kind) -> Job {
    Job {
        id: id.to_owned(),
        deadline_unix_ms: 0,
        action: Some(Action { kind: Some(kind) }),
    }
}

pub fn command(line: &str) -> Command {
    Command {
        command: line.to_owned(),
        timeout_s: 0,
        continue_on_error: false,
    }
}

pub fn run_commands(id: &str, commands: Vec<Command>) -> Job {
    job(id, action::Kind::RunCommands(RunCommands { commands }))
}

pub fn engine_commands(id: &str, commands: &[&str]) -> Job {
    let commands = commands.iter().map(|c| (*c).to_owned()).collect();
    job(
        id,
        action::Kind::EngineCommands(EngineCommands { commands }),
    )
}

pub fn apply_config(id: &str, target: &str, digest: &str, size: usize) -> Job {
    job(
        id,
        action::Kind::ApplyConfig(ApplyConfig {
            target: target.to_owned(),
            digest: digest.to_owned(),
            size: size as u64,
        }),
    )
}

pub fn service_control(id: &str, service: &str, operation: Operation) -> Job {
    job(
        id,
        action::Kind::ServiceControl(ServiceControl {
            service: service.to_owned(),
            operation: operation.into(),
        }),
    )
}

/// Checks the shape every job must have and returns its parts: sequence
/// numbers from 1, `started` first, exactly one `finished`, at the end.
pub fn dissect(events: &[JobEvent]) -> (Vec<CommandResult>, Vec<String>, JobFinished) {
    let seqs: Vec<u64> = events.iter().map(|e| e.seq).collect();
    assert_eq!(seqs, (1..=events.len() as u64).collect::<Vec<_>>());
    assert!(
        matches!(events[0].body, Some(Body::Started(_))),
        "{events:?}"
    );
    let mut results = Vec::new();
    let mut progress = Vec::new();
    let mut finished = Vec::new();
    for event in &events[1..] {
        assert!(event.time_unix_ms > 0);
        match event.body.clone().expect("event body") {
            Body::Started(_) => panic!("second started event"),
            Body::CommandResult(result) => results.push(result),
            Body::Progress(p) => progress.push(p.message),
            Body::Finished(f) => finished.push(f),
        }
    }
    assert_eq!(finished.len(), 1, "{events:?}");
    assert!(matches!(
        events.last().and_then(|e| e.body.as_ref()),
        Some(Body::Finished(_))
    ));
    (results, progress, finished.remove(0))
}

pub fn outcome(finished: &JobFinished) -> Outcome {
    finished.outcome()
}
