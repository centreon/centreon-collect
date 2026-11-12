//! Throughput and latency of the full path (REST → store → agent → store →
//! REST), with one replica and one agent. Not a regular test:
//!
//! ```bash
//! cargo test --release -p gorgone-e2e --test bench -- --ignored --nocapture
//! ```
//!
//! `GORGONE_BENCH_JOBS`, `GORGONE_BENCH_CONCURRENCY`, `GORGONE_BENCH_NODES` and
//! `GORGONE_TEST_DATABASE_URL` (MariaDB) change the run.

use std::net::SocketAddr;
use std::sync::Arc;
use std::time::{Duration, Instant};

use gorgone_agent::AgentConfig;
use gorgone_server::{Config, Server};
use serde_json::{Value, json};
use tokio::sync::{Semaphore, oneshot};

fn env_usize(name: &str, default: usize) -> usize {
    std::env::var(name)
        .ok()
        .and_then(|v| v.parse().ok())
        .unwrap_or(default)
}

#[tokio::test(flavor = "multi_thread")]
#[ignore = "benchmark"]
async fn end_to_end_throughput() {
    let jobs = env_usize("GORGONE_BENCH_JOBS", 2000);
    let concurrency = env_usize("GORGONE_BENCH_CONCURRENCY", 64);
    let dir = tempfile::tempdir().unwrap();
    let database_url = std::env::var("GORGONE_TEST_DATABASE_URL")
        .unwrap_or_else(|_| format!("sqlite://{}/bench.db?mode=rwc", dir.path().display()));

    let any = SocketAddr::from(([127, 0, 0, 1], 0));
    let server = Server::start(Config {
        rest_listen: any,
        agent_listen: any,
        peer_listen: any,
        database_url,
        auto_enroll: true,
        ..Config::default()
    })
    .await
    .unwrap();

    let nodes = env_usize("GORGONE_BENCH_NODES", 1);
    let mut agents = Vec::with_capacity(nodes);
    for node in 0..nodes {
        let agent_config = AgentConfig::from_toml(&format!(
            r#"
node_id = "bench{node}"
server_url = "http://{}"
token = "unused"
max_concurrent_jobs = {concurrency}

[commands]
allowlist = ['true']
"#,
            server.agent_addr
        ))
        .unwrap();
        let (stop, stopped) = oneshot::channel::<()>();
        let agent = tokio::spawn(gorgone_agent::run(agent_config, async {
            stopped.await.ok();
        }));
        agents.push((stop, agent));
    }
    tokio::time::sleep(Duration::from_millis(500)).await;

    let http = reqwest::Client::new();
    let base = format!("http://{}", server.rest_addr);
    let permits = Arc::new(Semaphore::new(concurrency));
    let started = Instant::now();
    let mut tasks = Vec::with_capacity(jobs);
    for job in 0..jobs {
        let node = job % nodes;
        let (http, base, permits) = (http.clone(), base.clone(), permits.clone());
        tasks.push(tokio::spawn(async move {
            let _permit = permits.acquire_owned().await.unwrap();
            let begin = Instant::now();
            let reply: Value = http
                .post(format!("{base}/v1/nodes/bench{node}/jobs"))
                .json(&json!({ "run_commands": { "commands": [{ "command": "true" }] } }))
                .send()
                .await
                .unwrap()
                .json()
                .await
                .unwrap();
            let id = reply["id"].as_str().unwrap().to_owned();
            let job: Value = http
                .get(format!("{base}/v1/jobs/{id}?wait=30"))
                .send()
                .await
                .unwrap()
                .json()
                .await
                .unwrap();
            assert_eq!(job["state"], "succeeded", "{job}");
            begin.elapsed()
        }));
    }
    let mut latencies = Vec::with_capacity(jobs);
    for task in tasks {
        latencies.push(task.await.unwrap());
    }
    let elapsed = started.elapsed();
    latencies.sort();
    let pct = |p: f64| latencies[((jobs as f64 * p) as usize).min(jobs - 1)];
    println!(
        "jobs={jobs} nodes={nodes} concurrency={concurrency} elapsed={elapsed:.2?} \
         throughput={:.0} jobs/s p50={:.1?} p95={:.1?} p99={:.1?} max={:.1?}",
        jobs as f64 / elapsed.as_secs_f64(),
        pct(0.50),
        pct(0.95),
        pct(0.99),
        latencies[jobs - 1],
    );

    for (stop, agent) in agents {
        stop.send(()).ok();
        agent.await.unwrap().unwrap();
    }
    server.shutdown().await;
}
