//! End-to-end tests: in-process replicas, a fake agent speaking the gRPC
//! contract, and the REST API.
//!
//! Each test uses its own SQLite file, or the shared database named by
//! `GORGONE_TEST_DATABASE_URL` (node ids are unique per test).

use std::net::SocketAddr;
use std::time::{Duration, Instant};

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

fn unique_node() -> String {
    format!("node-{}", uuid::Uuid::now_v7())
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

/// REST client of one replica.
struct Rest {
    base: String,
    http: reqwest::Client,
    bearer: Option<String>,
}

impl Rest {
    fn new(server: &ServerHandle) -> Self {
        Self {
            base: format!("http://{}", server.rest_addr),
            http: reqwest::Client::new(),
            bearer: None,
        }
    }

    fn request(&self, method: reqwest::Method, path: &str) -> reqwest::RequestBuilder {
        let builder = self.http.request(method, format!("{}{path}", self.base));
        match &self.bearer {
            Some(token) => builder.bearer_auth(token),
            None => builder,
        }
    }

    async fn get(&self, path: &str) -> (u16, Value) {
        Self::json(self.request(reqwest::Method::GET, path)).await
    }

    async fn post(&self, path: &str, body: &Value) -> (u16, Value) {
        Self::json(self.request(reqwest::Method::POST, path).json(body)).await
    }

    async fn json(builder: reqwest::RequestBuilder) -> (u16, Value) {
        let response = builder.send().await.unwrap();
        let status = response.status().as_u16();
        let text = response.text().await.unwrap();
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
        .send(AgentFrameKind::Heartbeat(Heartbeat { running_jobs: 1 }))
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
        rest.request(
            reqwest::Method::PUT,
            &format!("/v1/nodes/{node}/config/engine"),
        )
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
        rest.request(
            reqwest::Method::PUT,
            &format!("/v1/nodes/{node}/config/engine"),
        )
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
    let basic = rest
        .http
        .get(format!("{}/v1/nodes", rest.base))
        .basic_auth("web", Some("rest-test-token"));
    assert_eq!(Rest::json(basic).await.0, 200);
    rest.bearer = Some("rest-test-token".into());
    assert_eq!(rest.get("/v1/nodes").await.0, 200);
    assert_eq!(rest.get("/metrics").await.0, 200);

    // Agent: unknown node, then missing and wrong token.
    let refused = FakeAgent::connect(server.agent_addr, &node, None).await;
    assert_eq!(refused.err().unwrap().code(), Code::Unauthenticated);

    let (status, enrolled) =
        Rest::json(rest.request(reqwest::Method::PUT, &format!("/v1/nodes/{node}"))).await;
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
    let (status, _) =
        Rest::json(rest.request(reqwest::Method::DELETE, &format!("/v1/nodes/{node}"))).await;
    assert_eq!(status, 204);
    assert!(agent.next_frame(STEP).await.is_none());
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
    assert!(
        first.next_frame(STEP).await.is_none(),
        "old session must close"
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
    Rest::json(rest.request(reqwest::Method::DELETE, path)).await
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
    ) -> Result<FakeAgent, String> {
        let channel = self.channel(addr, common_name).await?;
        FakeAgent::connect_channel(channel, node_id, None)
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

    let mut agent = pki.session(server.agent_addr, None, &node).await.unwrap();
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
async fn mtls_certificate_common_name_identifies_the_node() {
    let pki = Pki::new();
    let db = TestDb::new();
    // No auto-enrolment: only the certificate can authenticate the agent.
    let server = start(&db, |c| {
        c.auto_enroll = false;
        c.agent_tls = Some(pki.server("agent", true));
    })
    .await;
    let rest = Rest::new(&server);
    let node = unique_node();

    // A certificate never creates a node: it must be enrolled first.
    let refused = pki
        .session(server.agent_addr, Some(&node), &node)
        .await
        .err()
        .unwrap();
    assert_eq!(refused, "Unauthenticated");
    let (status, _) =
        Rest::json(rest.request(reqwest::Method::PUT, &format!("/v1/nodes/{node}"))).await;
    assert_eq!(status, 200);

    // CN = node id: accepted without any token.
    let mut agent = pki
        .session(server.agent_addr, Some(&node), &node)
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

    // CN ≠ node id: refused, whatever the certificate chain says.
    let other = unique_node();
    let refused = pki
        .session(server.agent_addr, Some(&other), &node)
        .await
        .err()
        .unwrap();
    assert_eq!(refused, "PermissionDenied");

    // No client certificate: the handshake itself fails.
    let refused = tokio::time::timeout(STEP, pki.session(server.agent_addr, None, &node)).await;
    assert!(
        !matches!(refused, Ok(Ok(_))),
        "a client certificate is required"
    );

    server.shutdown().await;
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
        rest.request(
            reqwest::Method::PUT,
            &format!("/v1/nodes/{node}/config/engine"),
        )
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
    // Connected, then never reads its session again.
    let stalled = FakeAgent::connect(server.agent_addr, &stalled_node, None)
        .await
        .unwrap();
    let mut healthy = FakeAgent::connect(server.agent_addr, &healthy_node, None)
        .await
        .unwrap();

    // More than the flow-control windows and the session buffer can hold.
    let big = run_commands(&"x".repeat(192 << 10));
    for _ in 0..300 {
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
    while received < 300 {
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
        async move { Rest::json(rest.request(reqwest::Method::PUT, &path)).await }
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
        .send(AgentFrameKind::Heartbeat(Heartbeat { running_jobs: 0 }))
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
        rest.request(
            reqwest::Method::PUT,
            &format!("/v1/nodes/{node}/config/engine"),
        )
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
    let (status, _) =
        Rest::json(rest.request(reqwest::Method::PUT, &format!("/v1/nodes/{ghost}"))).await;
    assert_eq!(status, 200);
    submit(&rest, &ghost, &run_commands("echo ok")).await;

    // Only the bundle upload accepts a large body. The server answers 413
    // without reading it and closes: the client sees the status, or its
    // write cut short.
    let oversized = rest
        .request(reqwest::Method::POST, &format!("/v1/nodes/{ghost}/jobs"))
        .json(&run_commands(&"x".repeat(2 << 20)))
        .send()
        .await;
    match oversized {
        Ok(response) => assert_eq!(response.status(), 413),
        Err(err) => assert!(err.is_request() || err.is_body(), "{err:?}"),
    }
    // A new client: the connection above is being closed by the server.
    let mut rest = Rest::new(&server);
    rest.bearer = Some("rest-test-token".into());
    let (status, _) = Rest::json(
        rest.request(
            reqwest::Method::PUT,
            &format!("/v1/nodes/{ghost}/config/engine"),
        )
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
