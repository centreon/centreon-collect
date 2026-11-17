//! One node driven by two real agents, one per component: the engine and
//! its broker in separate containers, as on a central whose broker runs
//! apart from its engine.

mod common;

use std::net::SocketAddr;
use std::os::unix::fs::PermissionsExt;
use std::os::unix::process::ExitStatusExt;
use std::path::Path;
use std::time::{Duration, Instant};

use common::Http;
use gorgone_agent::AgentConfig;
use gorgone_server::{Config, Server, ServerHandle};
use serde_json::{Value, json};
use tokio::sync::oneshot;

const NODE: &str = "1";

async fn server(database_url: &str) -> ServerHandle {
    let any = SocketAddr::from(([127, 0, 0, 1], 0));
    let config = Config {
        rest_listen: any,
        agent_listen: any,
        peer_listen: any,
        database_url: database_url.to_owned(),
        heartbeat_interval_s: 1,
        ..Config::default()
    };
    Server::start(config).await.expect("server starts")
}

/// Agent of `component`; `extra` holds its targets and services.
fn agent_config(server: &ServerHandle, token: &str, component: &str, extra: &str) -> AgentConfig {
    let toml = format!(
        r#"
node_id = "{NODE}"
component = "{component}"
server_url = "http://{agent}"
token = "{token}"

[reconnect]
min_ms = 50
max_ms = 200
{extra}
"#,
        agent = server.agent_addr,
    );
    AgentConfig::from_toml(&toml).expect("agent config")
}

async fn push(http: Http, dir: &Path, target: &str, file: &str, content: &str) -> String {
    let source = tempfile::tempdir_in(dir).unwrap();
    std::fs::write(source.path().join(file), content).unwrap();
    let bundle = gorgone_core::bundle::pack(source.path()).unwrap();
    let (status, pushed) = http
        .request(
            "PUT",
            &format!("/v1/nodes/{NODE}/config/{target}"),
            "application/octet-stream",
            &bundle,
        )
        .await;
    assert!((200..300).contains(&status), "{status}");
    let pushed: Value = serde_json::from_slice(&pushed).unwrap();
    pushed["id"].as_str().unwrap().to_owned()
}

async fn wait(http: Http, id: &str) -> Value {
    http.json("GET", &format!("/v1/jobs/{id}?wait=10"), None)
        .await
}

fn read(path: &Path) -> (String, u32) {
    let mode = std::fs::metadata(path).unwrap().permissions().mode() & 0o777;
    (std::fs::read_to_string(path).unwrap(), mode)
}

