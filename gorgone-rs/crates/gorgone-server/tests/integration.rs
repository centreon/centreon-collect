//! End-to-end tests: in-process replicas, a fake agent speaking the gRPC
//! contract, and the REST API.
//!
//! Each test uses its own SQLite file, or the shared database named by
//! `GORGONE_TEST_DATABASE_URL` (node ids are unique per test).

use std::net::SocketAddr;
use std::sync::atomic::{AtomicU64, Ordering};
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

use gorgone_proto::NODE_ID_METADATA;
use gorgone_proto::agent::action::Kind;
use gorgone_proto::agent::agent_frame::Frame as AgentFrameKind;
use gorgone_proto::agent::agent_gateway_client::AgentGatewayClient;
use gorgone_proto::agent::job_event::Body;
use gorgone_proto::agent::job_finished::Outcome;
use gorgone_proto::agent::server_frame::Frame;
use gorgone_proto::agent::{
    AgentFrame, CommandResult, EventAck, FetchBlobRequest, Heartbeat, Hello, Job, JobEvent,
    JobFinished, JobStarted, ServerFrame, Welcome,
};
use gorgone_proto::peer::WakeRequest;
use gorgone_proto::peer::peer_client::PeerClient;
use gorgone_server::{Config, Server, ServerHandle};
use serde_json::{Value, json};
use tokio::io::{AsyncRead, AsyncReadExt, AsyncWrite, AsyncWriteExt};
use tokio::net::TcpStream;
use tokio::sync::mpsc;
use tokio_stream::wrappers::ReceiverStream;
use tonic::transport::{Certificate, Channel, ClientTlsConfig, Identity};
use tonic::{Code, Request, Status, Streaming};

const STEP: Duration = Duration::from_secs(5);

/// Database of one test.
struct TestDb {
    url: String,
    _dir: Option<tempfile::TempDir>,
}

impl TestDb {
    fn new() -> Self {
        if let Ok(url) = std::env::var("GORGONE_TEST_DATABASE_URL") {
            return Self { url, _dir: None };
        }
        let dir = tempfile::tempdir().unwrap();
        let url = format!("sqlite://{}/gorgone.db?mode=rwc", dir.path().display());
        Self {
            url,
            _dir: Some(dir),
        }
    }
}

/// Unique across tests, and across runs sharing one database.
fn unique_node() -> String {
    static NEXT: AtomicU64 = AtomicU64::new(0);
    let nanos = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap()
        .as_nanos();
    let rank = NEXT.fetch_add(1, Ordering::Relaxed);
    format!("node-{nanos:x}-{:x}-{rank}", std::process::id())
}

async fn start(db: &TestDb, tweak: impl FnOnce(&mut Config)) -> ServerHandle {
    let loopback = SocketAddr::from(([127, 0, 0, 1], 0));
    let mut config = Config {
        rest_listen: loopback,
        agent_listen: loopback,
        peer_listen: loopback,
        database_url: db.url.clone(),
        sweep_interval_ms: 200,
        heartbeat_interval_s: 5,
        auto_enroll: true,
        // Some thirty test replicas run at once on one database server:
        // small pools keep them under its limit of 151 connections.
        db_max_connections: Some(3),
        // Tests exercise the API without credentials unless they say so.
        insecure_no_auth: true,
        ..Config::default()
    };
    tweak(&mut config);
    Server::start(config).await.unwrap()
}

/// Minimal agent: one session, driven step by step by the test.
struct FakeAgent {
    client: AgentGatewayClient<Channel>,
    node_id: String,
    token: Option<String>,
    tx: mpsc::Sender<AgentFrame>,
    inbound: Streaming<ServerFrame>,
    welcome: Welcome,
}

fn authorize<T>(request: &mut Request<T>, token: Option<&str>) {
    if let Some(token) = token {
        let value = format!("Bearer {token}").parse().unwrap();
        request.metadata_mut().insert("authorization", value);
    }
}

impl FakeAgent {
    async fn connect(addr: SocketAddr, node_id: &str, token: Option<&str>) -> Result<Self, Status> {
        let channel = Channel::from_shared(format!("http://{addr}"))
            .unwrap()
            .connect()
            .await
            .unwrap();
        Self::connect_channel(channel, node_id, token).await
    }

    async fn connect_channel(
        channel: Channel,
        node_id: &str,
        token: Option<&str>,
    ) -> Result<Self, Status> {
        Self::open(channel, Some(node_id), node_id, token).await
    }

    /// Opens a session announcing `metadata_node` in the request metadata
    /// and `node_id` in the `Hello` frame.
    async fn open(
        channel: Channel,
        metadata_node: Option<&str>,
        node_id: &str,
        token: Option<&str>,
    ) -> Result<Self, Status> {
        let mut client = AgentGatewayClient::new(channel);
        let (tx, rx) = mpsc::channel(64);
        let hello = Hello {
            node_id: node_id.to_owned(),
            agent_version: "fake-1.0".into(),
            capabilities: vec!["run_commands".into()],
        };
        tx.send(AgentFrame {
            frame: Some(AgentFrameKind::Hello(hello)),
        })
        .await
        .unwrap();
        let mut request = Request::new(ReceiverStream::new(rx));
        authorize(&mut request, token);
        if let Some(metadata_node) = metadata_node {
            request
                .metadata_mut()
                .insert(NODE_ID_METADATA, metadata_node.parse().unwrap());
        }
        let mut inbound = client.session(request).await?.into_inner();
        let first = tokio::time::timeout(STEP, inbound.message())
            .await
            .expect("welcome in time")?;
        let Some(ServerFrame {
            frame: Some(Frame::Welcome(welcome)),
        }) = first
        else {
            panic!("first server frame must be Welcome, got {first:?}");
        };
        Ok(Self {
            client,
            node_id: node_id.to_owned(),
            token: token.map(str::to_owned),
            tx,
            inbound,
            welcome,
        })
    }

    async fn next_frame(&mut self, wait: Duration) -> Option<Frame> {
        match tokio::time::timeout(wait, self.inbound.message()).await {
            Ok(Ok(Some(frame))) => frame.frame,
            _ => None,
        }
    }

    /// Reads the session until the server ends it, and returns the frames
    /// received meanwhile. Panics, telling which case it is, when the
    /// stream is still open after `wait` or when a job was dispatched to it.
    async fn expect_closed(&mut self, wait: Duration) -> Vec<Frame> {
        let mut frames = Vec::new();
        let deadline = tokio::time::Instant::now() + wait;
        loop {
            match tokio::time::timeout_at(deadline, self.inbound.message()).await {
                // End of stream, cleanly or with a status.
                Ok(Ok(None) | Err(_)) => break,
                Ok(Ok(Some(frame))) => frames.extend(frame.frame),
                Err(_) => panic!(
                    "session of {} still open after {wait:?}; frames received meanwhile: {frames:?}",
                    self.node_id
                ),
            }
        }
        let dispatched: Vec<&Frame> = frames
            .iter()
            .filter(|frame| matches!(frame, Frame::Dispatch(_)))
            .collect();
        assert!(
            dispatched.is_empty(),
            "a closing session of {} was given jobs: {dispatched:?}",
            self.node_id
        );
        frames
    }

    async fn next_dispatch(&mut self) -> Job {
        loop {
            match self.next_frame(STEP).await {
                Some(Frame::Dispatch(dispatch)) => return dispatch.job.unwrap(),
                Some(_) => {}
                None => panic!("no dispatch received"),
            }
        }
    }

    async fn next_cancel(&mut self) -> String {
        loop {
            match self.next_frame(STEP).await {
                Some(Frame::Cancel(cancel)) => return cancel.job_id,
                Some(_) => {}
                None => panic!("no cancel received"),
            }
        }
    }

    async fn next_ack(&mut self) -> EventAck {
        loop {
            match self.next_frame(STEP).await {
                Some(Frame::Ack(ack)) => return ack,
                Some(_) => {}
                None => panic!("no ack received"),
            }
        }
    }

    async fn send(&self, frame: AgentFrameKind) {
        self.tx
            .send(AgentFrame { frame: Some(frame) })
            .await
            .unwrap();
    }

    /// Sends an event and waits for its acknowledgement.
    async fn event(&mut self, job_id: &str, seq: u64, body: Body) {
        let time_unix_ms = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_millis() as i64;
        self.send(AgentFrameKind::Event(JobEvent {
            job_id: job_id.to_owned(),
            seq,
            time_unix_ms,
            body: Some(body),
        }))
        .await;
        let ack = self.next_ack().await;
        assert_eq!((ack.job_id.as_str(), ack.seq), (job_id, seq));
    }

    async fn fetch_blob(&mut self, digest: &str) -> Result<Vec<Vec<u8>>, Status> {
        let mut request = Request::new(FetchBlobRequest {
            digest: digest.to_owned(),
        });
        authorize(&mut request, self.token.as_deref());
        request
            .metadata_mut()
            .insert(NODE_ID_METADATA, self.node_id.parse().unwrap());
        let mut stream = self.client.fetch_blob(request).await?.into_inner();
        let mut chunks = Vec::new();
        while let Some(chunk) = stream.message().await? {
            chunks.push(chunk.data);
        }
        Ok(chunks)
    }
}

fn command_result(index: u32, stdout: &str) -> Body {
    Body::CommandResult(CommandResult {
        index,
        command: "echo ok".into(),
        exit_code: 0,
        stdout: stdout.into(),
        stderr: String::new(),
        duration_ms: 12,
        timed_out: false,
    })
}

fn finished(outcome: Outcome, message: &str) -> Body {
    Body::Finished(JobFinished {
        outcome: outcome.into(),
        message: message.into(),
    })
}

/// One HTTP/1.1 request, sent on its own connection.
struct Call {
    addr: SocketAddr,
    method: &'static str,
    path: String,
    headers: Vec<(&'static str, String)>,
    body: Vec<u8>,
}

impl Call {
    fn header(mut self, name: &'static str, value: impl Into<String>) -> Self {
        self.headers.push((name, value.into()));
        self
    }

    fn json(self, body: &Value) -> Self {
        self.header("content-type", "application/json")
            .body(body.to_string())
    }

    fn body(mut self, body: impl Into<Vec<u8>>) -> Self {
        self.body = body.into();
        self
    }

    /// Status and body of the answer.
    async fn send(self) -> std::io::Result<(u16, String)> {
        let stream = TcpStream::connect(self.addr).await?;
        self.send_on(stream).await
    }

    /// Same on an established connection (TLS, for instance).
    async fn send_on<S>(self, mut stream: S) -> std::io::Result<(u16, String)>
    where
        S: AsyncRead + AsyncWrite + Unpin,
    {
        let mut request = format!(
            "{} {} HTTP/1.1\r\nhost: {}\r\nconnection: close\r\ncontent-length: {}\r\n",
            self.method,
            self.path,
            self.addr,
            self.body.len()
        );
        for (name, value) in &self.headers {
            request.push_str(&format!("{name}: {value}\r\n"));
        }
        request.push_str("\r\n");
        let mut bytes = request.into_bytes();
        bytes.extend_from_slice(&self.body);
        // A server refusing the body answers and closes before reading it
        // all: the answer matters more than the failed write.
        let written = stream.write_all(&bytes).await;
        let mut raw = Vec::new();
        let read = stream.read_to_end(&mut raw).await;
        match parse_response(&raw) {
            Some(response) => Ok(response),
            None => Err(written
                .and(read.map(|_| ()))
                .err()
                .unwrap_or_else(|| std::io::Error::other("malformed HTTP response"))),
        }
    }
}

/// Status and body of a complete HTTP/1.1 response; the body may be chunked
/// (server-sent events are).
fn parse_response(raw: &[u8]) -> Option<(u16, String)> {
    let end = raw.windows(4).position(|window| window == b"\r\n\r\n")?;
    let head = std::str::from_utf8(&raw[..end]).ok()?;
    let mut lines = head.split("\r\n");
    let status = lines.next()?.split(' ').nth(1)?.parse().ok()?;
    let chunked = lines.any(|line| {
        let line = line.to_ascii_lowercase();
        line.starts_with("transfer-encoding:") && line.contains("chunked")
    });
    let mut rest = &raw[end + 4..];
    if !chunked {
        return Some((status, String::from_utf8_lossy(rest).into_owned()));
    }
    let mut body = Vec::new();
    loop {
        let line = rest.windows(2).position(|window| window == b"\r\n")?;
        let size = std::str::from_utf8(&rest[..line]).ok()?;
        let size = usize::from_str_radix(size.split(';').next()?.trim(), 16).ok()?;
        if size == 0 {
            return Some((status, String::from_utf8_lossy(&body).into_owned()));
        }
        let chunk = rest.get(line + 2..line + 2 + size)?;
        body.extend_from_slice(chunk);
        rest = rest.get(line + 2 + size + 2..)?;
    }
}

#[test]
fn test_client_parses_plain_and_chunked_responses() {
    let plain = b"HTTP/1.1 404 Not Found\r\ncontent-length: 2\r\n\r\nno";
    assert_eq!(parse_response(plain), Some((404, "no".into())));
    let chunked =
        b"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n3\r\nabc\r\n2;x=y\r\nde\r\n0\r\n\r\n";
    assert_eq!(parse_response(chunked), Some((200, "abcde".into())));
    // Cut short: no answer rather than half of one.
    assert_eq!(parse_response(&chunked[..chunked.len() - 7]), None);
    assert_eq!(parse_response(b"HTTP/1.1 200 OK\r\n"), None);
}

/// REST client of one replica.
struct Rest {
    addr: SocketAddr,
    bearer: Option<String>,
}

impl Rest {
    fn new(server: &ServerHandle) -> Self {
        Self {
            addr: server.rest_addr,
            bearer: None,
        }
    }

    fn request(&self, method: &'static str, path: &str) -> Call {
        let call = Call {
            addr: self.addr,
            method,
            path: path.to_owned(),
            headers: Vec::new(),
            body: Vec::new(),
        };
        match &self.bearer {
            Some(token) => call.header("authorization", format!("Bearer {token}")),
            None => call,
        }
    }

    async fn get(&self, path: &str) -> (u16, Value) {
        Self::json(self.request("GET", path)).await
    }

    async fn post(&self, path: &str, body: &Value) -> (u16, Value) {
        Self::json(self.request("POST", path).json(body)).await
    }

    /// Plaintext request, whatever the listener expects.
    async fn get_plain(&self, path: &str) -> std::io::Result<(u16, String)> {
        tokio::time::timeout(STEP, self.request("GET", path).send())
            .await
            .unwrap_or_else(|_| Err(std::io::ErrorKind::TimedOut.into()))
    }

    async fn json(call: Call) -> (u16, Value) {
        Self::decode(call.send().await.unwrap())
    }

