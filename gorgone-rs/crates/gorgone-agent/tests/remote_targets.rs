//! Targets sent to remote control APIs (`urls`): plain tar `PUT`s, bearer
//! token, applied digest in heartbeats, kept across a restart.

mod support;

use std::io::Read;
use std::net::SocketAddr;
use std::sync::{Arc, Mutex};

use gorgone_core::bundle;
use gorgone_proto::agent::AppliedConfig;
use gorgone_proto::agent::job_finished::Outcome;
use gorgone_proto::agent::service_control::Operation;
use support::{
    Agent, FakeServer, apply_config, base_config, dissect, outcome, path_str, service_control,
};
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::{TcpListener, TcpStream};

/// One request as received: head and body.
type Received = (String, Vec<u8>);

/// Control API answering every request with `204 No Content`.
struct FakeControl {
    addr: SocketAddr,
    requests: Arc<Mutex<Vec<Received>>>,
    task: tokio::task::JoinHandle<()>,
}

async fn read_request(stream: &mut TcpStream) -> Received {
    let mut received = Vec::new();
    let mut chunk = [0_u8; 4096];
    let end = loop {
        if let Some(end) = received.windows(4).position(|w| w == b"\r\n\r\n") {
            break end + 4;
        }
        let read = stream.read(&mut chunk).await.unwrap();
        assert!(read > 0, "request cut short");
        received.extend_from_slice(&chunk[..read]);
    };
    let head = String::from_utf8(received[..end].to_vec()).unwrap();
    let length: usize = head
        .lines()
        .find_map(|line| line.strip_prefix("Content-Length: "))
        .map_or(0, |value| value.parse().unwrap());
    let mut body = received[end..].to_vec();
    while body.len() < length {
        let read = stream.read(&mut chunk).await.unwrap();
        assert!(read > 0, "body cut short");
        body.extend_from_slice(&chunk[..read]);
    }
    (head, body)
}

impl FakeControl {
    async fn start() -> Self {
        let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
        let addr = listener.local_addr().unwrap();
        let requests = Arc::new(Mutex::new(Vec::new()));
        let seen = requests.clone();
        let task = tokio::spawn(async move {
            loop {
                let (mut stream, _) = listener.accept().await.unwrap();
                let request = read_request(&mut stream).await;
                seen.lock().unwrap().push(request);
                let _ = stream.write_all(b"HTTP/1.1 204 No Content\r\n\r\n").await;
            }
        });
        Self {
            addr,
            requests,
            task,
        }
    }

    fn requests(&self) -> Vec<Received> {
        self.requests.lock().unwrap().clone()
    }
}

impl Drop for FakeControl {
    fn drop(&mut self) {
        self.task.abort();
    }
}

fn header<'a>(head: &'a str, name: &str) -> Option<&'a str> {
    head.lines()
        .find_map(|line| line.strip_prefix(name)?.strip_prefix(": "))
}

fn tar_files(body: &[u8]) -> Vec<(String, String)> {
    let mut archive = tar::Archive::new(body);
    archive
        .entries()
        .unwrap()
        .map(|entry| {
            let mut entry = entry.unwrap();
            let mut content = String::new();
            entry.read_to_string(&mut content).unwrap();
            (entry.path().unwrap().display().to_string(), content)
        })
        .collect()
}

fn bundle_of(name: &str, content: &str) -> (String, Vec<u8>) {
    let src = tempfile::tempdir().unwrap();
    std::fs::write(src.path().join(name), content).unwrap();
    let blob = bundle::pack(src.path()).unwrap();
    (bundle::digest(&blob), blob)
}

fn applied(target: &str, digest: &str) -> AppliedConfig {
    AppliedConfig {
        target: target.to_owned(),
        digest: digest.to_owned(),
    }
}

