//! Two server replicas on one database and a real agent connected to the
//! first one; every request goes through the second one.

use std::net::SocketAddr;
use std::path::Path;
use std::time::Duration;

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

struct Api {
    http: reqwest::Client,
    base: String,
}

impl Api {
    fn new(server: &ServerHandle) -> Self {
        Self {
            http: reqwest::Client::new(),
            base: format!("http://{}", server.rest_addr),
        }
    }

    async fn submit(&self, action: Value) -> String {
        let reply: Value = self
            .http
            .post(format!("{}/v1/nodes/{NODE}/jobs", self.base))
            .json(&action)
            .send()
            .await
            .unwrap()
            .json()
            .await
            .unwrap();
        reply["id"].as_str().expect("job id").to_owned()
    }

    async fn wait(&self, id: &str) -> Value {
        self.http
            .get(format!("{}/v1/jobs/{id}?wait=10", self.base))
            .send()
            .await
            .unwrap()
            .json()
            .await
            .unwrap()
    }
}

#[tokio::test(flavor = "multi_thread")]
async fn commands_config_and_services_across_replicas() {
    let dir = tempfile::tempdir().unwrap();
    let database_url = format!("sqlite://{}/gorgone.db?mode=rwc", dir.path().display());
    let first = replica(&database_url).await;
    let second = replica(&database_url).await;
    let api = Api::new(&second);

    // Enrol the node, then start the agent against the first replica.
    let enrolled: Value = api
        .http
        .put(format!("{}/v1/nodes/{NODE}", api.base))
        .json(&json!({ "name": "poller" }))
        .send()
        .await
        .unwrap()
        .json()
        .await
        .unwrap();
    let token = enrolled["token"].as_str().expect("token returned once");

    let fifo = dir.path().join("centengine.cmd");
    nix::unistd::mkfifo(&fifo, nix::sys::stat::Mode::from_bits_truncate(0o600)).unwrap();
    let (stop, stopped) = oneshot::channel::<()>();
    let agent = tokio::spawn(gorgone_agent::run(
        agent_config(&first, token, dir.path()),
        async {
            stopped.await.ok();
        },
    ));

    // Legacy route, as centreon-web calls it.
    let reply: Value = api
        .http
        .post(format!("{}/api/nodes/{NODE}/core/action/command", api.base))
        .json(&json!([{ "command": "echo hello", "timeout": 10 }]))
        .send()
        .await
        .unwrap()
        .json()
        .await
        .unwrap();
    let legacy_token = reply["token"].as_str().expect("legacy token").to_owned();
    let job = api.wait(&legacy_token).await;
    assert_eq!(job["state"], "succeeded", "{job}");
    let logs: Value = api
        .http
        .get(format!("{}/api/nodes/{NODE}/log/{legacy_token}", api.base))
        .send()
        .await
        .unwrap()
        .json()
        .await
        .unwrap();
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
    let id = api
        .submit(json!({ "run_commands": { "commands": [{ "command": "cat /etc/shadow" }] } }))
        .await;
    assert_eq!(api.wait(&id).await["state"], "failed");

    // Configuration push followed by a reload: order is kept.
    let source = dir.path().join("generated");
    tokio::fs::create_dir(&source).await.unwrap();
    tokio::fs::write(source.join("centengine.cfg"), "cfg=1\n")
        .await
        .unwrap();
    let bundle = gorgone_core::bundle::pack(&source).unwrap();
    let pushed: Value = api
        .http
        .put(format!("{}/v1/nodes/{NODE}/config/engine", api.base))
        .body(bundle)
        .send()
        .await
        .unwrap()
        .json()
        .await
        .unwrap();
    let reload = api
        .submit(json!({ "service_control": { "service": "engine", "operation": "reload" } }))
        .await;
    assert_eq!(
        api.wait(pushed["id"].as_str().unwrap()).await["state"],
        "succeeded"
    );
    assert_eq!(api.wait(&reload).await["state"], "succeeded");
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
    let id = api
        .submit(json!({ "engine_commands": { "commands": [command] } }))
        .await;
    assert_eq!(api.wait(&id).await["state"], "succeeded");
    assert_eq!(reader.await.unwrap(), format!("{command}\n"));

    // Connection status as the monitoring plugin reads it.
    let status: Value = api
        .http
        .get(format!("{}/api/internal/constatus", api.base))
        .send()
        .await
        .unwrap()
        .json()
        .await
        .unwrap();
    assert!(status["data"][NODE].is_object(), "{status}");

    stop.send(()).ok();
    agent.await.unwrap().unwrap();
    first.shutdown().await;
    second.shutdown().await;
}
