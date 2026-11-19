//! `ServiceControl` jobs mapped to HTTP calls (`http-post:`), for a service
//! whose container exposes a control API.

mod support;

use std::net::SocketAddr;
use std::sync::{Arc, Mutex};

use gorgone_agent::AgentConfig;
use gorgone_proto::agent::job_finished::Outcome;
use gorgone_proto::agent::service_control::Operation;
use support::{Agent, FakeServer, base_config, dissect, outcome, service_control};
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::TcpListener;

/// Control API answering every request with the same status line.
struct FakeControl {
    addr: SocketAddr,
    requests: Arc<Mutex<Vec<String>>>,
    task: tokio::task::JoinHandle<()>,
}

impl FakeControl {
    async fn start(status: &'static str) -> Self {
        let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
        let addr = listener.local_addr().unwrap();
        let requests = Arc::new(Mutex::new(Vec::new()));
        let seen = requests.clone();
        let task = tokio::spawn(async move {
            loop {
                let (mut stream, _) = listener.accept().await.unwrap();
                let mut request = Vec::new();
                let mut chunk = [0_u8; 1024];
                while !request.ends_with(b"\r\n\r\n") {
                    let read = stream.read(&mut chunk).await.unwrap();
                    if read == 0 {
                        break;
                    }
                    request.extend_from_slice(&chunk[..read]);
                }
                let request = String::from_utf8(request).unwrap();
                let line = request.lines().next().unwrap_or_default().to_owned();
                seen.lock().unwrap().push(line);
                let answer = format!("HTTP/1.1 {status}\r\nContent-Length: 0\r\n\r\n");
                // The client may already be gone.
                let _ = stream.write_all(answer.as_bytes()).await;
            }
        });
        Self {
            addr,
            requests,
            task,
        }
    }

    /// Request lines received so far.
    fn requests(&self) -> Vec<String> {
        self.requests.lock().unwrap().clone()
    }
}

impl Drop for FakeControl {
    fn drop(&mut self) {
        self.task.abort();
    }
}

#[tokio::test]
async fn service_operations_post_to_every_configured_url() {
    let dir = tempfile::tempdir().unwrap();
    let stopped = dir.path().join("stopped");
    let broker = FakeControl::start("200 OK").await;
    let rrd = FakeControl::start("204 No Content").await;
    let failing = FakeControl::start("500 Internal Server Error").await;
    // Nothing listens on this port any more.
    let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
    let down = listener.local_addr().unwrap();
    drop(listener);

    let mut server = FakeServer::start().await;
    let config = format!(
        "http_post_timeout_ms = 1000\n{}[services.broker]\n\
         reload = \"http-post:http://{broker}/reload http://{rrd}/reload\"\n\
         restart = \"http-post:http://{down}/restart http://{failing}/restart \
         http://{broker}/restart\"\n\
         stop = \"touch {stopped}\"\n",
        base_config(&format!("http://{}", server.addr)),
        broker = broker.addr,
        rrd = rrd.addr,
        failing = failing.addr,
        stopped = support::path_str(&stopped),
    );
    let agent = Agent::start(&config);
    let mut session = server.next_session().await;

    session
        .dispatch(service_control("reload", "broker", Operation::Reload))
        .await;
    let (results, _, finished) = dissect(&session.job_events("reload").await);
    assert_eq!(outcome(&finished), Outcome::Ok, "{finished:?}");
    assert!(
        finished
            .message
            .starts_with("reload broker: 2 of 2 HTTP calls accepted; "),
        "{finished:?}"
    );
    let commands: Vec<&str> = results.iter().map(|r| r.command.as_str()).collect();
    assert_eq!(
        commands,
        [
            format!("POST http://{}/reload", broker.addr),
            format!("POST http://{}/reload", rrd.addr)
        ]
    );
    assert_eq!(results[0].stdout, "HTTP/1.1 200 OK");
    assert_eq!(results[1].stdout, "HTTP/1.1 204 No Content");
    assert!(results.iter().all(|result| result.exit_code == 0));
    assert_eq!(rrd.requests(), ["POST /reload HTTP/1.1"]);

    session
        .dispatch(service_control("restart", "broker", Operation::Restart))
        .await;
    let (results, _, finished) = dissect(&session.job_events("restart").await);
    assert_eq!(outcome(&finished), Outcome::Failed, "{finished:?}");
    assert!(
        finished
            .message
            .starts_with("restart broker: 1 of 3 HTTP calls accepted; "),
        "{finished:?}"
    );
    let codes: Vec<i32> = results.iter().map(|result| result.exit_code).collect();
    assert_eq!(codes, [1, 1, 0]);
    assert!(results[0].stderr.starts_with("connection to "));
    assert_eq!(results[1].stdout, "HTTP/1.1 500 Internal Server Error");
    // Called although the two before it failed.
    assert_eq!(
        broker.requests(),
        ["POST /reload HTTP/1.1", "POST /restart HTTP/1.1"]
    );

    // A command line next to HTTP operations still runs as a command.
    session
        .dispatch(service_control("stop", "broker", Operation::Stop))
        .await;
    let (results, _, finished) = dissect(&session.job_events("stop").await);
    assert_eq!(outcome(&finished), Outcome::Ok, "{finished:?}");
    assert_eq!(results.len(), 1);
    assert!(stopped.exists());
    agent.stop().await.unwrap();
}

#[test]
fn url_lists_are_checked_when_the_configuration_is_loaded() {
    let with = |reload: &str| {
        AgentConfig::from_toml(&format!(
            "{}[services.broker]\nreload = \"{reload}\"\n",
            base_config("http://central:5556")
        ))
    };
    assert!(with("http-post:http://broker:8080/reload http://broker-rrd:8080/reload").is_ok());
    for (reload, needle) in [
        (
            "http-post:https://broker:8080/reload",
            "TLS is out of scope",
        ),
        ("http-post:broker:8080/reload", "expected http://host"),
        ("http-post:http://user@broker/reload", "credentials"),
        ("http-post:", "at least one http:// URL"),
    ] {
        let message = with(reload).unwrap_err().to_string();
        assert!(message.contains("services.broker.reload"), "{message}");
        assert!(message.contains(needle), "{message}");
    }
}
