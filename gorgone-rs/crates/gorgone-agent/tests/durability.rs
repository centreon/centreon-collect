//! Agent restarts with a state directory: nothing lost, nothing run twice.

mod support;

use std::path::Path;
use std::time::Duration;

use gorgone_proto::agent::job_event::Body;
use gorgone_proto::agent::job_finished::Outcome;
use support::{
    Agent, CrashingAgent, FakeServer, base_config, command, dissect, outcome, path_str,
    run_commands,
};

fn durable_config(server: &FakeServer, state_dir: &Path) -> String {
    format!(
        "state_dir = \"{}\"\n{}[commands]\nallowlist = ['mktemp -p \\S+', 'sleep 60']\n",
        path_str(state_dir),
        base_config(&format!("http://{}", server.addr)),
    )
}

#[tokio::test]
async fn restart_resends_unacknowledged_events_and_replays_duplicates() {
    let state = tempfile::tempdir().unwrap();
    let runs = tempfile::tempdir().unwrap();
    let mut server = FakeServer::start().await;
    let config = durable_config(&server, state.path());
    // Each execution leaves one new file in `runs`.
    let once = || {
        run_commands(
            "once",
            vec![command(&format!("mktemp -p {}", path_str(runs.path())))],
        )
    };
    let executions = || std::fs::read_dir(runs.path()).unwrap().count();

    // The job runs, the server never acknowledges, the agent dies.
    let agent = CrashingAgent::start(&config);
    let mut session = server.next_session().await;
    session.auto_ack = false;
    session.dispatch(once()).await;
    let original = session.job_events("once").await;
    dissect(&original);
    assert_eq!(original.len(), 3);
    agent.crash().await;
    drop(session);

    // A new process sends the same events again, by itself.
    let agent = Agent::start(&config);
    let mut session = server.next_session().await;
    session.auto_ack = false;
    assert_eq!(session.job_events("once").await, original);
    // The server dispatches the job again: replayed, not executed.
    session.dispatch(once()).await;
    assert_eq!(session.job_events("once").await, original);
    assert_eq!(executions(), 1);
    for event in &original {
        session.ack(event).await;
    }
    agent.stop().await.unwrap();
    drop(session);

    // Acknowledgements are durable too; the job is still known.
    let agent = Agent::start(&config);
    let mut session = server.next_session().await;
    session.expect_no_event(Duration::from_millis(500)).await;
    session.dispatch(once()).await;
    assert_eq!(session.job_events("once").await, original);
    assert_eq!(executions(), 1);
    agent.stop().await.unwrap();
}

#[tokio::test]
async fn job_running_at_the_crash_is_closed_as_failed_and_never_rerun() {
    let state = tempfile::tempdir().unwrap();
    let runs = tempfile::tempdir().unwrap();
    let mut server = FakeServer::start().await;
    let config = durable_config(&server, state.path());
    let long = || {
        run_commands(
            "long",
            vec![
                command(&format!("mktemp -p {}", path_str(runs.path()))),
                command("sleep 60"),
            ],
        )
    };
    let executions = || std::fs::read_dir(runs.path()).unwrap().count();

    let agent = CrashingAgent::start(&config);
    let mut session = server.next_session().await;
    session.dispatch(long()).await;
    // Wait for the first command: the job is now in its `sleep`.
    loop {
        let event = session.next_event().await;
        if matches!(event.body, Some(Body::CommandResult(_))) {
            break;
        }
    }
    agent.crash().await;
    drop(session);

    let agent = Agent::start(&config);
    let mut session = server.next_session().await;
    // Earlier events may come again if their ack was lost in the crash.
    let events = session.job_events("long").await;
    let closing = events.last().unwrap();
    assert_eq!(closing.seq, 3);
    let Some(Body::Finished(finished)) = &closing.body else {
        panic!("expected a terminal event, got {closing:?}");
    };
    assert_eq!(outcome(finished), Outcome::Failed);
    assert_eq!(finished.message, "agent restarted during execution");

    // The server dispatches it again: the whole history comes back, no run.
    session.dispatch(long()).await;
    let replayed = session.job_events("long").await;
    let (results, _, finished) = dissect(&replayed);
    assert_eq!(outcome(&finished), Outcome::Failed);
    assert_eq!(results.len(), 1);
    session.expect_no_event(Duration::from_millis(300)).await;
    assert_eq!(executions(), 1);
    agent.stop().await.unwrap();
}

#[tokio::test]
async fn unusable_state_directory_is_fatal() {
    let dir = tempfile::tempdir().unwrap();
    let file = dir.path().join("not-a-directory");
    std::fs::write(&file, "").unwrap();
    let config = format!(
        "state_dir = \"{}\"\n{}",
        path_str(&file),
        base_config("http://127.0.0.1:1")
    );
    let config = gorgone_agent::AgentConfig::from_toml(&config).unwrap();
    let error = gorgone_agent::run(config, std::future::pending())
        .await
        .unwrap_err();
    assert!(
        matches!(error, gorgone_agent::AgentError::Io { .. }),
        "{error}"
    );
}

#[tokio::test]
async fn second_agent_on_the_same_state_directory_is_refused() {
    let state = tempfile::tempdir().unwrap();
    let mut server = FakeServer::start().await;
    let config = durable_config(&server, state.path());
    let first = Agent::start(&config);
    server.next_session().await;

    let second = gorgone_agent::AgentConfig::from_toml(&config).unwrap();
    let error = gorgone_agent::run(second, std::future::pending())
        .await
        .unwrap_err();
    assert!(
        matches!(error, gorgone_agent::AgentError::StateDirLocked(_)),
        "{error}"
    );
    assert!(error.to_string().contains("locked by another"), "{error}");

    // The lock goes away with its owner.
    first.stop().await.unwrap();
    Agent::start(&config).stop().await.unwrap();
}