    fn decode((status, text): (u16, String)) -> (u16, Value) {
        (
            status,
            serde_json::from_str(&text).unwrap_or(Value::String(text)),
        )
    }
}

fn codes(log: &Value) -> Vec<i64> {
    log["data"]
        .as_array()
        .unwrap()
        .iter()
        .map(|row| row["code"].as_i64().unwrap())
        .collect()
}

#[tokio::test]
async fn legacy_submit_runs_on_the_agent_and_logs_keep_the_perl_shape() {
    let db = TestDb::new();
    let server = start(&db, |_| {}).await;
    let rest = Rest::new(&server);
    let node = unique_node();
    let mut agent = FakeAgent::connect(server.agent_addr, &node, None)
        .await
        .unwrap();
    assert!(!agent.welcome.session_id.is_empty());
    assert_eq!(agent.welcome.heartbeat_interval_s, 5);

    let (status, body) = rest
        .post(
            &format!("/api/nodes/{node}/core/action/command"),
            &json!([{ "command": "echo ok", "timeout": "10" }]),
        )
        .await;
    assert_eq!(status, 200, "{body}");
    let token = body["token"].as_str().unwrap().to_owned();

    let job = agent.next_dispatch().await;
    assert_eq!(job.id, token);
    assert!(job.deadline_unix_ms > 0);
    let Some(Kind::RunCommands(run)) = job.action.unwrap().kind else {
        panic!("expected RunCommands");
    };
    assert_eq!(run.commands.len(), 1);
    assert_eq!(run.commands[0].command, "echo ok");
    assert_eq!(run.commands[0].timeout_s, 10);
    assert!(run.commands[0].continue_on_error);

    agent
        .send(AgentFrameKind::Heartbeat(Heartbeat {
            running_jobs: 1,
            ..Heartbeat::default()
        }))
        .await;
    agent.event(&token, 1, Body::Started(JobStarted {})).await;
    let (_, running) = rest.get(&format!("/v1/jobs/{token}")).await;
    assert_eq!(running["state"], "running");
    agent.event(&token, 2, command_result(0, "ok\n")).await;
    agent.event(&token, 3, finished(Outcome::Ok, "")).await;

    // Legacy log: rows of the Perl history table, `data` being a JSON string.
    let (status, log) = rest.get(&format!("/api/log/{token}")).await;
    assert_eq!(status, 200);
    assert_eq!(log["message"], "Logs found");
    assert_eq!(log["token"], token.as_str());
    assert_eq!(codes(&log), [0, 4, 100, 2]);
    let row = &log["data"][2];
    for key in ["id", "etime", "instant"] {
        assert!(row[key].is_i64(), "{key} must be an integer: {row}");
    }
    assert!(row["ctime"].is_f64());
    assert_eq!(row["token"], token.as_str());
    let data: Value =
        serde_json::from_str(row["data"].as_str().expect("data is a string")).unwrap();
    assert_eq!(data["command"], "echo ok");
    assert_eq!(data["result"], json!({ "exit_code": 0, "stdout": "ok\n" }));
    assert!(data["metrics"]["duration"].is_i64());
    assert!(data["metadata"].is_null());
    let last: Value = serde_json::from_str(log["data"][3]["data"].as_str().unwrap()).unwrap();
    assert_eq!(
        last["message"],
        "commands processing has finished successfully"
    );

    // Same answer through the node-scoped route and with a legacy wait.
    let (_, scoped) = rest
        .get(&format!("/api/nodes/{node}/log/{token}?log_wait=100000"))
        .await;
    assert_eq!(codes(&scoped), [0, 4, 100, 2]);

    // Native view.
    let (status, job) = rest.get(&format!("/v1/jobs/{token}?wait=5")).await;
    assert_eq!(status, 200);
    assert_eq!(job["state"], "succeeded");
    assert_eq!(job["kind"], "run_commands");
    assert_eq!(job["attempts"], 1);
    assert_eq!(job["events"].as_array().unwrap().len(), 4);
    assert_eq!(job["events"][2]["data"]["stdout"], "ok\n");
    assert_eq!(
        job["action"]["run_commands"]["commands"][0]["command"],
        "echo ok"
    );

    // Event stream: replays everything and closes on the terminal event.
    let (status, stream) = rest.get(&format!("/v1/jobs/{token}/events")).await;
    assert_eq!(status, 200);
    let stream = stream.as_str().unwrap();
    assert_eq!(stream.matches("event: job_event").count(), 4);

    // Other legacy shapes.
    let (status, none) = rest.get("/api/log/unknown-token").await;
    assert_eq!(status, 200);
    assert_eq!(
        none,
        json!({ "error": "no_log", "message": "No log found for token", "data": [], "token": "unknown-token" })
    );
    let (status, unknown) = rest.get("/api/nodes/1/whatever").await;
    assert_eq!(status, 404);
    assert_eq!(
        unknown,
        json!({ "error": "method_unknown", "message": "Method not implemented", "http_response_code": "404" })
    );
    let (_, constatus) = rest.get("/api/internal/constatus").await;
    assert_eq!(constatus["action"], "constatus");
    assert_eq!(constatus["data"][&node]["ping_ok"], 1);
    let (_, thumbprint) = rest
        .get(&format!("/api/nodes/{node}/internal/thumbprint"))
        .await;
    assert_eq!(thumbprint["action"], "getthumbprint");
    assert!(
        !thumbprint["data"]["thumbprint"]
            .as_str()
            .unwrap()
            .is_empty()
    );
    let (_, information) = rest.get("/api/internal/information").await;
    assert_eq!(information["action"], "information");
    assert!(information["data"]["api_endpoints"].is_object());

    // Engine commands through the legacy route.
    let (status, body) = rest
        .post(
            &format!("/api/nodes/{node}/centreon/engine/command"),
            &json!({ "commands": ["[1] RESTART_PROGRAM"] }),
        )
        .await;
    assert_eq!(status, 200, "{body}");
    let job = agent.next_dispatch().await;
    assert_eq!(job.id, body["token"].as_str().unwrap());
    let Some(Kind::EngineCommands(engine)) = job.action.unwrap().kind else {
        panic!("expected EngineCommands");
    };
    assert_eq!(engine.commands, ["[1] RESTART_PROGRAM"]);

    // Probes and metrics.
    let (status, _) = rest.get("/healthz").await;
    assert_eq!(status, 200);
    let (status, _) = rest.get("/readyz").await;
    assert_eq!(status, 200);
    let (status, metrics) = rest.get("/metrics").await;
    assert_eq!(status, 200);
    assert!(
        metrics
            .as_str()
            .unwrap()
            .contains("gorgone_jobs_submitted_total")
    );

    server.shutdown().await;
}

#[tokio::test]
async fn job_submitted_while_the_node_is_offline_is_delivered_on_connection() {
    let db = TestDb::new();
    let server = start(&db, |_| {}).await;
    let rest = Rest::new(&server);
    let node = unique_node();

    let (status, body) = rest
        .post(
            &format!("/v1/nodes/{node}/jobs"),
            &json!({ "service_control": { "service": "engine", "operation": "reload" } }),
        )
        .await;
    assert_eq!(status, 202, "{body}");
    let id = body["id"].as_str().unwrap().to_owned();
    let (_, job) = rest.get(&format!("/v1/jobs/{id}")).await;
    assert_eq!(job["state"], "queued");
    assert_eq!(job["events"][0]["code"], 0);

    let mut agent = FakeAgent::connect(server.agent_addr, &node, None)
        .await
        .unwrap();
    let job = agent.next_dispatch().await;
    assert_eq!(job.id, id);
    let Some(Kind::ServiceControl(control)) = job.action.unwrap().kind else {
        panic!("expected ServiceControl");
    };
    assert_eq!((control.service.as_str(), control.operation), ("engine", 3));
    let (_, job) = rest.get(&format!("/v1/jobs/{id}")).await;
    assert_eq!(job["state"], "dispatched");

    agent
        .event(&id, 1, finished(Outcome::Rejected, "service not allowed"))
        .await;
    let (_, job) = rest.get(&format!("/v1/jobs/{id}?wait=5")).await;
    assert_eq!(job["state"], "failed");
    let (_, log) = rest.get(&format!("/api/log/{id}")).await;
    assert_eq!(codes(&log), [0, 1]);

    // Invalid submissions are refused.
    let (status, _) = rest
        .post(&format!("/v1/nodes/{node}/jobs"), &json!({ "nope": {} }))
        .await;
    assert!((400..500).contains(&status));
    let (status, _) = rest
        .post(
            &format!("/v1/nodes/{node}/jobs"),
            &json!({ "run_commands": { "commands": [] } }),
        )
        .await;
    assert_eq!(status, 400);
    let (status, _) = rest.get("/v1/jobs/unknown").await;
    assert_eq!(status, 404);

    server.shutdown().await;
}

#[tokio::test]
async fn job_submitted_on_another_replica_is_delivered_through_the_peer_wake_up() {
    let db = TestDb::new();
    // Slow sweep on the owner: only the wake-up can deliver in time.
    let owner = start(&db, |c| c.sweep_interval_ms = 5000).await;
    let other = start(&db, |_| {}).await;
    assert_ne!(owner.replica_id, other.replica_id);
    let node = unique_node();
    let mut agent = FakeAgent::connect(owner.agent_addr, &node, None)
        .await
        .unwrap();
    // Let the connection-time drain finish before submitting.
    tokio::time::sleep(Duration::from_millis(300)).await;

    let rest = Rest::new(&other);
    let submitted = Instant::now();
    let (status, body) = rest
        .post(
            &format!("/v1/nodes/{node}/jobs"),
            &json!({ "run_commands": { "commands": [{ "command": "echo ok" }] } }),
        )
        .await;
    assert_eq!(status, 202, "{body}");
    let id = body["id"].as_str().unwrap().to_owned();

    let job = agent.next_dispatch().await;
    assert_eq!(job.id, id);
    assert!(
        submitted.elapsed() < Duration::from_secs(2),
        "delivered by the sweep, not by the wake-up: {:?}",
        submitted.elapsed()
    );

    // The result is committed by the owner and read from the other replica.
    let waiter = tokio::spawn({
        let path = format!("/v1/jobs/{id}?wait=10");
        let rest = Rest::new(&other);
        async move { rest.get(&path).await }
    });
    tokio::time::sleep(Duration::from_millis(100)).await;
    agent.event(&id, 1, command_result(0, "from owner")).await;
    agent.event(&id, 2, finished(Outcome::Ok, "done")).await;
    let (status, job) = waiter.await.unwrap();
    assert_eq!(status, 200);
    assert_eq!(job["state"], "succeeded");
    assert_eq!(job["events"][1]["data"]["stdout"], "from owner");
    let (_, log) = rest.get(&format!("/api/log/{id}")).await;
    assert_eq!(codes(&log), [0, 100, 2]);

    // The node is seen connected from both replicas, on the owner.
    let (_, nodes) = rest.get("/v1/nodes").await;
    let entry = |nodes: &Value| {
        nodes["nodes"]
            .as_array()
            .unwrap()
            .iter()
            .find(|n| n["id"] == node.as_str())
            .cloned()
            .unwrap()
    };
    assert_eq!(entry(&nodes)["connected"], true);
    assert_eq!(
        entry(&nodes)["session"]["replica_id"],
        owner.replica_id.as_str()
    );

    // Graceful shutdown removes the sessions of the replica from the store.
    owner.shutdown().await;
    let (_, nodes) = rest.get("/v1/nodes").await;
    assert_eq!(entry(&nodes)["connected"], false);
    other.shutdown().await;
}

#[tokio::test]
async fn duplicate_event_seq_is_idempotent() {
    let db = TestDb::new();
    let server = start(&db, |_| {}).await;
    let rest = Rest::new(&server);
    let node = unique_node();
    let mut agent = FakeAgent::connect(server.agent_addr, &node, None)
        .await
        .unwrap();
    let (_, body) = rest
        .post(
            &format!("/api/nodes/{node}/core/action/command"),
            &json!([{ "command": "echo ok" }]),
        )
        .await;
    let id = body["token"].as_str().unwrap().to_owned();
    assert_eq!(agent.next_dispatch().await.id, id);

    // The resend is acknowledged again and changes nothing.
    agent.event(&id, 1, command_result(0, "first")).await;
    agent.event(&id, 1, command_result(0, "resent")).await;
    agent.event(&id, 2, finished(Outcome::Ok, "")).await;
    agent.event(&id, 2, finished(Outcome::Failed, "")).await;

    let (_, job) = rest.get(&format!("/v1/jobs/{id}")).await;
    assert_eq!(job["state"], "succeeded");
    let events = job["events"].as_array().unwrap();
    assert_eq!(events.len(), 3);
    assert_eq!(events[1]["data"]["stdout"], "first");
    assert_eq!(events[2]["code"], 2);

    server.shutdown().await;
}

#[tokio::test]
async fn job_past_its_deadline_expires_and_is_never_dispatched() {
    let db = TestDb::new();
    let server = start(&db, |_| {}).await;
    let rest = Rest::new(&server);
    let node = unique_node();
    let (status, body) = rest
        .post(
            &format!("/v1/nodes/{node}/jobs"),
            &json!({ "engine_commands": { "commands": ["[1] X"] }, "deadline_s": 1 }),
        )
        .await;
    assert_eq!(status, 202, "{body}");
    let id = body["id"].as_str().unwrap().to_owned();

    let (_, job) = rest.get(&format!("/v1/jobs/{id}?wait=10")).await;
    assert_eq!(job["state"], "expired");
    let (_, log) = rest.get(&format!("/api/log/{id}")).await;
    assert_eq!(codes(&log), [0, 1]);

    let mut agent = FakeAgent::connect(server.agent_addr, &node, None)
        .await
        .unwrap();
    assert!(agent.next_frame(Duration::from_millis(700)).await.is_none());

    server.shutdown().await;
}

#[tokio::test]
async fn bundle_upload_then_fetch_blob_round_trip() {
    let db = TestDb::new();
    let server = start(&db, |_| {}).await;
    let rest = Rest::new(&server);
    let node = unique_node();
    let mut agent = FakeAgent::connect(server.agent_addr, &node, None)
        .await
        .unwrap();

    // Incompressible content, so that the bundle spans several chunks.
    let src = tempfile::tempdir().unwrap();
    let mut seed = 0x9e37_79b9_7f4a_7c15_u64;
    let noise: Vec<u8> = (0..(5 << 19))
        .map(|_| {
            seed ^= seed << 13;
            seed ^= seed >> 7;
            seed ^= seed << 17;
            (seed >> 24) as u8
        })
        .collect();
    std::fs::write(src.path().join("centengine.cfg"), "cfg_dir=/etc\n").unwrap();
    std::fs::write(src.path().join("noise.bin"), &noise).unwrap();
    let bundle = gorgone_core::bundle::pack(src.path()).unwrap();
    assert!(bundle.len() > 2 << 20);
    let digest = gorgone_core::bundle::digest(&bundle);

    let (status, body) = Rest::json(
        rest.request("PUT", &format!("/v1/nodes/{node}/config/engine"))
            .body(bundle.clone()),
    )
    .await;
    assert_eq!(status, 202, "{body}");
    assert_eq!(body["digest"], digest.as_str());
    assert_eq!(body["size"], bundle.len());

    let job = agent.next_dispatch().await;
    assert_eq!(job.id, body["id"].as_str().unwrap());
    let Some(Kind::ApplyConfig(apply)) = job.action.unwrap().kind else {
        panic!("expected ApplyConfig");
    };
    assert_eq!(apply.target, "engine");
    assert_eq!(apply.digest, digest);
    assert_eq!(apply.size, bundle.len() as u64);

    let chunks = agent.fetch_blob(&apply.digest).await.unwrap();
    assert!(chunks.len() >= 3);
    assert!(chunks.iter().all(|c| c.len() <= 1 << 20));
    let fetched = chunks.concat();
    assert_eq!(gorgone_core::bundle::digest(&fetched), digest);
    assert_eq!(fetched, bundle);

    // The agent side accepts what the server stored.
    let dest = tempfile::tempdir().unwrap();
    let options = gorgone_core::bundle::ApplyOptions {
        marker: ".digest",
        prune: true,
        max_unpacked: gorgone_core::bundle::DEFAULT_MAX_UNPACKED,
    };
    let report = gorgone_core::bundle::apply(&fetched, &digest, dest.path(), &options).unwrap();
    assert_eq!(report.written, 2);

    let unknown = agent.fetch_blob(&"0".repeat(64)).await.unwrap_err();
    assert_eq!(unknown.code(), Code::NotFound);
    let malformed = agent.fetch_blob("../etc/passwd").await.unwrap_err();
    assert_eq!(malformed.code(), Code::InvalidArgument);

    // Something that is not a bundle is refused and submits nothing.
    let (status, _) = Rest::json(
        rest.request("PUT", &format!("/v1/nodes/{node}/config/engine"))
            .body("not a bundle"),
    )
    .await;
    assert_eq!(status, 400);
    // An ApplyConfig naming a blob the store does not have is refused too.
    let (status, _) = rest
        .post(
            &format!("/v1/nodes/{node}/jobs"),
            &json!({ "apply_config": { "target": "engine", "digest": "0".repeat(64) } }),
        )
        .await;
    assert_eq!(status, 400);

    server.shutdown().await;
}

#[tokio::test]
async fn agent_token_and_rest_bearer_are_enforced() {
    let db = TestDb::new();
    let server = start(&db, |c| {
        c.auto_enroll = false;
        c.rest.token = Some("rest-test-token".into());
    })
    .await;
    let node = unique_node();

    // REST: probes are open, the API is not.
    let mut rest = Rest::new(&server);
    assert_eq!(rest.get("/healthz").await.0, 200);
    assert_eq!(rest.get("/readyz").await.0, 200);
    assert_eq!(rest.get("/metrics").await.0, 401);
    assert_eq!(rest.get("/v1/nodes").await.0, 401);
    assert_eq!(rest.get("/api/internal/thumbprint").await.0, 401);
    rest.bearer = Some("wrong".into());
    assert_eq!(rest.get("/v1/nodes").await.0, 401);
    rest.bearer = None;
    let basic = |pair: &str| {
        use base64::Engine;
        let encoded = base64::engine::general_purpose::STANDARD.encode(pair);
        rest.request("GET", "/v1/nodes")
            .header("authorization", format!("Basic {encoded}"))
    };
    assert_eq!(Rest::json(basic("web:rest-test-token")).await.0, 200);
    assert_eq!(Rest::json(basic("web:wrong")).await.0, 401);
    rest.bearer = Some("rest-test-token".into());
    assert_eq!(rest.get("/v1/nodes").await.0, 200);
    assert_eq!(rest.get("/metrics").await.0, 200);

    // Agent: unknown node, then missing and wrong token.
    let refused = FakeAgent::connect(server.agent_addr, &node, None).await;
    assert_eq!(refused.err().unwrap().code(), Code::Unauthenticated);

    let (status, enrolled) = Rest::json(rest.request("PUT", &format!("/v1/nodes/{node}"))).await;
    assert_eq!(status, 200, "{enrolled}");
    let token = enrolled["token"].as_str().unwrap().to_owned();
    assert_eq!(token.len(), 64);

    for bad in [None, Some("wrong-token")] {
        let refused = FakeAgent::connect(server.agent_addr, &node, bad).await;
        assert_eq!(refused.err().unwrap().code(), Code::Unauthenticated);
    }
    let mut agent = FakeAgent::connect(server.agent_addr, &node, Some(&token))
        .await
        .unwrap();

    // FetchBlob is authenticated the same way.
    agent.token = Some("wrong-token".into());
    let refused = agent.fetch_blob(&"0".repeat(64)).await.unwrap_err();
    assert_eq!(refused.code(), Code::Unauthenticated);
    agent.token = Some(token);
    let unknown = agent.fetch_blob(&"0".repeat(64)).await.unwrap_err();
    assert_eq!(unknown.code(), Code::NotFound);

    // Removing the node closes its session and forbids a new one.
    let (status, _) = Rest::json(rest.request("DELETE", &format!("/v1/nodes/{node}"))).await;
    assert_eq!(status, 204);
    agent.expect_closed(STEP).await;
    let refused = FakeAgent::connect(server.agent_addr, &node, agent.token.as_deref()).await;
    assert_eq!(refused.err().unwrap().code(), Code::Unauthenticated);

    server.shutdown().await;
}

#[tokio::test]
async fn newer_session_replaces_the_older_one_and_lost_dispatch_is_requeued() {
    let db = TestDb::new();
    let server = start(&db, |_| {}).await;
    let rest = Rest::new(&server);
    let node = unique_node();
    let mut first = FakeAgent::connect(server.agent_addr, &node, None)
        .await
        .unwrap();
    let (_, body) = rest
        .post(
            &format!("/api/nodes/{node}/core/action/command"),
            &json!([{ "command": "echo ok" }]),
        )
        .await;
    let id = body["token"].as_str().unwrap().to_owned();
    assert_eq!(first.next_dispatch().await.id, id);

    // The agent restarts before producing any event: the job comes back.
    let mut second = FakeAgent::connect(server.agent_addr, &node, None)
        .await
        .unwrap();
    assert_ne!(first.welcome.session_id, second.welcome.session_id);
    // The replaced session ends, and gets nothing once the new one is welcomed.
    let stray = first.expect_closed(STEP).await;
    assert!(
        stray.is_empty(),
        "frames sent to the replaced session: {stray:?}"
    );
    let again = second.next_dispatch().await;
    assert_eq!(again.id, id);
    second.event(&id, 1, finished(Outcome::Ok, "")).await;
    let (_, job) = rest.get(&format!("/v1/jobs/{id}?wait=5")).await;
    assert_eq!(job["state"], "succeeded");
    assert_eq!(job["attempts"], 2);

    server.shutdown().await;
}

async fn delete(rest: &Rest, path: &str) -> (u16, Value) {
    Rest::json(rest.request("DELETE", path)).await
}

#[tokio::test]
async fn cancel_of_a_queued_job_and_of_a_running_job() {
    let db = TestDb::new();
    let server = start(&db, |_| {}).await;
    let rest = Rest::new(&server);
    let node = unique_node();
    let submit = || async {
        let (status, body) = rest
            .post(
                &format!("/v1/nodes/{node}/jobs"),
                &json!({ "run_commands": { "commands": [{ "command": "sleep 60" }] } }),
            )
            .await;
        assert_eq!(status, 202, "{body}");
        body["id"].as_str().unwrap().to_owned()
    };

    // Queued: cancelled at once, with a terminal KO event, never dispatched.
    let queued = submit().await;
    let (status, job) = delete(&rest, &format!("/v1/jobs/{queued}")).await;
    assert_eq!(status, 200, "{job}");
    assert_eq!(job["state"], "cancelled");
    let (_, log) = rest.get(&format!("/api/log/{queued}")).await;
    assert_eq!(codes(&log), [0, 1]);
    let last: Value = serde_json::from_str(log["data"][1]["data"].as_str().unwrap()).unwrap();
    assert_eq!(last["message"], "cancelled");
    assert_eq!(delete(&rest, &format!("/v1/jobs/{queued}")).await.0, 409);
    assert_eq!(delete(&rest, "/v1/jobs/unknown").await.0, 404);

    let mut agent = FakeAgent::connect(server.agent_addr, &node, None)
        .await
        .unwrap();
    assert!(agent.next_frame(Duration::from_millis(700)).await.is_none());

    // On the agent: flagged, and the agent receives the Cancel frame.
    let running = submit().await;
    assert_eq!(agent.next_dispatch().await.id, running);
    agent.event(&running, 1, Body::Started(JobStarted {})).await;
    let (status, job) = delete(&rest, &format!("/v1/jobs/{running}")).await;
    assert_eq!(status, 202, "{job}");
    assert_eq!(job["state"], "running");
    assert_eq!(job["cancel_requested"], true);
    assert_eq!(agent.next_cancel().await, running);
    // Asking again does not send a second frame.
    assert_eq!(delete(&rest, &format!("/v1/jobs/{running}")).await.0, 202);
    assert!(agent.next_frame(Duration::from_millis(700)).await.is_none());

    agent
        .event(&running, 2, finished(Outcome::Cancelled, ""))
        .await;
    let (_, job) = rest.get(&format!("/v1/jobs/{running}?wait=5")).await;
    assert_eq!(job["state"], "failed");
    assert_eq!(job["events"][2]["data"]["outcome"], "cancelled");
    assert_eq!(delete(&rest, &format!("/v1/jobs/{running}")).await.0, 409);

    server.shutdown().await;
}

#[tokio::test]
async fn cancel_reaches_an_agent_held_by_another_replica() {
    let db = TestDb::new();
    // Slow sweep on the owner: the peer wake-up carries the cancellation.
    let owner = start(&db, |c| c.sweep_interval_ms = 5000).await;
    let other = start(&db, |_| {}).await;
    let node = unique_node();
    let mut agent = FakeAgent::connect(owner.agent_addr, &node, None)
        .await
        .unwrap();
    let (_, body) = Rest::new(&owner)
        .post(
            &format!("/v1/nodes/{node}/jobs"),
            &json!({ "run_commands": { "commands": [{ "command": "sleep 60" }] } }),
        )
        .await;
    let id = body["id"].as_str().unwrap().to_owned();
    assert_eq!(agent.next_dispatch().await.id, id);

    let asked = Instant::now();
    let (status, _) = delete(&Rest::new(&other), &format!("/v1/jobs/{id}")).await;
    assert_eq!(status, 202);
    assert_eq!(agent.next_cancel().await, id);
    assert!(asked.elapsed() < Duration::from_secs(2));

    owner.shutdown().await;
    other.shutdown().await;
}

#[tokio::test]
async fn peer_token_is_required_when_configured() {
    let db = TestDb::new();
    let secure = |c: &mut Config| c.peer_token = Some("peer-test-token".into());
    let owner = start(&db, |c| {
        secure(c);
        c.sweep_interval_ms = 5000;
    })
    .await;
    let other = start(&db, secure).await;

    // A wake-up without the shared secret, or with a wrong one, is refused.
    let mut peer = PeerClient::connect(format!("http://{}", owner.peer_addr))
        .await
        .unwrap();
    let wake = |token: Option<&str>| {
        let mut request = Request::new(WakeRequest {
            node_id: "nobody".into(),
        });
        authorize(&mut request, token);
        request
    };
    let refused = peer.wake(wake(None)).await.unwrap_err();
    assert_eq!(refused.code(), Code::Unauthenticated);
    let refused = peer.wake(wake(Some("wrong"))).await.unwrap_err();
    assert_eq!(refused.code(), Code::Unauthenticated);
    let reply = peer.wake(wake(Some("peer-test-token"))).await.unwrap();
    assert!(!reply.into_inner().held);

    // The replicas present it to each other: cross-replica delivery works.
    let node = unique_node();
    let mut agent = FakeAgent::connect(owner.agent_addr, &node, None)
        .await
        .unwrap();
    tokio::time::sleep(Duration::from_millis(300)).await;
    let rest = Rest::new(&other);
    let submitted = Instant::now();
    let (_, body) = rest
        .post(
            &format!("/v1/nodes/{node}/jobs"),
            &json!({ "engine_commands": { "commands": ["[1] X"] } }),
        )
        .await;
    assert_eq!(agent.next_dispatch().await.id, body["id"].as_str().unwrap());
    assert!(submitted.elapsed() < Duration::from_secs(2));

    // Both replicas answer with the server id persisted in the store.
    let (_, a) = Rest::new(&owner).get("/api/internal/thumbprint").await;
    let (_, b) = rest.get("/api/internal/thumbprint").await;
    assert_eq!(a["data"]["thumbprint"], b["data"]["thumbprint"]);
    assert_eq!(a["data"]["thumbprint"].as_str().unwrap().len(), 32);

    owner.shutdown().await;
    other.shutdown().await;
}

#[tokio::test]
async fn centcore_commands_reach_the_agent() {
    let db = TestDb::new();
    let dir = tempfile::tempdir().unwrap();
    let cmd_file = dir.path().join("centcore.cmd");
    let server = start(&db, |c| {
        c.centcore.enabled = true;
        c.centcore.poll_interval_ms = 100;
        c.centcore.cmd_file = cmd_file.clone();
        c.centcore.cmd_dir = dir.path().join("centcore");
        c.centcore.cache_dir = dir.path().join("cache");
    })
    .await;
    let node = unique_node();
    let mut agent = FakeAgent::connect(server.agent_addr, &node, None)
        .await
        .unwrap();

    // Written aside then renamed, like a writer that appends atomically.
    let staged = dir.path().join("staged");
    std::fs::write(
        &staged,
        format!("EXTERNALCMD:{node}:[1] PROCESS_SERVICE_CHECK_RESULT;h;s;0;ok\nRELOAD:{node}\n"),
    )
    .unwrap();
    std::fs::rename(&staged, &cmd_file).unwrap();

    // A lease left by an interrupted run on a shared database delays the start.
    let mut first = None;
    for _ in 0..4 {
        if let Some(Frame::Dispatch(dispatch)) = agent.next_frame(STEP).await {
            first = dispatch.job;
            break;
        }
    }
    let Some(Kind::EngineCommands(engine)) = first.expect("dispatch").action.unwrap().kind else {
        panic!("expected EngineCommands");
    };
    assert_eq!(
        engine.commands,
        ["[1] PROCESS_SERVICE_CHECK_RESULT;h;s;0;ok"]
    );
    let Some(Kind::ServiceControl(control)) = agent.next_dispatch().await.action.unwrap().kind
    else {
        panic!("expected ServiceControl");
    };
    assert_eq!((control.service.as_str(), control.operation), ("engine", 3));
    assert!(!cmd_file.exists());

    server.shutdown().await;
}

/// Test certificate authority.
struct Pki {
    ca: rcgen::CertifiedIssuer<'static, rcgen::KeyPair>,
    dir: tempfile::TempDir,
}

impl Pki {
    fn new() -> Self {
        let mut params = rcgen::CertificateParams::new(Vec::<String>::new()).unwrap();
        params.is_ca = rcgen::IsCa::Ca(rcgen::BasicConstraints::Unconstrained);
        params
            .distinguished_name
            .push(rcgen::DnType::CommonName, "gorgone test ca");
        params.key_usages = vec![
            rcgen::KeyUsagePurpose::KeyCertSign,
            rcgen::KeyUsagePurpose::DigitalSignature,
        ];
        let key = rcgen::KeyPair::generate().unwrap();
        let ca = rcgen::CertifiedIssuer::self_signed(params, key).unwrap();
        let dir = tempfile::tempdir().unwrap();
        std::fs::write(dir.path().join("ca.pem"), ca.pem()).unwrap();
        Self { ca, dir }
    }

