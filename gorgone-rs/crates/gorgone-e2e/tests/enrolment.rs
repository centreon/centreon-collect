//! A real agent that only knows the server address and an enrolment token:
//! it enrols, keeps its identity, comes back without the token, and is
//! refused once its node is deleted.

mod common;

use std::net::SocketAddr;
use std::os::unix::fs::PermissionsExt;
use std::path::Path;
use std::time::Duration;

use common::Http;
use gorgone_agent::AgentConfig;
use gorgone_server::{Config, Server, ServerHandle};
use serde_json::{Value, json};
use sha2::Digest;
use tokio::sync::oneshot;

const NODE: &str = "41";
const TOKEN: &str = "enrolment-token-of-the-e2e-test";

async fn server(dir: &Path) -> ServerHandle {
    let any = SocketAddr::from(([127, 0, 0, 1], 0));
    Server::start(Config {
        rest_listen: any,
        agent_listen: any,
        peer_listen: any,
        database_url: format!("sqlite://{}/gorgone.db?mode=rwc", dir.display()),
        heartbeat_interval_s: 1,
        sweep_interval_ms: 200,
        // The shared-token mode is on: enrolled nodes must not fall back to it.
        auto_enroll: true,
        insecure_no_auth: true,
        ..Config::default()
    })
    .await
    .expect("server starts")
}

fn agent_config(server: &ServerHandle, state_dir: &Path) -> AgentConfig {
    let toml = format!(
        r#"
server_url = "http://{agent}"
enrolment_token = "{TOKEN}"
state_dir = "{state}"

[reconnect]
min_ms = 50
max_ms = 200

[commands]
allowlist = ['echo \w+']
"#,
        agent = server.agent_addr,
        state = state_dir.display(),
    );
    AgentConfig::from_toml(&toml).expect("agent config")
}

fn run_agent(config: AgentConfig) -> (oneshot::Sender<()>, tokio::task::JoinHandle<()>) {
    let (stop, stopped) = oneshot::channel::<()>();
    let task = tokio::spawn(async move {
        gorgone_agent::run(config, async {
            stopped.await.ok();
        })
        .await
        .expect("agent runs");
    });
    (stop, task)
}

async fn node(http: Http) -> Option<Value> {
    let (status, body) = http
        .request("GET", &format!("/v1/nodes/{NODE}"), "", b"")
        .await;
    (status == 200).then(|| serde_json::from_slice(&body).unwrap())
}

async fn wait_connected(http: Http) {
    for _ in 0..200 {
        if node(http).await.is_some_and(|n| n["connected"] == true) {
            return;
        }
        tokio::time::sleep(Duration::from_millis(50)).await;
    }
    panic!("node {NODE} never connected");
}

async fn echo_succeeds(http: Http) {
    let action = json!({ "run_commands": { "commands": [{ "command": "echo enrolled" }] } });
    let id = http
        .json("POST", &format!("/v1/nodes/{NODE}/jobs"), Some(&action))
        .await["id"]
        .as_str()
        .unwrap()
        .to_owned();
    let job = http
        .json("GET", &format!("/v1/jobs/{id}?wait=10"), None)
        .await;
    assert_eq!(job["state"], "succeeded", "{job}");
}

#[tokio::test(flavor = "multi_thread")]
async fn agent_enrols_keeps_its_identity_and_is_revoked_with_its_node() {
    let dir = tempfile::tempdir().unwrap();
    let state_dir = dir.path().join("state");
    let server = server(dir.path()).await;
    let http = Http {
        addr: server.rest_addr,
    };

    // Started before the central pushed the token: it retries.
    let (stop, agent) = run_agent(agent_config(&server, &state_dir));
    tokio::time::sleep(Duration::from_millis(300)).await;
    assert!(node(http).await.is_none());
    let hash: String = sha2::Sha256::digest(TOKEN.as_bytes())
        .iter()
        .map(|b| format!("{b:02x}"))
        .collect();
    let (status, _) = http
        .request(
            "PUT",
            "/v1/enrolment-tokens/e2e",
            "application/json",
            json!({ "token_hash": hash, "node_id": NODE, "expires_in_s": 600 })
                .to_string()
                .as_bytes(),
        )
        .await;
    assert_eq!(status, 201);
    wait_connected(http).await;
    echo_succeeds(http).await;

    // The identity is kept, private, with a credential of its own.
    let identity = state_dir.join("identity");
    let text = std::fs::read_to_string(&identity).unwrap();
    assert!(text.contains(&format!("node_id={NODE}\n")), "{text}");
    assert!(!text.contains(TOKEN));
    let mode = std::fs::metadata(&identity).unwrap().permissions().mode();
    assert_eq!(mode & 0o777, 0o600);
    let enrolments = http.json("GET", "/v1/enrolments", None).await;
    assert_eq!(enrolments["enrolments"][0]["state"], "enrolled");

    // A restart reconnects with the kept identity: the token, used up now,
    // is not presented again.
    stop.send(()).unwrap();
    agent.await.unwrap();
    let (stop, agent) = run_agent(agent_config(&server, &state_dir));
    wait_connected(http).await;
    echo_succeeds(http).await;

    // Deleting the node revokes the credential: the agent keeps trying, in
    // vain, although the server accepts unknown nodes (auto_enroll).
    let (status, _) = http
        .request("DELETE", &format!("/v1/nodes/{NODE}"), "", b"")
        .await;
    assert_eq!(status, 204);
    tokio::time::sleep(Duration::from_secs(1)).await;
    assert!(node(http).await.is_none());
    let enrolments = http.json("GET", "/v1/enrolments", None).await;
    assert_eq!(enrolments["enrolments"][0]["state"], "revoked");

    stop.send(()).unwrap();
    agent.await.unwrap();
    server.shutdown().await;
}