#[tokio::test(flavor = "multi_thread")]
async fn engine_and_broker_agents_of_one_node() {
    let dir = tempfile::tempdir().unwrap();
    let root = dir.path();
    let database_url = format!("sqlite://{}/gorgone.db?mode=rwc", root.display());
    let server = server(&database_url).await;
    let http = Http {
        addr: server.rest_addr,
    };
    let enrolled = http
        .json(
            "PUT",
            &format!("/v1/nodes/{NODE}"),
            Some(&json!({ "name": "Central" })),
        )
        .await;
    let token = enrolled["token"].as_str().expect("token returned once");

    // cbd stands in as a process that a SIGHUP ends.
    let mut cbd = std::process::Command::new("sleep")
        .arg("60")
        .spawn()
        .unwrap();
    let pid_file = root.join("cbd.pid");
    std::fs::write(&pid_file, cbd.id().to_string()).unwrap();

    let engine = agent_config(
        &server,
        token,
        "engine",
        &format!(
            r#"
[targets.engine_context]
path = "{root}/engine/etc-engine"
file_mode = "0640"
[targets.broker]
path = "{root}/engine/etc-broker"
"#,
            root = root.display()
        ),
    );
    let broker = agent_config(
        &server,
        token,
        "broker",
        &format!(
            r#"
[targets.engine_context]
path = "{root}/broker/etc-engine"
file_mode = "0640"
[targets.broker]
path = "{root}/broker/etc-broker"
[services.broker]
reload = "signal:HUP:{pid}"
"#,
            root = root.display(),
            pid = pid_file.display()
        ),
    );
    for path in [
        "engine/etc-engine",
        "engine/etc-broker",
        "broker/etc-engine",
        "broker/etc-broker",
    ] {
        std::fs::create_dir_all(root.join(path)).unwrap();
    }
    let mut stops = Vec::new();
    let mut agents = Vec::new();
    for config in [engine, broker] {
        let (stop, stopped) = oneshot::channel::<()>();
        stops.push(stop);
        agents.push(tokio::spawn(gorgone_agent::run(config, async {
            stopped.await.ok();
        })));
    }

    // Both sessions are listed, each with its component.
    let until = Instant::now() + Duration::from_secs(10);
    loop {
        let node = http.json("GET", &format!("/v1/nodes/{NODE}"), None).await;
        let connected: Vec<&Value> = node["components"]
            .as_array()
            .unwrap()
            .iter()
            .filter(|c| c["connected"] == true)
            .map(|c| &c["component"])
            .collect();
        if connected == [&json!("broker"), &json!("engine")] {
            break;
        }
        assert!(Instant::now() < until, "both agents connected: {node}");
        tokio::time::sleep(Duration::from_millis(50)).await;
    }

    // The key and the broker configuration reach both containers.
    let key = push(http, root, "engine_context", "engine-context.json", "{}\n").await;
    let config = push(http, root, "broker", "central-broker.json", "{\"b\":1}\n").await;
    for id in [&key, &config] {
        assert_eq!(wait(http, id).await["state"], "succeeded", "{id}");
        let copy = wait(http, &format!("{id}.broker")).await;
        assert_eq!(
            (&copy["state"], &copy["component"]),
            (&json!("succeeded"), &json!("broker"))
        );
    }
    for side in ["engine", "broker"] {
        assert_eq!(
            read(&root.join(side).join("etc-engine/engine-context.json")),
            ("{}\n".to_owned(), 0o640),
            "{side}"
        );
        assert_eq!(
            read(&root.join(side).join("etc-broker/central-broker.json")).0,
            "{\"b\":1}\n",
            "{side}"
        );
    }

    // The broker reload, after the latest broker push, is the broker
    // agent's: it signals cbd.
    let reply = http
        .json(
            "POST",
            &format!("/v1/nodes/{NODE}/jobs"),
            Some(&json!({
                "service_control": { "service": "broker", "operation": "reload" },
                "requires_latest_config": ["broker"],
            })),
        )
        .await;
    let reload = reply["id"].as_str().unwrap();
    let job = wait(http, reload).await;
    assert_eq!(
        (&job["state"], &job["component"], &job["requires"]),
        (&json!("succeeded"), &json!("broker"), &json!([config]))
    );
    assert_eq!(cbd.wait().unwrap().signal(), Some(1), "SIGHUP");

    // Each component reports what it applied.
    let until = Instant::now() + Duration::from_secs(10);
    loop {
        let node = http.json("GET", &format!("/v1/nodes/{NODE}"), None).await;
        let applied = |component: &str| {
            node["components"]
                .as_array()
                .unwrap()
                .iter()
                .find(|c| c["component"] == component)
                .map(|c| c["applied"].as_object().map_or(0, |a| a.len()))
        };
        if applied("broker") == Some(2) && applied("engine") == Some(2) {
            assert_eq!(node["applied"].as_object().unwrap().len(), 2, "{node}");
            break;
        }
        assert!(Instant::now() < until, "applied digests reported: {node}");
        tokio::time::sleep(Duration::from_millis(100)).await;
    }

    for stop in stops {
        stop.send(()).ok();
    }
    for agent in agents {
        agent.await.unwrap().unwrap();
    }
    server.shutdown().await;
}