    /// Issues a certificate; returns `(certificate, key)` in PEM.
    fn issue(&self, common_name: &str, sans: &[&str]) -> (String, String) {
        let sans: Vec<String> = sans.iter().map(|s| (*s).to_owned()).collect();
        let mut params = rcgen::CertificateParams::new(sans).unwrap();
        params
            .distinguished_name
            .push(rcgen::DnType::CommonName, common_name);
        params.extended_key_usages = vec![
            rcgen::ExtendedKeyUsagePurpose::ServerAuth,
            rcgen::ExtendedKeyUsagePurpose::ClientAuth,
        ];
        let key = rcgen::KeyPair::generate().unwrap();
        let cert = params.signed_by(&key, &self.ca).unwrap();
        (cert.pem(), key.serialize_pem())
    }

    /// Server TLS settings with a certificate for `localhost`.
    fn server(&self, name: &str, mtls: bool) -> gorgone_server::config::TlsConfig {
        let (cert, key) = self.issue("gorgone-server", &["localhost", "127.0.0.1"]);
        let cert_path = self.dir.path().join(format!("{name}.pem"));
        let key_path = self.dir.path().join(format!("{name}.key"));
        std::fs::write(&cert_path, cert).unwrap();
        std::fs::write(&key_path, key).unwrap();
        gorgone_server::config::TlsConfig {
            cert: cert_path,
            key: key_path,
            client_ca: mtls.then(|| self.dir.path().join("ca.pem")),
        }
    }

