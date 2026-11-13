//! Fake Centreon Engine gRPC API recording what the agent calls.

use std::path::{Path, PathBuf};
use std::sync::{Arc, Mutex};
use std::time::Duration;

use gorgone_proto::engine::engine_server::{Engine, EngineServer};
use gorgone_proto::engine::engine_signal_process::Process;
use gorgone_proto::engine::{CommandSuccess, EngineSignalProcess, ExternalCommand, Version};
use tokio::sync::oneshot;
use tokio::task::JoinHandle;
use tokio_stream::wrappers::{TcpListenerStream, UnixListenerStream};
use tonic::transport::Server;
use tonic::{Request, Response, Status};

/// One call received by the fake engine.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum EngineCall {
    ExternalCommand(String),
    /// `SignalProcess` with its process and whether a schedule time was set.
    Signal(Process, bool),
    Shutdown,
}

/// Version the fake engine announces.
pub const VERSION: &str = "25.10.3";

#[derive(Default)]
struct State {
    calls: Mutex<Vec<EngineCall>>,
    /// External commands holding this text are refused.
    refused: Mutex<Option<String>>,
    /// Time every call takes to answer.
    delay: Mutex<Duration>,
}

struct Api(Arc<State>);

impl Api {
    async fn record(&self, call: EngineCall) {
        self.0.calls.lock().unwrap().push(call);
        let delay = *self.0.delay.lock().unwrap();
        tokio::time::sleep(delay).await;
    }
}

#[tonic::async_trait]
impl Engine for Api {
    // Not recorded: the agent probes it on its own schedule.
    async fn get_version(&self, _: Request<()>) -> Result<Response<Version>, Status> {
        let delay = *self.0.delay.lock().unwrap();
        tokio::time::sleep(delay).await;
        Ok(Response::new(Version {
            major: 25,
            minor: 10,
            patch: 3,
        }))
    }

    async fn signal_process(
        &self,
        request: Request<EngineSignalProcess>,
    ) -> Result<Response<CommandSuccess>, Status> {
        let signal = request.into_inner();
        self.record(EngineCall::Signal(
            signal.process(),
            signal.scheduled_time > 0,
        ))
        .await;
        // The real engine leaves `value` unset here.
        Ok(Response::new(CommandSuccess { value: false }))
    }

    async fn shutdown_program(&self, _: Request<()>) -> Result<Response<()>, Status> {
        self.record(EngineCall::Shutdown).await;
        Ok(Response::new(()))
    }

    async fn process_external_command(
        &self,
        request: Request<ExternalCommand>,
    ) -> Result<Response<CommandSuccess>, Status> {
        let line = request.into_inner().command;
        self.record(EngineCall::ExternalCommand(line.clone())).await;
        let refused = self.0.refused.lock().unwrap().clone();
        if refused.is_some_and(|text| line.contains(&text)) {
            return Err(Status::invalid_argument(format!("unknown command {line}")));
        }
        Ok(Response::new(CommandSuccess { value: true }))
    }
}

/// Fake engine API listening on a local port or on a Unix socket.
pub struct FakeEngine {
    url: String,
    /// Set when listening on a Unix socket.
    socket: Option<PathBuf>,
    state: Arc<State>,
    /// Stop signal and task of the running server; `None` once stopped.
    server: Option<(oneshot::Sender<()>, JoinHandle<()>)>,
}

impl FakeEngine {
    pub async fn start() -> Self {
        let listener = tokio::net::TcpListener::bind("127.0.0.1:0").await.unwrap();
        let url = format!("http://{}", listener.local_addr().unwrap());
        let state = Arc::new(State::default());
        let (stop, stopped) = oneshot::channel::<()>();
        let router = Server::builder().add_service(EngineServer::new(Api(state.clone())));
        let task = tokio::spawn(async move {
            let incoming = TcpListenerStream::new(listener);
            let stopped = async { drop(stopped.await) };
            router
                .serve_with_incoming_shutdown(incoming, stopped)
                .await
                .unwrap();
        });
        Self {
            url,
            socket: None,
            state,
            server: Some((stop, task)),
        }
    }

    /// Starts an engine listening on the Unix socket `path`.
    pub fn start_unix(path: &Path) -> Self {
        let mut engine = Self {
            url: format!("unix:{}", path.to_str().expect("UTF-8 test path")),
            socket: Some(path.to_path_buf()),
            state: Arc::new(State::default()),
            server: None,
        };
        engine.restart();
        engine
    }

    /// Ends the engine as a stopped container does: connections are closed
    /// and the socket file is gone.
    pub async fn stop(&mut self) {
        let Some((stop, task)) = self.server.take() else {
            return;
        };
        let _ = stop.send(());
        tokio::time::timeout(super::PATIENCE, task)
            .await
            .expect("fake engine did not stop in time")
            .expect("fake engine task");
        if let Some(socket) = &self.socket {
            std::fs::remove_file(socket).unwrap();
        }
    }

    /// Listens again on a new socket file at the same path; the calls
    /// recorded so far are kept.
    pub fn restart(&mut self) {
        assert!(self.server.is_none(), "engine still running");
        let socket = self.socket.as_ref().expect("Unix socket engine");
        let listener = tokio::net::UnixListener::bind(socket).unwrap();
        let (stop, stopped) = oneshot::channel::<()>();
        let router = Server::builder().add_service(EngineServer::new(Api(self.state.clone())));
        let task = tokio::spawn(async move {
            let incoming = UnixListenerStream::new(listener);
            let stopped = async { drop(stopped.await) };
            router
                .serve_with_incoming_shutdown(incoming, stopped)
                .await
                .unwrap();
        });
        self.server = Some((stop, task));
    }

    pub fn url(&self) -> String {
        self.url.clone()
    }

    /// Every call received so far, in arrival order.
    pub fn calls(&self) -> Vec<EngineCall> {
        self.state.calls.lock().unwrap().clone()
    }

    /// External command lines received so far, in arrival order.
    pub fn lines(&self) -> Vec<String> {
        self.calls()
            .into_iter()
            .filter_map(|call| match call {
                EngineCall::ExternalCommand(line) => Some(line),
                _ => None,
            })
            .collect()
    }

    pub fn refuse_commands_holding(&self, text: &str) {
        *self.state.refused.lock().unwrap() = Some(text.to_owned());
    }

    pub fn set_delay(&self, delay: Duration) {
        *self.state.delay.lock().unwrap() = delay;
    }
}