#[tokio::test]
async fn remote_targets_are_pushed_reported_and_remembered_across_a_restart() {
    let broker = FakeControl::start().await;
    let state = tempfile::tempdir().unwrap();
    let engine_dir = tempfile::tempdir().unwrap();
    let mut server = FakeServer::start().await;
    let config = format!(
        "state_dir = \"{state}\"\nhttp_bearer_token = \"c0ntr0l\"\n{base}\
         [targets.broker]\nurls = [\"http://{broker}/files/broker\"]\n\
         [targets.engine_context]\npath = \"{engine}\"\nfile_mode = \"0640\"\n\
         urls = [\"http://{broker}/files/engine_context\"]\n\
         [services.broker]\nreload = \"http-post:http://{broker}/reload\"\n",
        state = path_str(state.path()),
        base = base_config(&format!("http://{}", server.addr)),
        broker = broker.addr,
        engine = path_str(engine_dir.path()),
    );
    let agent = Agent::start(&config);
    let mut session = server.next_session().await;
    assert_eq!(session.next_heartbeat().await.applied, []);

    let (broker_digest, broker_blob) = bundle_of("central-broker.json", "{\"broker\":1}\n");
    let (context_digest, context_blob) = bundle_of("engine-context.json", "{\"key\":\"k\"}\n");
    server.add_blob(&broker_digest, broker_blob);
    server.add_blob(&context_digest, context_blob);
    session
        .dispatch(apply_config(
            "context",
            "engine_context",
            &context_digest,
            0,
        ))
        .await;
    session
        .dispatch(apply_config("broker", "broker", &broker_digest, 0))
        .await;
    session
        .dispatch(service_control("reload", "broker", Operation::Reload))
        .await;
    for job in ["context", "broker", "reload"] {
        let (_, _, finished) = dissect(&session.job_events(job).await);
        assert_eq!(outcome(&finished), Outcome::Ok, "{job}: {finished:?}");
    }
    let wanted = [
        applied("broker", &broker_digest),
        applied("engine_context", &context_digest),
    ];
    session.heartbeat_where(|beat| beat.applied == wanted).await;

    let requests = broker.requests();
    let lines: Vec<&str> = requests
        .iter()
        .map(|(head, _)| head.lines().next().unwrap())
        .collect();
    assert_eq!(
        lines,
        [
            "PUT /files/engine_context HTTP/1.1",
            "PUT /files/broker HTTP/1.1",
            "POST /reload HTTP/1.1",
        ]
    );
    for (head, _) in &requests {
        assert_eq!(header(head, "Authorization"), Some("Bearer c0ntr0l"));
    }
    assert_eq!(
        tar_files(&requests[1].1),
        [(
            "central-broker.json".to_owned(),
            "{\"broker\":1}\n".to_owned()
        )]
    );
    let local = engine_dir.path().join("engine-context.json");
    let mode =
        std::os::unix::fs::PermissionsExt::mode(&std::fs::metadata(&local).unwrap().permissions());
    assert_eq!(mode & 0o7777, 0o640);
    agent.stop().await.unwrap();

    // The restarted agent reports both before any job, and skips them.
    let agent = Agent::start(&config);
    let mut session = server.next_session().await;
    assert_eq!(session.next_heartbeat().await.applied, wanted);
    session
        .dispatch(apply_config("again", "broker", &broker_digest, 0))
        .await;
    let (results, _, finished) = dissect(&session.job_events("again").await);
    assert_eq!(finished.message, "broker: configuration unchanged");
    assert!(results.is_empty());
    assert_eq!(broker.requests().len(), 3);
    agent.stop().await.unwrap();
}

#[tokio::test]
async fn no_authorization_header_without_a_token() {
    let broker = FakeControl::start().await;
    let mut server = FakeServer::start().await;
    let config = format!(
        "{}[targets.broker]\nurls = [\"http://{}/files/broker\"]\n\
         [services.broker]\nreload = \"http-post:http://{}/reload\"\n",
        base_config(&format!("http://{}", server.addr)),
        broker.addr,
        broker.addr,
    );
    let agent = Agent::start(&config);
    let mut session = server.next_session().await;
    let (digest, blob) = bundle_of("central-broker.json", "{}\n");
    server.add_blob(&digest, blob);
    session
        .dispatch(apply_config("push", "broker", &digest, 0))
        .await;
    session
        .dispatch(service_control("reload", "broker", Operation::Reload))
        .await;
    for job in ["push", "reload"] {
        let (_, _, finished) = dissect(&session.job_events(job).await);
        assert_eq!(outcome(&finished), Outcome::Ok, "{job}: {finished:?}");
    }
    let requests = broker.requests();
    assert_eq!(requests.len(), 2);
    for (head, _) in &requests {
        assert_eq!(header(head, "Authorization"), None, "{head}");
    }
    agent.stop().await.unwrap();
}