    /// TLS client settings trusting the test CA, with a client certificate
    /// for `common_name` if any.
    fn rustls_client(&self, common_name: Option<&str>) -> rustls::ClientConfig {
        use rustls::pki_types::pem::PemObject;
        use rustls::pki_types::{CertificateDer, PrivateKeyDer};
        let mut roots = rustls::RootCertStore::empty();
        roots
            .add(CertificateDer::from_pem_slice(self.ca.pem().as_bytes()).unwrap())
            .unwrap();
        let builder = rustls::ClientConfig::builder().with_root_certificates(roots);
        match common_name {
            Some(common_name) => {
                let (cert, key) = self.issue(common_name, &[]);
                builder
                    .with_client_auth_cert(
                        vec![CertificateDer::from_pem_slice(cert.as_bytes()).unwrap()],
                        PrivateKeyDer::from_pem_slice(key.as_bytes()).unwrap(),
                    )
                    .unwrap()
            }
            None => builder.with_no_client_auth(),
        }
    }

    /// Opens a TLS channel, with the client certificate of `common_name` if any.
    async fn channel(
        &self,
        addr: SocketAddr,
        common_name: Option<&str>,
    ) -> Result<Channel, String> {
        let mut tls = ClientTlsConfig::new()
            .ca_certificate(Certificate::from_pem(self.ca.pem()))
            .domain_name("localhost");
        if let Some(common_name) = common_name {
            let (cert, key) = self.issue(common_name, &[]);
            tls = tls.identity(Identity::from_pem(cert, key));
        }
        Channel::from_shared(format!("https://{addr}"))
            .unwrap()
            .tls_config(tls)
            .unwrap()
            .connect()
            .await
            .map_err(|err| format!("{err:?}"))
    }

