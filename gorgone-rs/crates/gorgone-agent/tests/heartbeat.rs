//! What a heartbeat tells about the node: applied configuration digests and
//! the state of the engine API.

mod support;

use std::time::Duration;

use gorgone_core::bundle;
use gorgone_proto::agent::job_finished::Outcome;
use gorgone_proto::agent::{AppliedConfig, EngineState};
use support::engine::{FakeEngine, VERSION};
use support::{Agent, FakeServer, apply_config, base_config, dissect, outcome, path_str};

fn bundle_of(content: &str) -> (String, Vec<u8>) {
    let src = tempfile::tempdir().unwrap();
    std::fs::write(src.path().join("centengine.cfg"), content).unwrap();
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
async fn applied_digests_appear_once_a_bundle_is_applied() {
    let engine_dir = tempfile::tempdir().unwrap();
    let broker_dir = tempfile::tempdir().unwrap();
    let mut server = FakeServer::start().await;
    let config = format!(
        "{}[targets.engine]\npath = \"{}\"\n[targets.broker]\npath = \"{}\"\n",
        base_config(&format!("http://{}", server.addr)),
        path_str(engine_dir.path()),
        path_str(broker_dir.path()),
    );
    let agent = Agent::start(&config);
    let mut session = server.next_session().await;

    // Nothing applied yet: no target is listed, and no engine API is set.
    let beat = session.next_heartbeat().await;
    assert_eq!(beat.applied, []);
    assert_eq!(beat.engine, None);

    let (first, blob) = bundle_of("version=1\n");
    server.add_blob(&first, blob.clone());
    session
        .dispatch(apply_config("one", "engine", &first, blob.len()))
        .await;
    let (_, _, finished) = dissect(&session.job_events("one").await);
    assert_eq!(outcome(&finished), Outcome::Ok, "{finished:?}");
    // The broker target, never applied, stays out.
    let beat = session
        .heartbeat_where(|beat| !beat.applied.is_empty())
        .await;
    assert_eq!(beat.applied, [applied("engine", &first)]);

    let (second, blob) = bundle_of("version=2\n");
    server.add_blob(&second, blob.clone());
    session
        .dispatch(apply_config("two", "engine", &second, blob.len()))
        .await;
    session
        .dispatch(apply_config("three", "broker", &first, 0))
        .await;
    session.job_events("three").await;
    let wanted = [applied("broker", &first), applied("engine", &second)];
    session.heartbeat_where(|beat| beat.applied == wanted).await;

    // A failed job leaves what was applied before.
    let unknown = "f".repeat(64);
    session
        .dispatch(apply_config("missing", "engine", &unknown, 0))
        .await;
    let (_, _, finished) = dissect(&session.job_events("missing").await);
    assert_eq!(outcome(&finished), Outcome::Failed, "{finished:?}");
    assert_eq!(session.next_heartbeat().await.applied, wanted);
    agent.stop().await.unwrap();

    // A restarted agent reads the markers back before its first heartbeat.
    let agent = Agent::start(&config);
    let mut session = server.next_session().await;
    assert_eq!(session.next_heartbeat().await.applied, wanted);
    agent.stop().await.unwrap();
}

#[tokio::test]
async fn engine_state_follows_the_engine_api() {
    let dir = tempfile::tempdir().unwrap();
    let socket = dir.path().join("engine.sock");
    let mut engine = FakeEngine::start_unix(&socket);
    let mut server = FakeServer::start().await;
    let config = format!(
        "{}[engine]\ngrpc_url = \"{}\"\ngrpc_timeout_ms = 5000\n",
        base_config(&format!("http://{}", server.addr)),
        engine.url(),
    );
    let agent = Agent::start(&config);
    let mut session = server.next_session().await;

    let up = EngineState {
        reachable: true,
        version: VERSION.to_owned(),
    };
    let down = EngineState::default();
    session
        .heartbeat_where(|beat| beat.engine.as_ref() == Some(&up))
        .await;

    engine.stop().await;
    session
        .heartbeat_where(|beat| beat.engine.as_ref() == Some(&down))
        .await;

    engine.restart();
    session
        .heartbeat_where(|beat| beat.engine.as_ref() == Some(&up))
        .await;

    // A silent engine is reported unreachable and delays no heartbeat: the
    // probe gives up after half an interval, far below `grpc_timeout_ms`.
    engine.set_delay(Duration::from_secs(30));
    session
        .heartbeat_where(|beat| beat.engine.as_ref() == Some(&down))
        .await;
    let started = tokio::time::Instant::now();
    for _ in 0..3 {
        assert_eq!(session.next_heartbeat().await.engine.as_ref(), Some(&down));
    }
    // Three one-second intervals; waiting for `grpc_timeout_ms` would take 15 s.
    assert!(started.elapsed() < Duration::from_secs(6));
    agent.stop().await.unwrap();
}
