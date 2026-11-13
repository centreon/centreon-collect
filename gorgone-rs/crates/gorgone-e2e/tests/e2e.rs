//! Two server replicas on one database and a real agent connected to the
//! first one; every request goes through the second one.

mod common;

use std::net::SocketAddr;
use std::path::Path;
use std::time::Duration;

use common::Http;
use gorgone_agent::AgentConfig;
use gorgone_server::{Config, Server, ServerHandle};
use serde_json::{Value, json};
use tokio::io::AsyncReadExt;
use tokio::sync::oneshot;

const NODE: &str = "7";

async fn replica(database_url: &str) -> ServerHandle {
    let any = SocketAddr::from(([127, 0, 0, 1], 0));
    let config = Config {
        rest_listen: any,
        agent_listen: any,
        peer_listen: any,
        database_url: database_url.to_owned(),
        sweep_interval_ms: 60_000, // delivery must come from the peer hint
        heartbeat_interval_s: 1,
        ..Config::default()
    };
    Server::start(config).await.expect("server starts")
}

fn agent_config(server: &ServerHandle, token: &str, dir: &Path) -> AgentConfig {
    let toml = format!(
        r#"
node_id = "{NODE}"
server_url = "http://{agent}"
token = "{token}"

[reconnect]
min_ms = 50
max_ms = 200

[commands]
allowlist = ['echo \w+', 'false']

[engine]
command_file = "{dir}/centengine.cmd"
allowed_commands = ['SCHEDULE_FORCED_HOST_CHECK']

[targets.engine]
path = "{dir}/etc-engine"
prune = true

[services.engine]
reload = "touch {dir}/engine-reloaded"
"#,
        agent = server.agent_addr,
        dir = dir.display(),
    );
    AgentConfig::from_toml(&toml).expect("agent config")
}

async fn submit(http: Http, action: Value) -> String {
    let reply = http
        .json("POST", &format!("/v1/nodes/{NODE}/jobs"), Some(&action))
        .await;
    reply["id"].as_str().expect("job id").to_owned()
}

async fn wait(http: Http, id: &str) -> Value {
    http.json("GET", &format!("/v1/jobs/{id}?wait=10"), None)
        .await
}

#[tokio::test(flavor = "multi_thread")]
async fn commands_config_and_services_across_replicas() {
    let dir = tempfile::tempdir().unwrap();
    let database_url = format!("sqlite://{}/gorgone.db?mode=rwc", dir.path().display());
    let first = replica(&database_url).await;
    let second = replica(&database_url).await;
    let http = Http {
        addr: second.rest_addr,
    };

    // Enrol the node, then start the agent against the first replica.
    let enrolled = http
        .json(
            "PUT",
            &format!("/v1/nodes/{NODE}"),
            Some(&json!({ "name": "poller" })),
        )
        .await;
    let token = enrolled["token"].as_str().expect("token returned once");

    let fifo = dir.path().join("centengine.cmd");
    let made = std::process::Command::new("mkfifo")
        .arg(&fifo)
        .status()
        .unwrap();
    assert!(made.success());
    let (stop, stopped) = oneshot::channel::<()>();
    let agent = tokio::spawn(gorgone_agent::run(
        agent_config(&first, token, dir.path()),
        async {
            stopped.await.ok();
        },
    ));

    // Legacy route, as centreon-web calls it.
    let reply = http
        .json(
            "POST",
            &format!("/api/nodes/{NODE}/core/action/command"),
            Some(&json!([{ "command": "echo hello", "timeout": 10 }])),
        )
        .await;
    let legacy_token = reply["token"].as_str().expect("legacy token").to_owned();
    let job = wait(http, &legacy_token).await;
    assert_eq!(job["state"], "succeeded", "{job}");
    let logs = http
        .json(
            "GET",
            &format!("/api/nodes/{NODE}/log/{legacy_token}"),
            None,
        )
        .await;
    assert_eq!(logs["message"], "Logs found");
    let codes: Vec<i64> = logs["data"]
        .as_array()
        .unwrap()
        .iter()
        .map(|row| row["code"].as_i64().unwrap())
        .collect();
    assert!(codes.contains(&100) && codes.contains(&2), "{codes:?}");
    assert!(logs.to_string().contains("hello"));

    // The agent allowlist wins over the central.
    let id = submit(
        http,
        json!({ "run_commands": { "commands": [{ "command": "cat /etc/shadow" }] } }),
    )
    .await;
    assert_eq!(wait(http, &id).await["state"], "failed");

    // Configuration push followed by a reload: order is kept.
    let source = dir.path().join("generated");
    tokio::fs::create_dir(&source).await.unwrap();
    tokio::fs::write(source.join("centengine.cfg"), "cfg=1\n")
        .await
        .unwrap();
    let bundle = gorgone_core::bundle::pack(&source).unwrap();
    let (status, pushed) = http
        .request(
            "PUT",
            &format!("/v1/nodes/{NODE}/config/engine"),
            "application/octet-stream",
            &bundle,
        )
        .await;
    assert!((200..300).contains(&status), "{status}");
    let pushed: Value = serde_json::from_slice(&pushed).unwrap();
    let reload = submit(
        http,
        json!({ "service_control": { "service": "engine", "operation": "reload" } }),
    )
    .await;
    assert_eq!(
        wait(http, pushed["id"].as_str().unwrap()).await["state"],
        "succeeded"
    );
    assert_eq!(wait(http, &reload).await["state"], "succeeded");
    assert_eq!(
        tokio::fs::read_to_string(dir.path().join("etc-engine/centengine.cfg"))
            .await
            .unwrap(),
        "cfg=1\n"
    );
    assert!(dir.path().join("engine-reloaded").exists());

    // External command reaches the engine pipe.
    let reader = tokio::spawn({
        let fifo = fifo.clone();
        async move {
            let mut pipe = tokio::fs::File::open(fifo).await.unwrap();
            let mut line = vec![0u8; 256];
            let read = pipe.read(&mut line).await.unwrap();
            String::from_utf8_lossy(&line[..read]).into_owned()
        }
    });
    tokio::time::sleep(Duration::from_millis(200)).await;
    let command = "[1700000000] SCHEDULE_FORCED_HOST_CHECK;host;1700000000";
    let id = submit(
        http,
        json!({ "engine_commands": { "commands": [command] } }),
    )
    .await;
    assert_eq!(wait(http, &id).await["state"], "succeeded");
    assert_eq!(reader.await.unwrap(), format!("{command}\n"));

    // Connection status as the monitoring plugin reads it.
    let status = http.json("GET", "/api/internal/constatus", None).await;
    assert!(status["data"][NODE].is_object(), "{status}");

    stop.send(()).ok();
    agent.await.unwrap().unwrap();
    first.shutdown().await;
    second.shutdown().await;
}