    /// Tries a whole session opening; the error is the gRPC code, or the
    /// transport failure.
    async fn session(
        &self,
        addr: SocketAddr,
        common_name: Option<&str>,
        node_id: &str,
        token: Option<&str>,
    ) -> Result<FakeAgent, String> {
        let channel = self.channel(addr, common_name).await?;
        FakeAgent::connect_channel(channel, node_id, token)
            .await
            .map_err(|status| format!("{:?}", status.code()))
    }
}

#[tokio::test]
async fn agent_gateway_over_tls() {
    let pki = Pki::new();
    let db = TestDb::new();
    let server = start(&db, |c| c.agent_tls = Some(pki.server("agent", false))).await;
    let rest = Rest::new(&server);
    let node = unique_node();

    let mut agent = pki
        .session(server.agent_addr, None, &node, None)
        .await
        .unwrap();
    let (_, body) = rest
        .post(
            &format!("/api/nodes/{node}/core/action/command"),
            &json!([{ "command": "echo ok" }]),
        )
        .await;
    let id = body["token"].as_str().unwrap().to_owned();
    assert_eq!(agent.next_dispatch().await.id, id);
    agent.event(&id, 1, finished(Outcome::Ok, "")).await;
    let (_, job) = rest.get(&format!("/v1/jobs/{id}?wait=5")).await;
    assert_eq!(job["state"], "succeeded");

    // A plaintext client cannot open a session on the TLS listener.
    let plain = async {
        let channel = Channel::from_shared(format!("http://{}", server.agent_addr))
            .unwrap()
            .connect()
            .await
            .map_err(|err| err.to_string())?;
        FakeAgent::connect_channel(channel, &node, None)
            .await
            .map(|_| ())
            .map_err(|status| status.to_string())
    };
    let refused = tokio::time::timeout(STEP, plain).await;
    assert!(!matches!(refused, Ok(Ok(()))), "plaintext must not work");

    server.shutdown().await;
}

#[tokio::test]
async fn mtls_admits_trusted_agents_and_the_token_identifies_the_node() {
    let pki = Pki::new();
    let db = TestDb::new();
    let server = start(&db, |c| {
        c.auto_enroll = false;
        c.agent_tls = Some(pki.server("agent", true));
    })
    .await;
    let rest = Rest::new(&server);
    let node = unique_node();
    let (status, enrolled) = Rest::json(rest.request("PUT", &format!("/v1/nodes/{node}"))).await;
    assert_eq!(status, 200);
    let token = enrolled["token"].as_str().unwrap().to_owned();

    // A certificate alone identifies nobody, even named after the node.
    for bad in [None, Some("wrong-token")] {
        let refused = pki
            .session(server.agent_addr, Some(&node), &node, bad)
            .await
            .err()
            .unwrap();
        assert_eq!(refused, "Unauthenticated");
    }
    // A certificate never creates a node either.
    let ghost = unique_node();
    let refused = pki
        .session(server.agent_addr, Some(&ghost), &ghost, Some(&token))
        .await
        .err()
        .unwrap();
    assert_eq!(refused, "Unauthenticated");

    // Trusted certificate (whatever its common name) and the node's token.
    let mut agent = pki
        .session(server.agent_addr, Some("any-poller"), &node, Some(&token))
        .await
        .unwrap();
    let (_, body) = rest
        .post(
            &format!("/v1/nodes/{node}/jobs"),
            &json!({ "engine_commands": { "commands": ["[1] X"] } }),
        )
        .await;
    assert_eq!(agent.next_dispatch().await.id, body["id"].as_str().unwrap());
    // The same identity covers FetchBlob.
    let unknown = agent.fetch_blob(&"0".repeat(64)).await.unwrap_err();
    assert_eq!(unknown.code(), Code::NotFound);
    agent.token = None;
    let refused = agent.fetch_blob(&"0".repeat(64)).await.unwrap_err();
    assert_eq!(refused.code(), Code::Unauthenticated);

    // No client certificate: the handshake itself fails, token or not.
    let refused = tokio::time::timeout(
        STEP,
        pki.session(server.agent_addr, None, &node, Some(&token)),
    )
    .await;
    assert!(
        !matches!(refused, Ok(Ok(_))),
        "a client certificate is required"
    );

    server.shutdown().await;
}

#[tokio::test]
async fn rest_api_over_tls() {
    let pki = Pki::new();
    let db = TestDb::new();
    let server = start(&db, |c| {
        c.rest_tls = Some(pki.server("rest", false));
        c.rest.token = Some("rest-test-token".into());
        // The client address is known behind TLS too.
        c.rest.allowed_cidrs = vec!["127.0.0.0/8".parse().unwrap()];
    })
    .await;
    let mut rest = Rest::new(&server);
    rest.bearer = Some("rest-test-token".into());
    let connector = tokio_rustls::TlsConnector::from(std::sync::Arc::new(pki.rustls_client(None)));
    let addr = server.rest_addr;
    let tls = || async {
        let stream = TcpStream::connect(addr).await?;
        connector
            .connect("localhost".try_into().unwrap(), stream)
            .await
    };

    let healthz = rest.request("GET", "/healthz");
    assert_eq!(healthz.send_on(tls().await.unwrap()).await.unwrap().0, 200);
    let node = unique_node();
    let submit = rest
        .request("POST", &format!("/v1/nodes/{node}/jobs"))
        .json(&run_commands("echo ok"));
    let (status, body) = Rest::decode(submit.send_on(tls().await.unwrap()).await.unwrap());
    assert_eq!(status, 202, "{body}");
    let id = body["id"].as_str().unwrap();
    let job = rest.request("GET", &format!("/v1/jobs/{id}"));
    let (status, job) = Rest::decode(job.send_on(tls().await.unwrap()).await.unwrap());
    assert_eq!(status, 200);
    assert_eq!(job["state"], "queued");
    rest.bearer = None;
    let anonymous = rest.request("GET", "/v1/nodes");
    assert_eq!(
        anonymous.send_on(tls().await.unwrap()).await.unwrap().0,
        401
    );

    // A client that never finishes its handshake does not hold the others.
    let _stalled = TcpStream::connect(server.rest_addr).await.unwrap();
    let healthz = rest.request("GET", "/healthz");
    assert_eq!(healthz.send_on(tls().await.unwrap()).await.unwrap().0, 200);

    // Plaintext on the TLS listener gets no HTTP answer.
    assert!(rest.get_plain("/healthz").await.is_err());

    // The listener stops with the server.
    server.shutdown().await;
    assert!(tls().await.is_err());
}

#[tokio::test]
async fn peer_wake_up_over_tls() {
    let pki = Pki::new();
    let db = TestDb::new();
    let secure = |c: &mut Config, name: &str| {
        // mTLS between replicas, on top of the shared secret.
        c.peer_tls = Some(pki.server(name, true));
        c.peer_ca = Some(pki.dir.path().join("ca.pem"));
        c.peer_token = Some("peer-test-token".into());
    };
    let owner = start(&db, |c| {
        secure(c, "owner");
        c.sweep_interval_ms = 5000;
    })
    .await;
    let other = start(&db, |c| secure(c, "other")).await;
    let node = unique_node();
    let mut agent = FakeAgent::connect(owner.agent_addr, &node, None)
        .await
        .unwrap();
    tokio::time::sleep(Duration::from_millis(300)).await;

    let submitted = Instant::now();
    let (_, body) = Rest::new(&other)
        .post(
            &format!("/v1/nodes/{node}/jobs"),
            &json!({ "engine_commands": { "commands": ["[1] X"] } }),
        )
        .await;
    assert_eq!(agent.next_dispatch().await.id, body["id"].as_str().unwrap());
    assert!(
        submitted.elapsed() < Duration::from_secs(2),
        "delivered by the sweep, not by the TLS wake-up"
    );

    // A plaintext caller cannot reach the peer service.
    let plain = async {
        let mut peer = PeerClient::connect(format!("http://{}", owner.peer_addr))
            .await
            .map_err(|err| err.to_string())?;
        let mut request = Request::new(WakeRequest {
            node_id: node.clone(),
        });
        authorize(&mut request, Some("peer-test-token"));
        peer.wake(request)
            .await
            .map(|_| ())
            .map_err(|s| s.to_string())
    };
    let refused = tokio::time::timeout(STEP, plain).await;
    assert!(!matches!(refused, Ok(Ok(()))), "plaintext must not work");

    owner.shutdown().await;
    other.shutdown().await;
}

fn run_commands(command: &str) -> Value {
    json!({ "run_commands": { "commands": [{ "command": command }] } })
}

async fn submit(rest: &Rest, node: &str, action: &Value) -> String {
    let (status, body) = rest.post(&format!("/v1/nodes/{node}/jobs"), action).await;
    assert_eq!(status, 202, "{body}");
    body["id"].as_str().unwrap().to_owned()
}

async fn plain_channel(addr: SocketAddr) -> Channel {
    Channel::from_shared(format!("http://{addr}"))
        .unwrap()
        .connect()
        .await
        .unwrap()
}

async fn connected(rest: &Rest, node: &str) -> bool {
    let (_, nodes) = rest.get("/v1/nodes").await;
    nodes["nodes"]
        .as_array()
        .unwrap()
        .iter()
        .any(|n| n["id"] == node && n["connected"] == true)
}

#[tokio::test]
async fn job_lost_with_its_session_is_delivered_before_later_jobs() {
    let db = TestDb::new();
    let server = start(&db, |_| {}).await;
    let rest = Rest::new(&server);
    let node = unique_node();
    let mut first = FakeAgent::connect(server.agent_addr, &node, None)
        .await
        .unwrap();

    // An ApplyConfig is claimed into a session that dies without a word.
    let src = tempfile::tempdir().unwrap();
    std::fs::write(src.path().join("centengine.cfg"), "a=1\n").unwrap();
    let bundle = gorgone_core::bundle::pack(src.path()).unwrap();
    let (status, body) = Rest::json(
        rest.request("PUT", &format!("/v1/nodes/{node}/config/engine"))
            .body(bundle),
    )
    .await;
    assert_eq!(status, 202, "{body}");
    let apply = body["id"].as_str().unwrap().to_owned();
    assert_eq!(first.next_dispatch().await.id, apply);

    // The agent reconnects and a reload is submitted right after: the
    // configuration must still arrive first.
    let mut second = FakeAgent::connect(server.agent_addr, &node, None)
        .await
        .unwrap();
    let reload = submit(
        &rest,
        &node,
        &json!({ "service_control": { "service": "engine", "operation": "reload" } }),
    )
    .await;
    assert_eq!(second.next_dispatch().await.id, apply);
    assert_eq!(second.next_dispatch().await.id, reload);

    server.shutdown().await;
}

#[tokio::test]
async fn dispatch_follows_the_submission_order_of_the_store() {
    let db = TestDb::new();
    let (a, b) = (start(&db, |_| {}).await, start(&db, |_| {}).await);
    let node = unique_node();
    let (rest_a, rest_b) = (Rest::new(&a), Rest::new(&b));

    // Submits interleaved on two replicas while the node is away.
    let submits = (0..30).map(|i| {
        let rest = if i % 2 == 0 { &rest_a } else { &rest_b };
        let node = &node;
        async move { submit(rest, node, &run_commands(&format!("echo {i}"))).await }
    });
    let ids = futures_util::future::join_all(submits).await;
    assert_eq!(ids.len(), 30);

    let mut agent = FakeAgent::connect(a.agent_addr, &node, None).await.unwrap();
    let mut ranks = Vec::new();
    for _ in 0..30 {
        let job = agent.next_dispatch().await;
        let (_, stored) = rest_b.get(&format!("/v1/jobs/{}", job.id)).await;
        ranks.push(stored["seq"].as_i64().unwrap());
        assert!(job.ttl_ms > 0 && job.ttl_ms <= 300_000, "{}", job.ttl_ms);
    }
    // Received in the order the store committed them, without gap.
    assert_eq!(ranks, (1..=30).collect::<Vec<i64>>());

    a.shutdown().await;
    b.shutdown().await;
}

#[tokio::test]
async fn stalled_agent_blocks_neither_submits_nor_other_nodes() {
    let db = TestDb::new();
    let server = start(&db, |c| c.session_send_timeout_ms = 500).await;
    let rest = Rest::new(&server);
    let (stalled_node, healthy_node) = (unique_node(), unique_node());
    // Connected, then never reads its session again. Small flow-control
    // windows: the stall shows after little data, which keeps the test light
    // for the database.
    let narrow = Channel::from_shared(format!("http://{}", server.agent_addr))
        .unwrap()
        .initial_stream_window_size(65_535)
        .initial_connection_window_size(65_535)
        .connect()
        .await
        .unwrap();
    let stalled = FakeAgent::connect_channel(narrow, &stalled_node, None)
        .await
        .unwrap();
    let mut healthy = FakeAgent::connect(server.agent_addr, &healthy_node, None)
        .await
        .unwrap();

    // More than the windows, the HTTP/2 send buffer and the session buffer
    // (256 frames) can hold.
    const JOBS: usize = 600;
    let big = run_commands(&"x".repeat(8 << 10));
    for _ in 0..JOBS {
        let asked = Instant::now();
        submit(&rest, &stalled_node, &big).await;
        assert!(asked.elapsed() < Duration::from_secs(2), "submit blocked");
    }
    // Another node is served meanwhile.
    let id = submit(&rest, &healthy_node, &run_commands("echo ok")).await;
    assert_eq!(healthy.next_dispatch().await.id, id);

    // The stalled session ends up closed, and its jobs are not lost.
    let closed = async {
        while connected(&rest, &stalled_node).await {
            tokio::time::sleep(Duration::from_millis(100)).await;
        }
    };
    tokio::time::timeout(Duration::from_secs(15), closed)
        .await
        .expect("stalled session closed");
    drop(stalled);
    let mut back = FakeAgent::connect(server.agent_addr, &stalled_node, None)
        .await
        .unwrap();
    let mut received = 0;
    while received < JOBS {
        back.next_dispatch().await;
        received += 1;
    }

    server.shutdown().await;
}

#[tokio::test]
async fn cancel_is_sent_again_to_a_new_session() {
    let db = TestDb::new();
    let server = start(&db, |_| {}).await;
    let rest = Rest::new(&server);
    let node = unique_node();
    let mut first = FakeAgent::connect(server.agent_addr, &node, None)
        .await
        .unwrap();
    let id = submit(&rest, &node, &run_commands("sleep 60")).await;
    assert_eq!(first.next_dispatch().await.id, id);
    first.event(&id, 1, Body::Started(JobStarted {})).await;
    assert_eq!(delete(&rest, &format!("/v1/jobs/{id}")).await.0, 202);
    assert_eq!(first.next_cancel().await, id);

    // The agent reconnects with the job still running: it is told again.
    let mut second = FakeAgent::connect(server.agent_addr, &node, None)
        .await
        .unwrap();
    assert_eq!(second.next_cancel().await, id);
    second.event(&id, 2, finished(Outcome::Cancelled, "")).await;
    let (_, job) = rest.get(&format!("/v1/jobs/{id}?wait=5")).await;
    assert_eq!(job["state"], "failed");

    server.shutdown().await;
}

#[tokio::test]
async fn re_enrolment_ends_the_session_held_by_another_replica() {
    let db = TestDb::new();
    let strict = |c: &mut Config| c.auto_enroll = false;
    let (holder, other) = (start(&db, strict).await, start(&db, strict).await);
    let node = unique_node();
    let enrol = |server: &ServerHandle| {
        let rest = Rest::new(server);
        let path = format!("/v1/nodes/{node}");
        async move { Rest::json(rest.request("PUT", &path)).await }
    };
    let (_, enrolled) = enrol(&holder).await;
    let token = enrolled["token"].as_str().unwrap().to_owned();
    let mut agent = FakeAgent::connect(holder.agent_addr, &node, Some(&token))
        .await
        .unwrap();
    assert!(connected(&Rest::new(&other), &node).await);

    // Rotated on the other replica: the session row goes away, and the
    // holder closes the session at the next sign of life.
    let (status, rotated) = enrol(&other).await;
    assert_eq!(status, 200);
    assert_ne!(rotated["token"], token.as_str());
    assert!(!connected(&Rest::new(&other), &node).await);
    agent
        .send(AgentFrameKind::Heartbeat(Heartbeat {
            running_jobs: 0,
            ..Heartbeat::default()
        }))
        .await;
    let ended = async { while agent.inbound.message().await.is_ok_and(|f| f.is_some()) {} };
    tokio::time::timeout(STEP, ended)
        .await
        .expect("session closed");
    let refused = FakeAgent::connect(holder.agent_addr, &node, Some(&token)).await;
    assert_eq!(refused.err().unwrap().code(), Code::Unauthenticated);

    holder.shutdown().await;
    other.shutdown().await;
}

#[tokio::test]
async fn session_is_authenticated_from_metadata_before_any_frame() {
    let db = TestDb::new();
    let server = start(&db, |_| {}).await;
    let node = unique_node();

    // No node id in the metadata: refused whatever the Hello says.
    let channel = plain_channel(server.agent_addr).await;
    let refused = FakeAgent::open(channel, None, &node, None).await;
    assert_eq!(refused.err().unwrap().code(), Code::Unauthenticated);

    // A Hello naming another node than the authenticated one.
    let channel = plain_channel(server.agent_addr).await;
    let refused = FakeAgent::open(channel, Some(&node), &unique_node(), None).await;
    assert_eq!(refused.err().unwrap().code(), Code::PermissionDenied);

    // FetchBlob without node id.
    let mut client = AgentGatewayClient::new(plain_channel(server.agent_addr).await);
    let refused = client
        .fetch_blob(Request::new(FetchBlobRequest {
            digest: "0".repeat(64),
        }))
        .await
        .unwrap_err();
    assert_eq!(refused.code(), Code::Unauthenticated);

    server.shutdown().await;
}

#[tokio::test]
async fn agent_events_are_bound_to_the_jobs_of_their_node() {
    let db = TestDb::new();
    let server = start(&db, |_| {}).await;
    let rest = Rest::new(&server);
    let (node, intruder_node) = (unique_node(), unique_node());
    let mut agent = FakeAgent::connect(server.agent_addr, &node, None)
        .await
        .unwrap();
    let mut intruder = FakeAgent::connect(server.agent_addr, &intruder_node, None)
        .await
        .unwrap();
    let events = |id: &str| {
        let path = format!("/v1/jobs/{id}");
        let rest = &rest;
        async move { rest.get(&path).await.1 }
    };

    let id = submit(&rest, &node, &run_commands("echo ok")).await;
    assert_eq!(agent.next_dispatch().await.id, id);
    // Another node cannot write into the job (acknowledged, not stored).
    intruder
        .event(&id, 1, finished(Outcome::Ok, "forged"))
        .await;
    let job = events(&id).await;
    assert_eq!(job["state"], "dispatched");
    assert_eq!(job["events"].as_array().unwrap().len(), 1);

    // Nothing is accepted after the terminal event.
    agent.event(&id, 1, finished(Outcome::Failed, "real")).await;
    agent.event(&id, 2, command_result(0, "late")).await;
    agent.event(&id, 3, finished(Outcome::Ok, "late")).await;
    let job = events(&id).await;
    assert_eq!(job["state"], "failed");
    assert_eq!(job["events"].as_array().unwrap().len(), 2);

    // A job that was never dispatched takes no event either.
    let offline = unique_node();
    let queued = submit(&rest, &offline, &run_commands("echo ok")).await;
    let mut late = FakeAgent::connect(server.agent_addr, &offline, None)
        .await
        .unwrap();
    assert_eq!(late.next_dispatch().await.id, queued);
    late.event(&queued, 1, finished(Outcome::Ok, "")).await;
    assert_eq!(events(&queued).await["state"], "succeeded");

    // Oversized output is truncated, not stored whole.
    let noisy = submit(&rest, &node, &run_commands("yes")).await;
    assert_eq!(agent.next_dispatch().await.id, noisy);
    agent
        .event(&noisy, 1, command_result(0, &"y".repeat(3 << 20)))
        .await;
    let job = events(&noisy).await;
    let stored = &job["events"][1]["data"];
    assert_eq!(stored["truncated"], true);
    assert!(stored["stdout"].as_str().unwrap().len() < 300 << 10);

    // Past the event cap the job is failed by the server.
    agent.event(&noisy, 10_001, command_result(0, "x")).await;
    let job = events(&noisy).await;
    assert_eq!(job["state"], "failed");
    let last = job["events"].as_array().unwrap().last().unwrap().clone();
    assert_eq!(last["code"], 1);
    assert!(
        last["data"]["message"]
            .as_str()
            .unwrap()
            .contains("more than 10000 events")
    );

    server.shutdown().await;
}

#[tokio::test]
async fn blob_is_served_only_to_the_node_whose_pending_job_needs_it() {
    let db = TestDb::new();
    let server = start(&db, |_| {}).await;
    let rest = Rest::new(&server);
    let (node, other_node) = (unique_node(), unique_node());
    let mut agent = FakeAgent::connect(server.agent_addr, &node, None)
        .await
        .unwrap();
    let mut other = FakeAgent::connect(server.agent_addr, &other_node, None)
        .await
        .unwrap();

    let src = tempfile::tempdir().unwrap();
    std::fs::write(src.path().join("secret.cfg"), unique_node()).unwrap();
    let bundle = gorgone_core::bundle::pack(src.path()).unwrap();
    let (status, body) = Rest::json(
        rest.request("PUT", &format!("/v1/nodes/{node}/config/engine"))
            .body(bundle.clone()),
    )
    .await;
    assert_eq!(status, 202, "{body}");
    let digest = body["digest"].as_str().unwrap().to_owned();
    let id = agent.next_dispatch().await.id;

    // Another node gets the same answer as for a blob that does not exist.
    let refused = other.fetch_blob(&digest).await.unwrap_err();
    assert_eq!(refused.code(), Code::NotFound);
    assert_eq!(agent.fetch_blob(&digest).await.unwrap().concat(), bundle);

    // Once the job is finished the blob is no longer handed out.
    agent.event(&id, 1, finished(Outcome::Ok, "")).await;
    let refused = agent.fetch_blob(&digest).await.unwrap_err();
    assert_eq!(refused.code(), Code::NotFound);

    server.shutdown().await;
}

#[tokio::test]
async fn unknown_node_and_large_bodies_are_refused_on_rest() {
    let db = TestDb::new();
    let server = start(&db, |c| {
        c.auto_enroll = false;
        c.rest.token = Some("rest-test-token".into());
        c.metrics_public = true;
    })
    .await;
    let mut rest = Rest::new(&server);
    // Metrics were explicitly made public; the API was not.
    assert_eq!(rest.get("/metrics").await.0, 200);
    assert_eq!(rest.get("/v1/nodes").await.0, 401);
    rest.bearer = Some("rest-test-token".into());

    // Nodes must be enrolled to get jobs.
    let ghost = unique_node();
    let (status, body) = rest
        .post(&format!("/v1/nodes/{ghost}/jobs"), &run_commands("echo ok"))
        .await;
    assert_eq!(status, 404, "{body}");
    let (status, _) = rest
        .post(
            &format!("/api/nodes/{ghost}/core/action/command"),
            &json!([{ "command": "echo ok" }]),
        )
        .await;
    assert_eq!(status, 404);
    let (status, _) = Rest::json(rest.request("PUT", &format!("/v1/nodes/{ghost}"))).await;
    assert_eq!(status, 200);
    submit(&rest, &ghost, &run_commands("echo ok")).await;

    // Only the bundle upload accepts a large body. The server answers 413
    // without reading it and closes: the client sees the status, or its
    // write cut short.
    let oversized = rest
        .request("POST", &format!("/v1/nodes/{ghost}/jobs"))
        .json(&run_commands(&"x".repeat(2 << 20)))
        .send()
        .await;
    match oversized {
        Ok((status, _)) => assert_eq!(status, 413),
        Err(err) => assert!(
            matches!(
                err.kind(),
                std::io::ErrorKind::BrokenPipe | std::io::ErrorKind::ConnectionReset
            ),
            "{err:?}"
        ),
    }
    // A new client: the connection above is being closed by the server.
    let mut rest = Rest::new(&server);
    rest.bearer = Some("rest-test-token".into());
    let (status, _) = Rest::json(
        rest.request("PUT", &format!("/v1/nodes/{ghost}/config/engine"))
            .body(vec![0u8; 2 << 20]),
    )
    .await;
    // Read in full, then refused for what it is: not a bundle.
    assert_eq!(status, 400);

    server.shutdown().await;
}

#[tokio::test]
async fn pipelined_events_are_acknowledged_in_order_after_their_commit() {
    let db = TestDb::new();
    let server = start(&db, |_| {}).await;
    let rest = Rest::new(&server);
    let node = unique_node();
    let mut agent = FakeAgent::connect(server.agent_addr, &node, None)
        .await
        .unwrap();
    let jobs = [
        submit(&rest, &node, &run_commands("echo a")).await,
        submit(&rest, &node, &run_commands("echo b")).await,
    ];
    for id in &jobs {
        assert_eq!(&agent.next_dispatch().await.id, id);
    }

    // Sent without waiting for any acknowledgement: the server commits them
    // in batches. A duplicate and an event of an unknown job are in the flow.
    let mut sent = Vec::new();
    for seq in 1..=40u64 {
        for id in &jobs {
            sent.push((
                id.clone(),
                seq,
                command_result(seq as u32, &format!("line {seq}")),
            ));
        }
        if seq == 20 {
            sent.push((jobs[0].clone(), 20, command_result(20, "duplicate")));
            sent.push(("no-such-job".to_owned(), 1, command_result(0, "stray")));
        }
    }
    for id in &jobs {
        sent.push((id.clone(), 41, finished(Outcome::Ok, "")));
    }
    for (job_id, seq, body) in &sent {
        agent
            .send(AgentFrameKind::Event(JobEvent {
                job_id: job_id.clone(),
                seq: *seq,
                time_unix_ms: 1_700_000_000_000,
                body: Some(body.clone()),
            }))
            .await;
    }

    // One acknowledgement per event sent, in the same order.
    for (job_id, seq, _) in &sent {
        let ack = agent.next_ack().await;
        assert_eq!((&ack.job_id, ack.seq), (job_id, *seq));
        // Acknowledged means committed: the event is readable right away.
        if *seq == 41 {
            let (_, job) = rest.get(&format!("/v1/jobs/{job_id}")).await;
            assert_eq!(job["state"], "succeeded");
        }
    }
    for id in &jobs {
        let (_, job) = rest.get(&format!("/v1/jobs/{id}")).await;
        let events = job["events"].as_array().unwrap();
        // Accepted + 40 results + finished, each once and in order.
        assert_eq!(events.len(), 42);
        let seqs: Vec<i64> = events.iter().map(|e| e["seq"].as_i64().unwrap()).collect();
        assert_eq!(seqs, (0..=41).collect::<Vec<i64>>());
        assert_eq!(events[20]["data"]["stdout"], "line 20");
    }

    server.shutdown().await;
}

/// A small valid bundle whose content, hence digest, depends on `content`.
fn small_bundle(content: &str) -> Vec<u8> {
    let src = tempfile::tempdir().unwrap();
    std::fs::write(src.path().join("centengine.cfg"), content).unwrap();
    gorgone_core::bundle::pack(src.path()).unwrap()
}

/// `PUT /v1/nodes/{node}/config/{target}{query}` of a small bundle.
async fn push_config(
    rest: &Rest,
    node: &str,
    target: &str,
    query: &str,
    content: &str,
) -> (u16, Value) {
    let path = format!("/v1/nodes/{node}/config/{target}{query}");
    Rest::json(rest.request("PUT", &path).body(small_bundle(content))).await
}

/// Pushes a configuration and returns the id of its job.
async fn push(rest: &Rest, node: &str, target: &str) -> String {
    let (status, body) = push_config(rest, node, target, "", &unique_node()).await;
    assert_eq!(status, 202, "{body}");
    body["id"].as_str().unwrap().to_owned()
}

/// Engine restart, with extra top-level members of the submission.
fn restart(extra: Value) -> Value {
    let mut body = json!({ "service_control": { "service": "engine", "operation": "restart" } });
    for (key, value) in extra.as_object().unwrap() {
        body[key] = value.clone();
    }
    body
}

/// The job once it is terminal.
async fn finished_job(rest: &Rest, id: &str) -> Value {
    let (status, job) = rest.get(&format!("/v1/jobs/{id}?wait=10")).await;
    assert_eq!(status, 200, "{job}");
    job
}

/// Checks that the job was failed by the server on `prerequisite`.
async fn assert_failed_on(rest: &Rest, id: &str, prerequisite: &str, state: &str) {
    let job = finished_job(rest, id).await;
    assert_eq!(job["state"], "failed", "{job}");
    assert_eq!(job["attempts"], 0, "never dispatched: {job}");
    let events = job["events"].as_array().unwrap();
    assert_eq!(events.len(), 2, "{job}");
    assert_eq!(events[1]["code"], 1);
    assert_eq!(
        events[1]["data"],
        json!({ "message": format!("prerequisite {prerequisite} did not succeed ({state})") })
    );
}

async fn assert_nothing_received(agent: &mut FakeAgent) {
    let frame = agent.next_frame(Duration::from_millis(400)).await;
    assert!(frame.is_none(), "unexpected frame: {frame:?}");
}

#[tokio::test]
async fn job_is_dispatched_once_its_prerequisites_succeeded_and_order_is_kept() {
    let db = TestDb::new();
    // Slow sweep: only the commit of the prerequisites can resume the drain.
    let server = start(&db, |c| c.sweep_interval_ms = 5000).await;
    let rest = Rest::new(&server);
    let node = unique_node();
    let mut agent = FakeAgent::connect(server.agent_addr, &node, None)
        .await
        .unwrap();

    let engine = push(&rest, &node, "engine").await;
    let broker = push(&rest, &node, "broker").await;
    let restarted = submit(
        &rest,
        &node,
        &restart(json!({ "requires": [engine, broker] })),
    )
    .await;
    let later = submit(&rest, &node, &run_commands("echo later")).await;

    assert_eq!(agent.next_dispatch().await.id, engine);
    assert_eq!(agent.next_dispatch().await.id, broker);
    // The restart waits for both pushes, and what follows waits behind it.
    assert_nothing_received(&mut agent).await;
    agent.event(&engine, 1, finished(Outcome::Ok, "")).await;
    assert_nothing_received(&mut agent).await;
    let (_, job) = rest.get(&format!("/v1/jobs/{restarted}")).await;
    assert_eq!(job["state"], "queued");
    assert_eq!(job["requires"], json!([engine, broker]));
    let (_, job) = rest.get(&format!("/v1/jobs/{later}")).await;
    assert_eq!(
        (&job["state"], &job["requires"]),
        (&json!("queued"), &json!([]))
    );

    let resumed = Instant::now();
    agent.event(&broker, 1, finished(Outcome::Ok, "")).await;
    let job = agent.next_dispatch().await;
    assert_eq!(job.id, restarted);
    assert_eq!(job.requires, [engine.clone(), broker.clone()]);
    assert_eq!(agent.next_dispatch().await.id, later);
    assert!(
        resumed.elapsed() < Duration::from_secs(2),
        "resumed by the sweep, not by the commit: {:?}",
        resumed.elapsed()
    );

    server.shutdown().await;
}

#[tokio::test]
async fn job_whose_prerequisite_failed_is_failed_without_being_sent() {
    let db = TestDb::new();
    let server = start(&db, |c| c.sweep_interval_ms = 5000).await;
    let rest = Rest::new(&server);
    let node = unique_node();
    let mut agent = FakeAgent::connect(server.agent_addr, &node, None)
        .await
        .unwrap();

    let pushed = push(&rest, &node, "engine").await;
    let restarted = submit(&rest, &node, &restart(json!({ "requires": [pushed] }))).await;
    // Requires the restart: fails with it.
    let dependent = submit(&rest, &node, &restart(json!({ "requires": [restarted] }))).await;
    let later = submit(&rest, &node, &run_commands("echo later")).await;

    assert_eq!(agent.next_dispatch().await.id, pushed);
    agent
        .event(&pushed, 1, finished(Outcome::Failed, "bad bundle"))
        .await;
    // Later jobs still flow; the two restarts are never sent.
    assert_eq!(agent.next_dispatch().await.id, later);
    assert_failed_on(&rest, &restarted, &pushed, "failed").await;
    assert_failed_on(&rest, &dependent, &restarted, "failed").await;
    let (_, log) = rest.get(&format!("/api/log/{restarted}")).await;
    assert_eq!(codes(&log), [0, 1]);
    assert_nothing_received(&mut agent).await;
    let (_, metrics) = rest.get_plain("/metrics").await.unwrap();
    let failed: u64 = metrics
        .lines()
        .find_map(|line| line.strip_prefix("gorgone_jobs_prerequisite_failed_total "))
        .unwrap()
        .parse()
        .unwrap();
    assert!(failed >= 2, "{failed}");

    // A cancelled prerequisite counts as not succeeded, node away or not.
    let away = unique_node();
    let first = submit(&rest, &away, &run_commands("echo first")).await;
    let second = submit(&rest, &away, &restart(json!({ "requires": [first] }))).await;
    let (status, _) = delete(&rest, &format!("/v1/jobs/{first}")).await;
    assert_eq!(status, 200);
    let mut agent = FakeAgent::connect(server.agent_addr, &away, None)
        .await
        .unwrap();
    assert_failed_on(&rest, &second, &first, "cancelled").await;
    assert_nothing_received(&mut agent).await;

    server.shutdown().await;
}

#[tokio::test]
async fn prerequisites_must_be_jobs_of_the_same_node() {
    let db = TestDb::new();
    let server = start(&db, |_| {}).await;
    let rest = Rest::new(&server);
    let (node, other) = (unique_node(), unique_node());
    let foreign = submit(&rest, &other, &run_commands("echo other")).await;
    let own = submit(&rest, &node, &run_commands("echo own")).await;
    let jobs = format!("/v1/nodes/{node}/jobs");

    for requires in [
        json!(["no-such-job"]),
        json!([foreign]),
        json!([own, foreign]),
        json!((0..17).map(|i| format!("job-{i}")).collect::<Vec<_>>()),
    ] {
        let (status, body) = rest
            .post(&jobs, &restart(json!({ "requires": requires })))
            .await;
        assert_eq!(status, 400, "{requires}: {body}");
        assert_eq!(body["error"], "bad_request");
    }
    let (status, body) = rest.post(&jobs, &restart(json!({ "requires": "x" }))).await;
    assert!((400..500).contains(&status), "{body}");
    let query = format!("?requires={own},{foreign}");
    let (status, body) = push_config(&rest, &node, "engine", &query, "a=1\n").await;
    assert_eq!(status, 400, "{body}");

    // Accepted on the configuration route too, repeated ids counting once.
    let query = format!("?requires={own},{own}");
    let (status, body) = push_config(&rest, &node, "engine", &query, "a=1\n").await;
    assert_eq!(status, 202, "{body}");
    let (_, job) = rest
        .get(&format!("/v1/jobs/{}", body["id"].as_str().unwrap()))
        .await;
    assert_eq!(job["requires"], json!([own]));
    // Nothing was submitted by the refused requests.
    let mut agent = FakeAgent::connect(server.agent_addr, &node, None)
        .await
        .unwrap();
    assert_eq!(agent.next_dispatch().await.id, own);
    assert_nothing_received(&mut agent).await;

    server.shutdown().await;
}

#[tokio::test]
async fn prerequisite_is_enforced_for_a_job_submitted_on_another_replica() {
    let db = TestDb::new();
    let owner = start(&db, |c| c.sweep_interval_ms = 5000).await;
    let other = start(&db, |c| c.sweep_interval_ms = 5000).await;
    let node = unique_node();
    let mut agent = FakeAgent::connect(owner.agent_addr, &node, None)
        .await
        .unwrap();
    tokio::time::sleep(Duration::from_millis(300)).await;

    let rest = Rest::new(&other);
    let pushed = push(&rest, &node, "engine").await;
    let restarted = submit(&rest, &node, &restart(json!({ "requires": [pushed] }))).await;
    let later = submit(&rest, &node, &run_commands("echo later")).await;
    assert_eq!(agent.next_dispatch().await.id, pushed);
    assert_nothing_received(&mut agent).await;

    // The owner commits the result and resumes its drain by itself.
    let resumed = Instant::now();
    agent.event(&pushed, 1, finished(Outcome::Ok, "")).await;
    let job = agent.next_dispatch().await;
    assert_eq!(
        (job.id.as_str(), &job.requires),
        (restarted.as_str(), &vec![pushed])
    );
    assert_eq!(agent.next_dispatch().await.id, later);
    assert!(resumed.elapsed() < Duration::from_secs(2));

    owner.shutdown().await;
    other.shutdown().await;
}

#[tokio::test]
async fn resubmission_with_the_same_id_creates_nothing() {
    let db = TestDb::new();
    let server = start(&db, |_| {}).await;
    let rest = Rest::new(&server);
    let (node, other) = (unique_node(), unique_node());
    let mut agent = FakeAgent::connect(server.agent_addr, &node, None)
        .await
        .unwrap();
    let jobs = format!("/v1/nodes/{node}/jobs");
    let id = format!("restart_{node}");

    let (status, body) = rest.post(&jobs, &restart(json!({ "id": id }))).await;
    assert_eq!((status, &body), (202, &json!({ "id": id })));
    // The retry gets the same answer, whatever it carries.
    let again = json!({ "run_commands": { "commands": [{ "command": "echo no" }] }, "id": id });
    let (status, body) = rest.post(&jobs, &again).await;
    assert_eq!((status, &body), (202, &json!({ "id": id })));
    assert_eq!(agent.next_dispatch().await.id, id);
    assert_nothing_received(&mut agent).await;
    let (_, job) = rest.get(&format!("/v1/jobs/{id}")).await;
    assert_eq!(job["kind"], "service_control");
    assert_eq!(job["events"].as_array().unwrap().len(), 1);

    // The id belongs to its node.
    let (status, body) = rest
        .post(
            &format!("/v1/nodes/{other}/jobs"),
            &restart(json!({ "id": id })),
        )
        .await;
    assert_eq!(status, 409, "{body}");
    assert_eq!(body["error"], "conflict");
    let query = format!("?id={id}");
    let (status, body) = push_config(&rest, &other, "engine", &query, "a=1\n").await;
    assert_eq!(status, 409, "{body}");

    let too_long = "a".repeat(65);
    for bad in ["", "with space", "a/b", "a.b", too_long.as_str()] {
        let (status, body) = rest.post(&jobs, &restart(json!({ "id": bad }))).await;
        assert_eq!(status, 400, "{bad}: {body}");
    }
    let (status, _) = push_config(&rest, &node, "engine", "?id=a.b", "a=1\n").await;
    assert_eq!(status, 400);

    // Configuration push: the retry is told what the first one submitted.
    let query = format!("?id=push-{node}");
    let (status, first) = push_config(&rest, &node, "engine", &query, "a=1\n").await;
    assert_eq!(status, 202, "{first}");
    assert_eq!(first["id"], format!("push-{node}"));
    let (status, second) = push_config(&rest, &node, "engine", &query, "a=2\n").await;
    assert_eq!((status, &second), (202, &first));
    assert_eq!(agent.next_dispatch().await.id, format!("push-{node}"));
    assert_nothing_received(&mut agent).await;

    server.shutdown().await;
}

#[tokio::test]
async fn restart_requiring_the_latest_configuration_follows_the_last_push() {
    let db = TestDb::new();
    let server = start(&db, |c| c.sweep_interval_ms = 5000).await;
    let rest = Rest::new(&server);
    let node = unique_node();
    let mut agent = FakeAgent::connect(server.agent_addr, &node, None)
        .await
        .unwrap();
    let latest = || restart(json!({ "requires_latest_config": true }));
    let requires = |id: String| {
        let rest = &rest;
        async move { rest.get(&format!("/v1/jobs/{id}")).await.1["requires"].clone() }
    };

    // No push at all: nothing to wait for.
    let first = submit(&rest, &node, &latest()).await;
    assert_eq!(agent.next_dispatch().await.id, first);
    assert_eq!(requires(first.clone()).await, json!([]));
    agent.event(&first, 1, finished(Outcome::Ok, "")).await;

    // The last push failed: the restart is failed without being sent.
    let failed_push = push(&rest, &node, "engine").await;
    assert_eq!(agent.next_dispatch().await.id, failed_push);
    agent
        .event(&failed_push, 1, finished(Outcome::Failed, "bad bundle"))
        .await;
    let refused = submit(&rest, &node, &latest()).await;
    assert_eq!(requires(refused.clone()).await, json!([failed_push]));
    assert_failed_on(&rest, &refused, &failed_push, "failed").await;
    assert_nothing_received(&mut agent).await;

    // A newer push supersedes it; the restart is held until it succeeded.
    let good_push = push(&rest, &node, "engine").await;
    let held = submit(&rest, &node, &latest()).await;
    assert_eq!(requires(held.clone()).await, json!([good_push]));
    assert_eq!(agent.next_dispatch().await.id, good_push);
    assert_nothing_received(&mut agent).await;
    agent.event(&good_push, 1, finished(Outcome::Ok, "")).await;
    let job = agent.next_dispatch().await;
    assert_eq!(
        (job.id.as_str(), &job.requires),
        (held.as_str(), &vec![good_push.clone()])
    );
    agent.event(&held, 1, finished(Outcome::Ok, "")).await;

    // One push per target, joined to the explicit list without repetition.
    let broker_push = push(&rest, &node, "broker").await;
    assert_eq!(agent.next_dispatch().await.id, broker_push);
    agent
        .event(&broker_push, 1, finished(Outcome::Ok, ""))
        .await;
    let both = submit(
        &rest,
        &node,
        &restart(json!({ "requires_latest_config": true, "requires": [good_push, first] })),
    )
    .await;
    assert_eq!(
        requires(both.clone()).await,
        json!([good_push, first, broker_push])
    );
    assert_eq!(agent.next_dispatch().await.id, both);

    server.shutdown().await;
}

#[tokio::test]
async fn restart_may_name_the_configuration_targets_it_depends_on() {
    let db = TestDb::new();
    let server = start(&db, |c| c.sweep_interval_ms = 5000).await;
    let rest = Rest::new(&server);
    let node = unique_node();
    let mut agent = FakeAgent::connect(server.agent_addr, &node, None)
        .await
        .unwrap();
    let jobs = format!("/v1/nodes/{node}/jobs");
    let latest = |targets: Value| restart(json!({ "requires_latest_config": targets }));

    let mut pushes = Vec::new();
    for (target, outcome) in [
        ("engine", Outcome::Ok),
        ("broker", Outcome::Ok),
        ("vmware", Outcome::Rejected),
    ] {
        let id = push(&rest, &node, target).await;
        assert_eq!(agent.next_dispatch().await.id, id);
        agent.event(&id, 1, finished(outcome, "")).await;
        pushes.push(id);
    }
    let [engine, broker, vmware] = [&pushes[0], &pushes[1], &pushes[2]];

    // The rejected vmware push does not concern an engine restart.
    let named = submit(
        &rest,
        &node,
        &latest(json!(["engine", "broker", "never-pushed"])),
    )
    .await;
    let job = agent.next_dispatch().await;
    assert_eq!(job.id, named);
    assert_eq!(job.requires, [broker.clone(), engine.clone()]);
    // Every target: it does.
    let every = submit(&rest, &node, &latest(json!(true))).await;
    assert_failed_on(&rest, &every, vmware, "failed").await;
    // No target, either way of saying it.
    for none in [json!([]), json!(false)] {
        let id = submit(&rest, &node, &latest(none)).await;
        let job = agent.next_dispatch().await;
        assert_eq!((job.id, job.requires.len()), (id, 0));
    }

    // A failed engine push blocks the restart that lists it.
    let failed = push(&rest, &node, "engine").await;
    assert_eq!(agent.next_dispatch().await.id, failed);
    agent.event(&failed, 1, finished(Outcome::Failed, "")).await;
    let refused = submit(&rest, &node, &latest(json!(["engine", "broker"]))).await;
    assert_failed_on(&rest, &refused, &failed, "failed").await;
    assert_nothing_received(&mut agent).await;

    for bad in [
        json!((0..17).map(|i| format!("t{i}")).collect::<Vec<_>>()),
        json!(["x".repeat(192)]),
    ] {
        let (status, body) = rest.post(&jobs, &latest(bad)).await;
        assert_eq!(status, 400, "{body}");
    }
    let (status, body) = rest.post(&jobs, &latest(json!("engine"))).await;
    assert!((400..500).contains(&status), "{body}");

    server.shutdown().await;
}

#[tokio::test]
async fn node_state_reported_in_heartbeats_is_exposed_by_every_replica() {
    use gorgone_proto::agent::{AppliedConfig, EngineState};

    let db = TestDb::new();
    let (owner, other) = (start(&db, |_| {}).await, start(&db, |_| {}).await);
    let rest = Rest::new(&other);
    let node = unique_node();
    let path = format!("/v1/nodes/{node}");
    let (status, body) = rest.get(&path).await;
    assert_eq!(status, 404, "{body}");

    let agent = FakeAgent::connect(owner.agent_addr, &node, None)
        .await
        .unwrap();
    // Reads the node until `ready` holds.
    let node_when = |ready: fn(&Value) -> bool| {
        let (rest, path) = (&rest, &path);
        async move {
            let until = Instant::now() + STEP;
            loop {
                let (status, body) = rest.get(path).await;
                assert_eq!(status, 200, "{body}");
                if ready(&body) {
                    return body;
                }
                assert!(Instant::now() < until, "node state not updated: {body}");
                tokio::time::sleep(Duration::from_millis(20)).await;
            }
        }
    };
    let seen = node_when(|node| node["connected"] == true).await;
    assert_eq!(seen["applied"], json!({}));
    assert_eq!(seen["engine"], Value::Null);
    assert_eq!(seen["session"]["agent_version"], "fake-1.0");

    let digest = |seed: &str| seed.repeat(64);
    let heartbeat = |engine: Option<EngineState>| {
        AgentFrameKind::Heartbeat(Heartbeat {
            running_jobs: 0,
            applied: vec![
                AppliedConfig {
                    target: "engine".into(),
                    digest: digest("a"),
                },
                AppliedConfig {
                    target: "broker".into(),
                    digest: digest("b"),
                },
            ],
            engine,
        })
    };
    let running = Some(EngineState {
        reachable: true,
        version: "27.1.0".into(),
    });
    agent.send(heartbeat(running.clone())).await;
    let seen = node_when(|node| node["engine"] != Value::Null).await;
    assert_eq!(
        seen["applied"],
        json!({ "broker": digest("b"), "engine": digest("a") })
    );
    assert_eq!(
        seen["engine"],
        json!({ "reachable": true, "version": "27.1.0" })
    );
    assert_eq!(seen["id"], node.as_str());
    let changed_at = seen["state_changed_at"].as_i64().unwrap();

    // Same view in the list, on the replica holding the session too.
    for rest in [&rest, &Rest::new(&owner)] {
        let (_, nodes) = rest.get("/v1/nodes").await;
        let listed = nodes["nodes"]
            .as_array()
            .unwrap()
            .iter()
            .find(|n| n["id"] == node.as_str())
            .unwrap();
        assert_eq!(listed["applied"], seen["applied"]);
        assert_eq!(listed["engine"], seen["engine"]);
        assert_eq!(listed["connected"], true);
    }

    // The same state again is not written; a change is.
    tokio::time::sleep(Duration::from_millis(20)).await;
    agent.send(heartbeat(running)).await;
    agent.send(heartbeat(None)).await;
    let seen = node_when(|node| node["engine"] == Value::Null).await;
    assert_eq!(seen["applied"]["engine"], digest("a"));
    let cleared_at = seen["state_changed_at"].as_i64().unwrap();
    assert!(cleared_at > changed_at);
    agent
        .send(heartbeat(Some(EngineState {
            reachable: false,
            version: String::new(),
        })))
        .await;
    let seen = node_when(|node| node["engine"] != Value::Null).await;
    assert_eq!(seen["engine"], json!({ "reachable": false, "version": "" }));

    // The last report is kept when the node goes away.
    drop(agent);
    let seen = node_when(|node| node["connected"] == false).await;
    assert_eq!(seen["session"], Value::Null);
    assert_eq!(seen["applied"]["broker"], digest("b"));

    owner.shutdown().await;
    other.shutdown().await;
}

/// A plain tar (ustar) holding one regular file.
fn plain_tar(name: &str, content: &[u8]) -> Vec<u8> {
    let mut header = [0u8; 512];
    header[..name.len()].copy_from_slice(name.as_bytes());
    header[100..107].copy_from_slice(b"0000644");
    header[108..115].copy_from_slice(b"0000000");
    header[116..123].copy_from_slice(b"0000000");
    header[124..135].copy_from_slice(format!("{:011o}", content.len()).as_bytes());
    header[136..147].copy_from_slice(b"00000000000");
    header[156] = b'0';
    header[257..263].copy_from_slice(b"ustar\0");
    header[263..265].copy_from_slice(b"00");
    // The checksum is computed with its own field read as spaces.
    header[148..156].fill(b' ');
    let sum: u32 = header.iter().map(|b| u32::from(*b)).sum();
    header[148..155].copy_from_slice(format!("{sum:06o}\0").as_bytes());
    let mut tar = header.to_vec();
    tar.extend_from_slice(content);
    tar.resize(tar.len().div_ceil(512) * 512 + 1024, 0);
    tar
}

/// `POST /v1/nodes/{node}/config/{target}/validation{query}` of a small bundle.
async fn validate_config(
    rest: &Rest,
    node: &str,
    target: &str,
    query: &str,
    content: &str,
) -> (u16, Value) {
    let path = format!("/v1/nodes/{node}/config/{target}/validation{query}");
    Rest::json(rest.request("POST", &path).body(small_bundle(content))).await
}

/// Result of the configuration checker, as the agent reports it.
fn checker_result(exit_code: i32, stdout: &str) -> Body {
    Body::CommandResult(CommandResult {
        index: 0,
        command: "centengine -v".into(),
        exit_code,
        stdout: stdout.into(),
        stderr: String::new(),
        duration_ms: 40,
        timed_out: false,
    })
}

#[tokio::test]
async fn configuration_is_validated_on_the_node_without_being_pushed() {
    let db = TestDb::new();
    let server = start(&db, |_| {}).await;
    let rest = Rest::new(&server);
    let (node, other_node) = (unique_node(), unique_node());
    let mut agent = FakeAgent::connect(server.agent_addr, &node, None)
        .await
        .unwrap();
    let mut other = FakeAgent::connect(server.agent_addr, &other_node, None)
        .await
        .unwrap();
    let route = format!("/v1/nodes/{node}/config/engine/validation");

    // Bundle body.
    let bundle = small_bundle(&unique_node());
    let (status, body) = Rest::json(rest.request("POST", &route).body(bundle.clone())).await;
    assert_eq!(status, 202, "{body}");
    let digest = gorgone_core::bundle::digest(&bundle);
    let id = body["id"].as_str().unwrap().to_owned();
    assert_eq!(
        body,
        json!({ "id": id, "digest": digest, "size": bundle.len() })
    );

    let job = agent.next_dispatch().await;
    assert_eq!(job.id, id);
    let Some(Kind::ValidateConfig(validate)) = job.action.unwrap().kind else {
        panic!("expected ValidateConfig");
    };
    assert_eq!(
        (validate.target.as_str(), validate.digest.as_str()),
        ("engine", digest.as_str())
    );
    assert_eq!(validate.size, bundle.len() as u64);

    // The blob goes to the node being validated, and to no other.
    assert_eq!(agent.fetch_blob(&digest).await.unwrap().concat(), bundle);
    let refused = other.fetch_blob(&digest).await.unwrap_err();
    assert_eq!(refused.code(), Code::NotFound);

    // Many lines, yet under the per-event cap: kept as is.
    let output: String = (0..2000)
        .map(|line| format!("Warning: service {line} has no contact\n"))
        .collect::<String>()
        + "Error: host 'srv-é' has no address\nTotal Errors:   1\n";
    agent.event(&id, 1, Body::Started(JobStarted {})).await;
    agent.event(&id, 2, checker_result(1, &output)).await;
    agent
        .event(
            &id,
            3,
            finished(Outcome::Failed, "configuration is not valid"),
        )
        .await;

    let job = finished_job(&rest, &id).await;
    assert_eq!(job["kind"], "validate_config");
    assert_eq!(job["state"], "failed");
    assert_eq!(
        job["action"],
        json!({ "validate_config": { "target": "engine", "digest": digest, "size": bundle.len() } })
    );
    let events = job["events"].as_array().unwrap();
    assert_eq!(events.len(), 4, "{job}");
    assert_eq!(events[2]["code"], 100);
    assert_eq!(events[2]["data"]["exit_code"], 1);
    assert_eq!(events[2]["data"]["stdout"], output.as_str());
    assert_eq!(events[2]["data"]["truncated"], false);
    assert_eq!(events[3]["code"], 1);
    assert_eq!(events[3]["data"]["message"], "configuration is not valid");

    // Legacy log: same events, `data` being a JSON string in the Perl shape.
    let (status, log) = rest.get(&format!("/api/log/{id}")).await;
    assert_eq!(status, 200);
    assert_eq!(codes(&log), [0, 4, 100, 1]);
    let data: Value = serde_json::from_str(log["data"][2]["data"].as_str().unwrap()).unwrap();
    assert_eq!(data["result"]["exit_code"], 1);
    assert_eq!(data["result"]["stdout"], output.as_str());
    assert_eq!(data["message"], "command execution issue");

    // Finished: the blob is no longer handed out.
    let refused = agent.fetch_blob(&digest).await.unwrap_err();
    assert_eq!(refused.code(), Code::NotFound);

    // Plain tar body, repacked by the server; a valid configuration.
    let tar = plain_tar("centengine.cfg", unique_node().as_bytes());
    let (status, body) = Rest::json(
        rest.request("POST", &route)
            .header("content-type", "application/x-tar")
            .body(tar.clone()),
    )
    .await;
    assert_eq!(status, 202, "{body}");
    let (id, digest) = (
        body["id"].as_str().unwrap().to_owned(),
        body["digest"].as_str().unwrap().to_owned(),
    );
    let job = agent.next_dispatch().await;
    assert!(matches!(
        job.action.unwrap().kind,
        Some(Kind::ValidateConfig(validate)) if validate.digest == digest
    ));
    let fetched = agent.fetch_blob(&digest).await.unwrap().concat();
    assert_eq!(gorgone_core::bundle::digest(&fetched), digest);
    assert_eq!(body["size"], fetched.len());
    let dest = tempfile::tempdir().unwrap();
    let options = gorgone_core::bundle::ApplyOptions {
        marker: ".digest",
        prune: true,
        max_unpacked: gorgone_core::bundle::DEFAULT_MAX_UNPACKED,
    };
    let report = gorgone_core::bundle::apply(&fetched, &digest, dest.path(), &options).unwrap();
    assert_eq!(report.written, 1);
    agent
        .event(&id, 1, checker_result(0, "Things look okay\n"))
        .await;
    agent.event(&id, 2, finished(Outcome::Ok, "")).await;
    let job = finished_job(&rest, &id).await;
    assert_eq!(job["state"], "succeeded");
    assert_eq!(job["events"][1]["data"]["stdout"], "Things look okay\n");
    let (_, log) = rest.get(&format!("/api/log/{id}")).await;
    assert_eq!(codes(&log), [0, 100, 2]);

    // Same refusals as a push: not a bundle, a tar sent as a bundle, a
    // target too long to be stored.
    let (status, _) = Rest::json(rest.request("POST", &route).body("not a bundle")).await;
    assert_eq!(status, 400);
    let (status, _) = Rest::json(rest.request("POST", &route).body(tar)).await;
    assert_eq!(status, 400);
    let long_target = format!("/v1/nodes/{node}/config/{}/validation", "t".repeat(192));
    let (status, body) = Rest::json(rest.request("POST", &long_target).body(bundle)).await;
    assert_eq!(status, 400, "{body}");

    server.shutdown().await;
}

#[tokio::test]
async fn validation_is_not_the_latest_push_of_its_target() {
    let db = TestDb::new();
    let server = start(&db, |c| c.sweep_interval_ms = 5000).await;
    let rest = Rest::new(&server);
    let node = unique_node();
    let mut agent = FakeAgent::connect(server.agent_addr, &node, None)
        .await
        .unwrap();
    let latest = || restart(json!({ "requires_latest_config": true }));
    let requires = |id: String| {
        let rest = &rest;
        async move { rest.get(&format!("/v1/jobs/{id}")).await.1["requires"].clone() }
    };

    // A failed validation, alone: a restart has nothing to wait for.
    let (status, body) = validate_config(&rest, &node, "engine", "", &unique_node()).await;
    assert_eq!(status, 202, "{body}");
    let rejected = body["id"].as_str().unwrap().to_owned();
    assert_eq!(agent.next_dispatch().await.id, rejected);
    agent
        .event(&rejected, 1, checker_result(1, "Error\n"))
        .await;
    agent
        .event(&rejected, 2, finished(Outcome::Failed, "not valid"))
        .await;
    let free = submit(&rest, &node, &latest()).await;
    assert_eq!(requires(free.clone()).await, json!([]));
    assert_eq!(agent.next_dispatch().await.id, free);
    agent.event(&free, 1, finished(Outcome::Ok, "")).await;

    // A real push is recorded, and a failed validation after it changes
    // nothing: the restart still depends on the push alone.
    let pushed = push(&rest, &node, "engine").await;
    assert_eq!(agent.next_dispatch().await.id, pushed);
    agent.event(&pushed, 1, finished(Outcome::Ok, "")).await;
    let (status, body) = validate_config(&rest, &node, "engine", "", &unique_node()).await;
    assert_eq!(status, 202, "{body}");
    let rejected = body["id"].as_str().unwrap().to_owned();
    assert_eq!(agent.next_dispatch().await.id, rejected);
    agent
        .event(&rejected, 1, finished(Outcome::Failed, "not valid"))
        .await;
    let restarted = submit(&rest, &node, &latest()).await;
    assert_eq!(requires(restarted.clone()).await, json!([pushed]));
    assert_eq!(agent.next_dispatch().await.id, restarted);

    server.shutdown().await;
}

#[tokio::test]
async fn validation_expires_fast_and_its_id_makes_a_retry_harmless() {
    let db = TestDb::new();
    let server = start(&db, |c| c.validation_deadline_s = 1).await;
    let rest = Rest::new(&server);
    let node = unique_node();
    let deadline_of = |id: String| {
        let rest = &rest;
        async move {
            let (_, job) = rest.get(&format!("/v1/jobs/{id}")).await;
            job["deadline_at"].as_i64().unwrap() - job["created_at"].as_i64().unwrap()
        }
    };

    // No agent: the default deadline of validations applies, not the one of
    // the other jobs (300 s), and the explicit one wins.
    let (status, body) = validate_config(&rest, &node, "engine", "", "a=1\n").await;
    assert_eq!(status, 202, "{body}");
    let short = body["id"].as_str().unwrap().to_owned();
    assert_eq!(deadline_of(short.clone()).await, 1000);
    let (status, body) = validate_config(&rest, &node, "engine", "?deadline_s=600", "a=1\n").await;
    assert_eq!(status, 202, "{body}");
    let long = body["id"].as_str().unwrap().to_owned();
    assert_eq!(deadline_of(long.clone()).await, 600_000);
    let (status, _) = validate_config(&rest, &node, "engine", "?deadline_s=soon", "a=1\n").await;
    assert_eq!(status, 400);
    // The same action through the generic route gets the same default.
    let generic = submit(
        &rest,
        &node,
        &json!({ "validate_config": { "target": "engine", "digest": body["digest"] } }),
    )
    .await;
    assert_eq!(deadline_of(generic.clone()).await, 1000);
    let pushed = push(&rest, &node, "engine").await;
    assert_eq!(deadline_of(pushed.clone()).await, 300_000);

    let job = finished_job(&rest, &short).await;
    assert_eq!(job["state"], "expired", "{job}");
    let (_, log) = rest.get(&format!("/api/log/{short}")).await;
    assert_eq!(codes(&log), [0, 1]);
    assert_eq!(finished_job(&rest, &generic).await["state"], "expired");
    let (_, job) = rest.get(&format!("/v1/jobs/{long}")).await;
    assert_eq!(job["state"], "queued");

    // Client id: the retry creates nothing and gets the first answer.
    let query = format!("?id=check-{node}&deadline_s=60");
    let (status, first) = validate_config(&rest, &node, "engine", &query, "a=2\n").await;
    assert_eq!(status, 202, "{first}");
    assert_eq!(first["id"], format!("check-{node}"));
    let (status, second) = validate_config(&rest, &node, "engine", &query, "a=3\n").await;
    assert_eq!((status, &second), (202, &first));
    let (status, body) = validate_config(&rest, &unique_node(), "engine", &query, "a=2\n").await;
    assert_eq!(status, 409, "{body}");
    let (status, _) = validate_config(&rest, &node, "engine", "?id=a.b", "a=2\n").await;
    assert_eq!(status, 400);

    // What is still queued reaches the agent once, in order; then
    // `requires` holds a validation until the named job succeeded.
    let mut agent = FakeAgent::connect(server.agent_addr, &node, None)
        .await
        .unwrap();
    let mut received = Vec::new();
    for _ in 0..3 {
        received.push(agent.next_dispatch().await.id);
    }
    assert_eq!(received, [long, pushed, format!("check-{node}")]);
    let first_job = submit(&rest, &node, &run_commands("echo first")).await;
    let query = format!("?requires={first_job}&deadline_s=60");
    let (status, body) = validate_config(&rest, &node, "engine", &query, "a=4\n").await;
    assert_eq!(status, 202, "{body}");
    let held = body["id"].as_str().unwrap().to_owned();
    assert_eq!(agent.next_dispatch().await.id, first_job);
    assert_nothing_received(&mut agent).await;
    agent.event(&first_job, 1, finished(Outcome::Ok, "")).await;
    let job = agent.next_dispatch().await;
    assert_eq!((job.id, job.requires), (held, vec![first_job]));

    server.shutdown().await;
}

#[tokio::test]
async fn validated_bundle_is_pushed_by_its_digest_without_a_second_upload() {
    let db = TestDb::new();
    let server = start(&db, |_| {}).await;
    let rest = Rest::new(&server);
    let node = unique_node();
    let mut agent = FakeAgent::connect(server.agent_addr, &node, None)
        .await
        .unwrap();
    let put = |query: String, body: Vec<u8>| {
        let rest = &rest;
        let path = format!("/v1/nodes/{node}/config/engine{query}");
        async move { Rest::json(rest.request("PUT", &path).body(body)).await }
    };

    let bundle = small_bundle(&unique_node());
    let (status, validated) = Rest::json(
        rest.request(
            "POST",
            &format!("/v1/nodes/{node}/config/engine/validation"),
        )
        .body(bundle.clone()),
    )
    .await;
    assert_eq!(status, 202, "{validated}");
    let digest = validated["digest"].as_str().unwrap().to_owned();
    let validation = agent.next_dispatch().await.id;
    agent.event(&validation, 1, checker_result(0, "ok\n")).await;
    agent.event(&validation, 2, finished(Outcome::Ok, "")).await;

    // Known digest, in any case: an ordinary push of the stored bundle.
    let query = format!("?digest={}&id=apply-{node}", digest.to_uppercase());
    let (status, pushed) = put(query.clone(), Vec::new()).await;
    assert_eq!(status, 202, "{pushed}");
    assert_eq!(
        pushed,
        json!({ "id": format!("apply-{node}"), "digest": digest, "size": bundle.len() })
    );
    let (status, again) = put(query, Vec::new()).await;
    assert_eq!((status, &again), (202, &pushed));
    let job = agent.next_dispatch().await;
    assert_eq!(job.id, format!("apply-{node}"));
    let Some(Kind::ApplyConfig(apply)) = job.action.unwrap().kind else {
        panic!("expected ApplyConfig");
    };
    assert_eq!(
        (apply.target.as_str(), apply.digest.as_str(), apply.size),
        ("engine", digest.as_str(), bundle.len() as u64)
    );
    assert_eq!(agent.fetch_blob(&digest).await.unwrap().concat(), bundle);
    assert_nothing_received(&mut agent).await;

    // It is the latest push of the target. The agent validates again before
    // applying: its refusal keeps the checker output in the events.
    let restarted = submit(
        &rest,
        &node,
        &restart(json!({ "requires_latest_config": true })),
    )
    .await;
    agent
        .event(&job.id, 1, checker_result(1, "Error: bad\n"))
        .await;
    agent
        .event(
            &job.id,
            2,
            finished(Outcome::Failed, "configuration is not valid"),
        )
        .await;
    assert_failed_on(&rest, &restarted, &job.id, "failed").await;
    let applied = finished_job(&rest, &job.id).await;
    assert_eq!(applied["kind"], "apply_config");
    assert_eq!(applied["state"], "failed");
    assert_eq!(applied["events"][1]["code"], 100);
    assert_eq!(applied["events"][1]["data"]["stdout"], "Error: bad\n");

    // Unknown digest, malformed digest, digest with a body.
    let (status, body) = put(format!("?digest={}", "0".repeat(64)), Vec::new()).await;
    assert_eq!(status, 404, "{body}");
    assert_eq!(body["error"], "not_found");
    assert!(
        body["message"].as_str().unwrap().contains("digest"),
        "{body}"
    );
    let (status, body) = put("?digest=abc".into(), Vec::new()).await;
    assert_eq!(status, 400, "{body}");
    let (status, body) = put(format!("?digest={digest}"), bundle.clone()).await;
    assert_eq!(status, 400, "{body}");
    assert_eq!(body["error"], "bad_request");
    // Without a digest an empty body is still not a bundle.
    let (status, _) = put(String::new(), Vec::new()).await;
    assert_eq!(status, 400);
    assert_nothing_received(&mut agent).await;

    server.shutdown().await;
}

#[tokio::test]
async fn validate_config_action_is_accepted_as_json() {
    let db = TestDb::new();
    let server = start(&db, |_| {}).await;
    let rest = Rest::new(&server);
    let node = unique_node();
    let mut agent = FakeAgent::connect(server.agent_addr, &node, None)
        .await
        .unwrap();
    let jobs = format!("/v1/nodes/{node}/jobs");

    // The blob must exist: here it comes from a push.
    let (status, pushed) = push_config(&rest, &node, "engine", "", &unique_node()).await;
    assert_eq!(status, 202, "{pushed}");
    let push_id = agent.next_dispatch().await.id;
    let digest = pushed["digest"].as_str().unwrap();

    let action = json!({ "validate_config": {
        "target": "engine", "digest": digest.to_uppercase(), "size": pushed["size"] } });
    let id = submit(&rest, &node, &action).await;
    let job = agent.next_dispatch().await;
    assert_eq!(job.id, id);
    assert!(matches!(
        job.action.unwrap().kind,
        Some(Kind::ValidateConfig(validate))
            if validate.target == "engine" && validate.digest == digest
    ));
    let (_, job) = rest.get(&format!("/v1/jobs/{id}")).await;
    assert_eq!(job["kind"], "validate_config");
    assert_eq!(
        job["action"],
        json!({ "validate_config": {
            "target": "engine", "digest": digest, "size": pushed["size"] } })
    );
    // Not a push: the latest one of the target is still the real one.
    let restarted = submit(
        &rest,
        &node,
        &restart(json!({ "requires_latest_config": ["engine"] })),
    )
    .await;
    let (_, job) = rest.get(&format!("/v1/jobs/{restarted}")).await;
    assert_eq!(job["requires"], json!([push_id]));

    for bad in [
        json!({ "validate_config": { "target": "engine", "digest": "0".repeat(64) } }),
        json!({ "validate_config": { "target": "", "digest": digest } }),
        json!({ "validate_config": { "digest": digest } }),
    ] {
        let (status, body) = rest.post(&jobs, &bad).await;
        assert!(status == 400 || status == 422, "{bad}: {status} {body}");
    }
    let (status, text) = rest.get_plain("/metrics").await.unwrap();
    assert_eq!(status, 200);
    assert!(
        text.contains("gorgone_jobs_submitted_total{kind=\"validate_config\"}"),
        "{text}"
    );

    server.shutdown().await;
}
