//! The agent driving a fake Centreon Engine through its gRPC API
//! (`engine.grpc_url`) instead of the command pipe and of command lines.

mod support;

use std::time::{Duration, Instant};

use gorgone_proto::agent::job_finished::Outcome;
use gorgone_proto::agent::service_control::Operation;
use gorgone_proto::engine::engine_signal_process::Process;
use support::engine::{EngineCall, FakeEngine};
use support::{Agent, FakeServer, base_config, dissect, engine_commands, outcome, service_control};

const ALLOWED: &str = "allowed_commands = ['RESTART_PROGRAM', 'SCHEDULE_FORCED_(HOST|SVC)_CHECK']";

fn config(server: &FakeServer, engine_url: &str, tail: &str) -> String {
    format!(
        "{}[engine]\ngrpc_url = \"{engine_url}\"\ngrpc_timeout_ms = 500\n{ALLOWED}\n{tail}",
        base_config(&format!("http://{}", server.addr)),
    )
}

#[tokio::test]
async fn engine_commands_are_sent_in_order_and_stop_at_the_first_failure() {
    let engine = FakeEngine::start().await;
    let mut server = FakeServer::start().await;
    // A command file that does not exist: nothing may fall back to the pipe.
    let tail = "command_file = \"/nonexistent/centengine.cmd\"\n";
    let agent = Agent::start(&config(&server, &engine.url(), tail));
    let mut session = server.next_session().await;

    let lines = [
        "[1] SCHEDULE_FORCED_HOST_CHECK;h;1",
        "[2] RESTART_PROGRAM",
        "[3] SCHEDULE_FORCED_SVC_CHECK;h;s;3",
    ];
    session.dispatch(engine_commands("all", &lines)).await;
    let (results, _, finished) = dissect(&session.job_events("all").await);
    assert_eq!(outcome(&finished), Outcome::Ok, "{finished:?}");
    assert!(finished.message.starts_with("3 commands"), "{finished:?}");
    assert!(results.is_empty());
    // As received by the job: no newline added, order kept.
    assert_eq!(engine.lines(), lines);

    engine.refuse_commands_holding(";bad;");
    let lines = [
        "[4] SCHEDULE_FORCED_HOST_CHECK;h;4",
        "[5] SCHEDULE_FORCED_SVC_CHECK;h;bad;5",
        "[6] RESTART_PROGRAM",
    ];
    session.dispatch(engine_commands("partial", &lines)).await;
    let (_, _, finished) = dissect(&session.job_events("partial").await);
    assert_eq!(outcome(&finished), Outcome::Failed, "{finished:?}");
    for needle in [
        "command 1: ",
        "InvalidArgument",
        "unknown command [5] SCHEDULE_FORCED_SVC_CHECK;h;bad;5",
        "1 of 3 commands accepted",
    ] {
        assert!(finished.message.contains(needle), "{finished:?}");
    }
    // The line after the failure was never sent.
    assert_eq!(engine.lines().len(), 5);
    assert_eq!(engine.lines()[3..], lines[..2]);
    agent.stop().await.unwrap();
}

#[tokio::test]
async fn refused_job_calls_nothing() {
    let engine = FakeEngine::start().await;
    let mut server = FakeServer::start().await;
    let agent = Agent::start(&config(&server, &engine.url(), ""));
    let mut session = server.next_session().await;

    for (id, line, needle) in [
        (
            "not-listed",
            "[1] PROCESS_FILE;/tmp/x;1",
            "PROCESS_FILE is not in",
        ),
        (
            "injected",
            "[1] RESTART_PROGRAM\n[1] PROCESS_FILE;/tmp/x;1",
            "line break",
        ),
        ("nul", "[1] RESTART_PROGRAM;a\0b", "NUL"),
        (
            "nameless",
            "[x] RESTART_PROGRAM",
            "no external command name",
        ),
    ] {
        // The allowed first line must not be sent either.
        let lines = ["[1] RESTART_PROGRAM", line];
        session.dispatch(engine_commands(id, &lines)).await;
        let (_, _, finished) = dissect(&session.job_events(id).await);
        assert_eq!(outcome(&finished), Outcome::Rejected, "{finished:?}");
        assert!(finished.message.starts_with("command 1: "), "{finished:?}");
        assert!(finished.message.contains(needle), "{finished:?}");
    }
    assert_eq!(engine.calls(), []);
    agent.stop().await.unwrap();
}

