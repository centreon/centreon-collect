//! A real agent that only knows the server address and an enrolment token:
//! it enrols, keeps its identity, comes back without the token, and is
//! refused once its node is deleted. Fleet enrolments wait for an approval
//! when the token asks for it; the credential is rotated on request, and a
//! rotation interrupted at any step finishes at the next start.

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
    agent_config_with(server, state_dir, TOKEN, "")
}

fn agent_config_with(
    server: &ServerHandle,
    state_dir: &Path,
    token: &str,
    extra: &str,
) -> AgentConfig {
    let toml = format!(
        r#"
server_url = "http://{agent}"
enrolment_token = "{token}"
state_dir = "{state}"
{extra}

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
    node_of(http, NODE).await
}

async fn node_of(http: Http, id: &str) -> Option<Value> {
    let (status, body) = http
        .request("GET", &format!("/v1/nodes/{id}"), "", b"")
        .await;
    (status == 200).then(|| serde_json::from_slice(&body).unwrap())
}

async fn wait_connected(http: Http) {
    wait_node_connected(http, NODE).await;
}

async fn wait_node_connected(http: Http, id: &str) {
    // A pending agent asks again every 2 to 10 seconds.
    for _ in 0..600 {
        if node_of(http, id)
            .await
            .is_some_and(|n| n["connected"] == true)
        {
            return;
        }
        tokio::time::sleep(Duration::from_millis(50)).await;
    }
    panic!("node {id} never connected");
}

fn sha256(text: &str) -> String {
    sha2::Sha256::digest(text.as_bytes())
        .iter()
        .map(|b| format!("{b:02x}"))
        .collect()
}

/// Records an enrolment token, as centreon-web does.
async fn put_token(http: Http, id: &str, token: &str, body: Value) {
    let mut body = body;
    body["token_hash"] = sha256(token).into();
    body["expires_in_s"] = 600.into();
    let (status, answer) = http
        .request(
            "PUT",
            &format!("/v1/enrolment-tokens/{id}"),
            "application/json",
            body.to_string().as_bytes(),
        )
        .await;
    assert_eq!(status, 201, "{}", String::from_utf8_lossy(&answer));
}

async fn post(http: Http, path: &str, body: Option<&Value>) -> (u16, Value) {
    let body = body.map(Value::to_string).unwrap_or_default();
    let (status, answer) = http
        .request("POST", path, "application/json", body.as_bytes())
        .await;
    let answer = serde_json::from_slice(&answer).unwrap_or(Value::Null);
    (status, answer)
}

/// Enrolments of the token `token_id`, oldest first.
async fn enrolments_of(http: Http, token_id: &str) -> Vec<Value> {
    let all = http.json("GET", "/v1/enrolments", None).await;
    all["enrolments"]
        .as_array()
        .unwrap()
        .iter()
        .filter(|e| e["token_id"] == token_id)
        .cloned()
        .collect()
}

/// Waits until `count` enrolments of the token exist.
async fn wait_enrolments(http: Http, token_id: &str, count: usize) -> Vec<Value> {
    for _ in 0..200 {
        let enrolments = enrolments_of(http, token_id).await;
        if enrolments.len() >= count {
            return enrolments;
        }
        tokio::time::sleep(Duration::from_millis(50)).await;
    }
    panic!("{count} enrolments of {token_id} never came");
}

async fn echo_succeeds(http: Http) {
    echo_succeeds_on(http, NODE).await;
}

async fn echo_succeeds_on(http: Http, node: &str) {
    let action = json!({ "run_commands": { "commands": [{ "command": "echo enrolled" }] } });
    let id = http
        .json("POST", &format!("/v1/nodes/{node}/jobs"), Some(&action))
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

/// Field `name` of the identity file of `state_dir`.
fn identity_field(state_dir: &Path, name: &str) -> Option<String> {
    let text = std::fs::read_to_string(state_dir.join("identity")).ok()?;
    text.lines()
        .find_map(|line| line.strip_prefix(&format!("{name}=")))
        .map(str::to_owned)
}

#[tokio::test(flavor = "multi_thread")]
async fn fleet_enrolments_wait_for_approval_and_rejection_is_told() {
    let dir = tempfile::tempdir().unwrap();
    let server = server(dir.path()).await;
    let http = Http {
        addr: server.rest_addr,
    };
    let token = "fleet-token-of-the-approval-e2e-test";
    put_token(
        http,
        "fleet",
        token,
        json!({ "max_uses": 2, "approval_required": true }),
    )
    .await;
    let tokens = http.json("GET", "/v1/enrolment-tokens", None).await;
    assert_eq!(tokens["tokens"][0]["approval_required"], true);

    let (approved_dir, rejected_dir) = (dir.path().join("a"), dir.path().join("b"));
    let (stop_a, agent_a) = run_agent(agent_config_with(
        &server,
        &approved_dir,
        token,
        "name = \"a\"",
    ));
    let (stop_b, agent_b) = run_agent(agent_config_with(
        &server,
        &rejected_dir,
        token,
        "name = \"b\"",
    ));
    let enrolments = wait_enrolments(http, "fleet", 2).await;
    let id_of = |name: &str| {
        enrolments.iter().find(|e| e["name"] == name).unwrap()["id"]
            .as_str()
            .unwrap()
            .to_owned()
    };
    let (a, b) = (id_of("a"), id_of("b"));
    for enrolment in &enrolments {
        assert_eq!(enrolment["state"], "pending");
        assert_eq!(enrolment["fleet"], true);
        assert_eq!(enrolment["approval_required"], true);
    }

    // Not approved: the central cannot give it a node.
    let assign = json!({ "node_id": "51" });
    let (status, answer) = post(http, &format!("/v1/enrolments/{a}/assign"), Some(&assign)).await;
    assert_eq!(status, 409, "{answer}");
    assert!(node_of(http, "51").await.is_none());

    let (status, _) = post(http, &format!("/v1/enrolments/{a}/approve"), None).await;
    assert_eq!(status, 204);
    let (status, _) = post(http, &format!("/v1/enrolments/{a}/approve"), None).await;
    assert_eq!(status, 204, "approving twice is harmless");
    let (status, _) = post(http, &format!("/v1/enrolments/{a}/assign"), Some(&assign)).await;
    assert_eq!(status, 200);
    wait_node_connected(http, "51").await;
    echo_succeeds_on(http, "51").await;

    // Rejected: no node, the agent learns it and keeps quiet.
    let (status, _) = post(http, &format!("/v1/enrolments/{b}/reject"), None).await;
    assert_eq!(status, 204);
    let (status, _) = post(http, &format!("/v1/enrolments/{b}/approve"), None).await;
    assert_eq!(status, 409);
    tokio::time::sleep(Duration::from_millis(500)).await;
    let rejected = enrolments_of(http, "fleet").await;
    let rejected = rejected.iter().find(|e| e["id"] == b.as_str()).unwrap();
    assert_eq!(rejected["state"], "rejected");
    assert_eq!(rejected["node_id"], Value::Null);
    assert_eq!(
        identity_field(&rejected_dir, "node_id").as_deref(),
        Some("")
    );
    assert!(
        !agent_b.is_finished(),
        "a rejected agent waits, it does not exit"
    );

    stop_a.send(()).unwrap();
    stop_b.send(()).unwrap();
    agent_a.await.unwrap();
    agent_b.await.unwrap();
    server.shutdown().await;
}

/// A gRPC client presenting `credential` for `NODE`.
async fn rotate_as(
    server: &ServerHandle,
    credential: &str,
    new: &str,
) -> Result<bool, tonic::Code> {
    use gorgone_proto::agent::RotateCredentialRequest;
    use gorgone_proto::agent::agent_gateway_client::AgentGatewayClient;
    let channel = tonic::transport::Endpoint::from_shared(format!("http://{}", server.agent_addr))
        .unwrap()
        .connect()
        .await
        .unwrap();
    let mut request = tonic::Request::new(RotateCredentialRequest {
        new_credential: new.to_owned(),
    });
    let metadata = request.metadata_mut();
    metadata.insert(gorgone_proto::NODE_ID_METADATA, NODE.parse().unwrap());
    metadata.insert(
        "authorization",
        format!("Bearer {credential}").parse().unwrap(),
    );
    AgentGatewayClient::new(channel)
        .rotate_credential(request)
        .await
        .map(|answer| answer.into_inner().done)
        .map_err(|status| status.code())
}

async fn wait_rotated(http: Http, state_dir: &Path, former: &str) -> String {
    for _ in 0..200 {
        let current = identity_field(state_dir, "credential").unwrap();
        let rotated = node(http)
            .await
            .is_some_and(|n| !n["credential_rotated_at"].is_null());
        if current != former && identity_field(state_dir, "next_credential").is_none() && rotated {
            return current;
        }
        tokio::time::sleep(Duration::from_millis(50)).await;
    }
    panic!("credential never rotated");
}

#[tokio::test(flavor = "multi_thread")]
async fn credential_rotation_on_request_and_after_each_crash_window() {
    let dir = tempfile::tempdir().unwrap();
    let state_dir = dir.path().join("state");
    let server = server(dir.path()).await;
    let http = Http {
        addr: server.rest_addr,
    };
    put_token(http, "bound", TOKEN, json!({ "node_id": NODE })).await;
    let (stop, agent) = run_agent(agent_config(&server, &state_dir));
    wait_connected(http).await;
    let first = identity_field(&state_dir, "credential").unwrap();

    // On request of the central: the agent rotates, the session comes back
    // with the new credential, the former one is refused.
    let (status, _) = post(http, &format!("/v1/nodes/{NODE}/rotate-credential"), None).await;
    assert_eq!(status, 202);
    let second = wait_rotated(http, &state_dir, &first).await;
    wait_connected(http).await;
    echo_succeeds(http).await;
    assert_eq!(
        rotate_as(&server, &first, &first).await,
        Err(tonic::Code::Unauthenticated)
    );
    assert_eq!(node(http).await.unwrap()["rotation_requested"], false);
    stop.send(()).unwrap();
    agent.await.unwrap();

    // Stopped after writing the new credential, before the server heard of
    // it: the next start finishes the rotation.
    let rewrite = |current: &str, next: &str| {
        std::fs::write(
            state_dir.join("identity"),
            format!("node_id={NODE}\ncredential={current}\nnext_credential={next}\n"),
        )
        .unwrap();
    };
    let third = "third-credential-chosen-before-a-crash-0003";
    rewrite(&second, third);
    let (stop, agent) = run_agent(agent_config(&server, &state_dir));
    assert_eq!(wait_rotated(http, &state_dir, &second).await, third);
    wait_connected(http).await;
    echo_succeeds(http).await;
    stop.send(()).unwrap();
    agent.await.unwrap();

    // Stopped once the server recorded the new one (both accepted).
    let fourth = "fourth-credential-recorded-before-a-crash-04";
    assert_eq!(rotate_as(&server, third, fourth).await, Ok(false));
    rewrite(third, fourth);
    let (stop, agent) = run_agent(agent_config(&server, &state_dir));
    assert_eq!(wait_rotated(http, &state_dir, third).await, fourth);
    assert_eq!(
        rotate_as(&server, third, third).await,
        Err(tonic::Code::Unauthenticated)
    );
    echo_succeeds(http).await;
    stop.send(()).unwrap();
    agent.await.unwrap();

    // Stopped once the server switched, before the file was updated: the
    // former credential is refused, the next start uses the new one.
    let fifth = "fifth-credential-promoted-before-a-crash-005";
    assert_eq!(rotate_as(&server, fourth, fifth).await, Ok(false));
    assert_eq!(rotate_as(&server, fifth, fifth).await, Ok(true));
    rewrite(fourth, fifth);
    let (stop, agent) = run_agent(agent_config(&server, &state_dir));
    assert_eq!(wait_rotated(http, &state_dir, fourth).await, fifth);
    wait_connected(http).await;
    echo_succeeds(http).await;
    stop.send(()).unwrap();
    agent.await.unwrap();
    server.shutdown().await;
}