#[tokio::test]
async fn silent_or_stopped_engine_fails_the_job_in_bounded_time() {
    let engine = FakeEngine::start().await;
    let mut server = FakeServer::start().await;
    let agent = Agent::start(&config(&server, &engine.url(), ""));
    let mut session = server.next_session().await;

    // Far beyond the 500 ms of `grpc_timeout_ms`.
    engine.set_delay(Duration::from_secs(30));
    let started = Instant::now();
    let lines = ["[1] RESTART_PROGRAM", "[2] RESTART_PROGRAM"];
    session.dispatch(engine_commands("silent", &lines)).await;
    let (_, _, finished) = dissect(&session.job_events("silent").await);
    assert_eq!(outcome(&finished), Outcome::Failed, "{finished:?}");
    assert!(
        finished.message.contains("no answer from the engine API"),
        "{finished:?}"
    );
    assert!(finished.message.contains("0 of 2"), "{finished:?}");
    assert!(started.elapsed() < Duration::from_secs(10));
    assert_eq!(engine.lines(), ["[1] RESTART_PROGRAM"]);
    agent.stop().await.unwrap();

    // Nothing listens on this port any more.
    let listener = tokio::net::TcpListener::bind("127.0.0.1:0").await.unwrap();
    let url = format!("http://{}", listener.local_addr().unwrap());
    drop(listener);
    let tail = "[services.engine]\nrestart = \"grpc:shutdown\"\n";
    let agent = Agent::start(&config(&server, &url, tail));
    let mut session = server.next_session().await;
    let started = Instant::now();
    session
        .dispatch(engine_commands("down", &["[1] RESTART_PROGRAM"]))
        .await;
    let (_, _, finished) = dissect(&session.job_events("down").await);
    assert_eq!(outcome(&finished), Outcome::Failed, "{finished:?}");
    assert!(finished.message.starts_with("command 0: "), "{finished:?}");
    session
        .dispatch(service_control("restart", "engine", Operation::Restart))
        .await;
    let (_, _, finished) = dissect(&session.job_events("restart").await);
    assert_eq!(outcome(&finished), Outcome::Failed, "{finished:?}");
    assert!(finished.message.contains("engine API"), "{finished:?}");
    assert!(started.elapsed() < Duration::from_secs(10));
    agent.stop().await.unwrap();
}

#[tokio::test]
async fn service_operations_map_to_engine_calls_or_command_lines() {
    let dir = tempfile::tempdir().unwrap();
    let stopped = dir.path().join("stopped");
    let engine = FakeEngine::start().await;
    let mut server = FakeServer::start().await;
    let tail = format!(
        "[services.engine]\nreload = \"grpc:reload\"\nrestart = \"grpc:shutdown\"\n\
         stop = \"touch {}\"\n",
        support::path_str(&stopped)
    );
    let agent = Agent::start(&config(&server, &engine.url(), &tail));
    let mut session = server.next_session().await;

    for (id, operation, needle) in [
        ("reload", Operation::Reload, "configuration reload accepted"),
        ("restart", Operation::Restart, "shutdown accepted"),
    ] {
        session
            .dispatch(service_control(id, "engine", operation))
            .await;
        let (results, _, finished) = dissect(&session.job_events(id).await);
        assert_eq!(outcome(&finished), Outcome::Ok, "{finished:?}");
        assert!(finished.message.contains(needle), "{finished:?}");
        // No process was run.
        assert!(results.is_empty());
    }
    assert_eq!(
        engine.calls(),
        [
            EngineCall::Signal(Process::Restart, true),
            EngineCall::Shutdown
        ]
    );

    // A command line in the same table still runs as a process.
    session
        .dispatch(service_control("stop", "engine", Operation::Stop))
        .await;
    let (results, _, finished) = dissect(&session.job_events("stop").await);
    assert_eq!(outcome(&finished), Outcome::Ok, "{finished:?}");
    assert_eq!(results.len(), 1);
    assert!(stopped.exists());

    // Unmapped: the engine API offers no way to start a stopped engine.
    session
        .dispatch(service_control("start", "engine", Operation::Start))
        .await;
    let (_, _, finished) = dissect(&session.job_events("start").await);
    assert_eq!(outcome(&finished), Outcome::Rejected, "{finished:?}");
    assert_eq!(engine.calls().len(), 2);
    agent.stop().await.unwrap();
}
