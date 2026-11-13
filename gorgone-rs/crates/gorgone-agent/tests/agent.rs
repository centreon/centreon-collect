//! End-to-end tests: a real agent against a fake gateway.

mod support;

use std::io::Read;
use std::time::{Duration, SystemTime, UNIX_EPOCH};

use gorgone_core::bundle;
use gorgone_proto::agent::job_event::Body;
use gorgone_proto::agent::job_finished::Outcome;
use gorgone_proto::agent::service_control::Operation;
use nix::sys::stat::Mode;
use support::{
    Agent, FakeServer, apply_config, base_config, command, dissect, engine_commands, outcome,
    path_str, run_commands, service_control,
};
use tonic::transport::{Certificate, Identity, ServerTlsConfig};

fn clear_text(server: &FakeServer) -> String {
    base_config(&format!("http://{}", server.addr))
}

fn bundle_of(files: &[(&str, &str)]) -> (String, Vec<u8>) {
    let src = tempfile::tempdir().unwrap();
    for (name, content) in files {
        std::fs::write(src.path().join(name), content).unwrap();
    }
    let blob = bundle::pack(src.path()).unwrap();
    (bundle::digest(&blob), blob)
}

#[tokio::test]
async fn handshake_carries_identity_and_heartbeats_follow() {
    let mut server = FakeServer::start().await;
    let agent = Agent::start(&clear_text(&server));
    let mut session = server.next_session().await;

    assert_eq!(session.hello.node_id, "7");
    assert_eq!(session.hello.agent_version, env!("CARGO_PKG_VERSION"));
    assert_eq!(
        session.hello.capabilities,
        [
            "run_commands",
            "engine_commands",
            "apply_config",
            "service_control"
        ]
    );
    assert_eq!(session.authorization.as_deref(), Some("Bearer test-token"));
    assert_eq!(session.node_id.as_deref(), Some("7"));
    // The fake server asks for one heartbeat per second.
    assert_eq!(session.next_heartbeat().await.running_jobs, 0);
    assert_eq!(session.next_heartbeat().await.running_jobs, 0);
    agent.stop().await.unwrap();
}

#[tokio::test]
async fn allowlisted_command_runs_and_stdout_is_captured() {
    let mut server = FakeServer::start().await;
    let config = clear_text(&server) + "[commands]\nallowlist = ['echo .*']\n";
    let agent = Agent::start(&config);
    let mut session = server.next_session().await;

    // Without a shell the `;` is an argument like any other.
    session
        .dispatch(run_commands("j1", vec![command("echo hello; echo $HOME")]))
        .await;
    let (results, _, finished) = dissect(&session.job_events("j1").await);
    assert_eq!(outcome(&finished), Outcome::Ok);
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].command, "echo hello; echo $HOME");
    assert_eq!(results[0].stdout, "hello; echo $HOME\n");
    assert_eq!((results[0].exit_code, results[0].timed_out), (0, false));
    agent.stop().await.unwrap();
}

#[tokio::test]
async fn prerequisites_of_a_job_are_left_to_the_server() {
    let mut server = FakeServer::start().await;
    let config = format!(
        "{}[commands]\nallowlist = ['/bin/echo .*']\n",
        clear_text(&server)
    );
    let agent = Agent::start(&config);
    let mut session = server.next_session().await;

    // Jobs this agent never saw: the server dispatched, so they succeeded.
    let mut job = run_commands("after", vec![command("/bin/echo done")]);
    job.requires = vec!["never-seen".to_owned(), "after".to_owned()];
    session.dispatch(job).await;
    let (results, _, finished) = dissect(&session.job_events("after").await);
    assert_eq!(outcome(&finished), Outcome::Ok, "{finished:?}");
    assert_eq!(results[0].stdout, "done\n");
    agent.stop().await.unwrap();
}

#[tokio::test]
async fn command_outside_allowlist_is_rejected_and_nothing_runs() {
    let dir = tempfile::tempdir().unwrap();
    let allowed = dir.path().join("allowed");
    let refused = dir.path().join("refused");
    let mut server = FakeServer::start().await;
    let config = format!(
        "{}[commands]\nallowlist = ['touch {}']\n",
        clear_text(&server),
        path_str(&allowed)
    );
    let agent = Agent::start(&config);
    let mut session = server.next_session().await;

    let job = run_commands(
        "j1",
        vec![
            command(&format!("touch {}", path_str(&allowed))),
            command(&format!("touch {}", path_str(&refused))),
        ],
    );
    session.dispatch(job).await;
    let (results, _, finished) = dissect(&session.job_events("j1").await);
    assert_eq!(outcome(&finished), Outcome::Rejected);
    assert!(results.is_empty());
    assert!(!allowed.exists() && !refused.exists());
    agent.stop().await.unwrap();
}

#[tokio::test]
async fn allowlist_cannot_be_escaped_with_dot_dot_or_a_newline() {
    let dir = tempfile::tempdir().unwrap();
    let plugins = dir.path().join("plugins");
    std::fs::create_dir(&plugins).unwrap();
    let marker = dir.path().join("marker");
    let mut server = FakeServer::start().await;
    // The kind of pattern an administrator writes for a plugin directory.
    let config = format!(
        "{}[commands]\nallowlist = ['{}/\\S+( .*)?', 'echo .*']\n",
        clear_text(&server),
        path_str(&plugins)
    );
    let agent = Agent::start(&config);
    let mut session = server.next_session().await;

    let depth = plugins.components().count();
    let escape = format!(
        "{}/{}bin/sh -c 'touch {}'",
        path_str(&plugins),
        "../".repeat(depth),
        path_str(&marker)
    );
    let second_line = format!("echo ok\ntouch {}", path_str(&marker));
    for (id, line, needle) in [
        ("escape", escape.as_str(), "program path"),
        ("newline", second_line.as_str(), "control character"),
    ] {
        session
            .dispatch(run_commands(id, vec![command(line)]))
            .await;
        let (results, _, finished) = dissect(&session.job_events(id).await);
        assert_eq!(outcome(&finished), Outcome::Rejected, "{finished:?}");
        assert!(finished.message.contains(needle), "{finished:?}");
        assert!(results.is_empty());
    }
    assert!(!marker.exists());
    agent.stop().await.unwrap();
}

#[tokio::test]
async fn timeout_kills_the_process() {
    let dir = tempfile::tempdir().unwrap();
    let pid_file = dir.path().join("pid");
    let mut server = FakeServer::start().await;
    let config = clear_text(&server) + "[commands]\nallowlist = ['.*']\nshell = true\n";
    let agent = Agent::start(&config);
    let mut session = server.next_session().await;

    let mut slow = command(&format!("echo $$ > {}; exec sleep 60", path_str(&pid_file)));
    slow.timeout_s = 1;
    session.dispatch(run_commands("j1", vec![slow])).await;
    let (results, _, finished) = dissect(&session.job_events("j1").await);
    assert_eq!(outcome(&finished), Outcome::Timeout);
    assert!(results[0].timed_out);
    assert!(results[0].duration_ms < 15_000);

    let pid: i32 = std::fs::read_to_string(&pid_file)
        .unwrap()
        .trim()
        .parse()
        .unwrap();
    let alive = nix::sys::signal::kill(nix::unistd::Pid::from_raw(pid), None).is_ok();
    assert!(!alive, "process {pid} survived its timeout");
    agent.stop().await.unwrap();
}

#[tokio::test]
async fn continue_on_error_decides_whether_the_next_command_runs() {
    let mut server = FakeServer::start().await;
    let config = clear_text(&server) + "[commands]\nallowlist = ['false', 'echo .*']\n";
    let agent = Agent::start(&config);
    let mut session = server.next_session().await;

    session
        .dispatch(run_commands(
            "stop",
            vec![command("false"), command("echo next")],
        ))
        .await;
    let (results, _, finished) = dissect(&session.job_events("stop").await);
    assert_eq!(outcome(&finished), Outcome::Failed);
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].exit_code, 1);

    let mut tolerant = command("false");
    tolerant.continue_on_error = true;
    session
        .dispatch(run_commands(
            "continue",
            vec![tolerant, command("echo next")],
        ))
        .await;
    let (results, _, finished) = dissect(&session.job_events("continue").await);
    assert_eq!(outcome(&finished), Outcome::Failed);
    assert_eq!(results.len(), 2);
    assert_eq!(
        (results[1].index, results[1].stdout.as_str()),
        (1, "next\n")
    );
    agent.stop().await.unwrap();
}

#[tokio::test]
async fn engine_commands_reach_the_fifo_and_fail_without_reader() {
    let dir = tempfile::tempdir().unwrap();
    let fifo = dir.path().join("centengine.cmd");
    nix::unistd::mkfifo(&fifo, Mode::from_bits_truncate(0o600)).unwrap();
    let mut server = FakeServer::start().await;
    let config = format!(
        "{}[engine]\ncommand_file = \"{}\"\nallowed_commands = ['RESTART_PROGRAM', 'SCHEDULE_FORCED_(HOST|SVC)_CHECK']\n",
        clear_text(&server),
        path_str(&fifo)
    );
    let agent = Agent::start(&config);
    let mut session = server.next_session().await;

    // No reader: a stopped engine must give an error, not a hang.
    session
        .dispatch(engine_commands("dead", &["[1] RESTART_PROGRAM"]))
        .await;
    let (_, _, finished) = dissect(&session.job_events("dead").await);
    assert_eq!(outcome(&finished), Outcome::Failed);
    assert!(
        finished.message.contains("no process reads"),
        "{finished:?}"
    );

    // Read-write open never blocks on Linux and makes this test the reader.
    let mut engine = std::fs::OpenOptions::new()
        .read(true)
        .write(true)
        .open(&fifo)
        .unwrap();
    let lines = ["[1] RESTART_PROGRAM", "[2] SCHEDULE_FORCED_HOST_CHECK;h;2"];
    session.dispatch(engine_commands("alive", &lines)).await;
    let (_, _, finished) = dissect(&session.job_events("alive").await);
    assert_eq!(outcome(&finished), Outcome::Ok, "{finished:?}");
    assert!(finished.message.starts_with("2 commands"), "{finished:?}");

    // Refused jobs write nothing, not even their allowed lines.
    for (id, lines) in [
        (
            "not-listed",
            ["[3] RESTART_PROGRAM", "[3] PROCESS_FILE;/tmp/x;1"],
        ),
        (
            "injected",
            [
                "[3] RESTART_PROGRAM",
                "[3] RESTART_PROGRAM;\n[3] PROCESS_FILE;/tmp/x;1",
            ],
        ),
    ] {
        session.dispatch(engine_commands(id, &lines)).await;
        let (_, _, finished) = dissect(&session.job_events(id).await);
        assert_eq!(outcome(&finished), Outcome::Rejected, "{finished:?}");
        assert!(finished.message.starts_with("command 1: "), "{finished:?}");
    }
    session
        .dispatch(engine_commands("last", &["[4] RESTART_PROGRAM"]))
        .await;
    let (_, _, finished) = dissect(&session.job_events("last").await);
    assert_eq!(outcome(&finished), Outcome::Ok, "{finished:?}");

    let expected = "[1] RESTART_PROGRAM\n[2] SCHEDULE_FORCED_HOST_CHECK;h;2\n[4] RESTART_PROGRAM\n";
    let mut received = vec![0_u8; expected.len()];
    tokio::task::spawn_blocking(move || {
        engine.read_exact(&mut received).unwrap();
        assert_eq!(String::from_utf8(received).unwrap(), expected);
    })
    .await
    .unwrap();
    agent.stop().await.unwrap();
}

#[tokio::test]
async fn apply_config_round_trip() {
    let dest = tempfile::tempdir().unwrap();
    std::fs::write(dest.path().join("stale.cfg"), "old").unwrap();
    let mut server = FakeServer::start().await;
    let config = format!(
        "{}[targets.engine]\npath = \"{}\"\nprune = true\n",
        clear_text(&server),
        path_str(dest.path())
    );
    let agent = Agent::start(&config);
    let mut session = server.next_session().await;

    let (digest, blob) = bundle_of(&[
        ("centengine.cfg", "cfg_file=hosts.cfg\n"),
        ("hosts.cfg", "h\n"),
    ]);
    let size = blob.len();
    assert!(size > 64, "the bundle must span several chunks");
    server.add_blob(&digest, blob.clone());

    session
        .dispatch(apply_config("first", "engine", &digest, size))
        .await;
    let (_, progress, finished) = dissect(&session.job_events("first").await);
    assert_eq!(outcome(&finished), Outcome::Ok, "{finished:?}");
    assert_eq!(finished.message, "engine: 2 files written, 1 pruned");
    assert_eq!(progress, [finished.message.as_str()]);
    assert_eq!(
        std::fs::read_to_string(dest.path().join("centengine.cfg")).unwrap(),
        "cfg_file=hosts.cfg\n"
    );
    assert!(dest.path().join("hosts.cfg").exists());
    assert!(!dest.path().join("stale.cfg").exists());

    let fetches = server.fetches();
    assert_eq!(fetches.len(), 1);
    assert_eq!(fetches[0].digest, digest);
    assert_eq!(fetches[0].node_id.as_deref(), Some("7"));
    assert_eq!(
        fetches[0].authorization.as_deref(),
        Some("Bearer test-token")
    );

    // Same digest again: nothing downloaded, nothing written.
    session
        .dispatch(apply_config("second", "engine", &digest, size))
        .await;
    let (_, progress, finished) = dissect(&session.job_events("second").await);
    assert_eq!(outcome(&finished), Outcome::Ok);
    assert_eq!(finished.message, "engine: configuration unchanged");
    assert_eq!(progress, [finished.message.as_str()]);
    assert_eq!(server.fetches().len(), 1);

    // The server serves bytes that do not hash to the requested digest.
    let wrong = "0".repeat(64);
    server.add_blob(&wrong, blob);
    session
        .dispatch(apply_config("wrong", "engine", &wrong, size))
        .await;
    let (_, _, finished) = dissect(&session.job_events("wrong").await);
    assert_eq!(outcome(&finished), Outcome::Failed);
    assert!(finished.message.contains("digest mismatch"), "{finished:?}");
    assert_eq!(
        bundle::applied_digest(dest.path(), ".gorgone-engine.digest").as_deref(),
        Some(digest.as_str())
    );

    session
        .dispatch(apply_config("unknown", "nowhere", &digest, size))
        .await;
    let (_, _, finished) = dissect(&session.job_events("unknown").await);
    assert_eq!(outcome(&finished), Outcome::Rejected);
    agent.stop().await.unwrap();
}

#[tokio::test]
async fn service_control_runs_the_mapped_command_only() {
    let dir = tempfile::tempdir().unwrap();
    let reloaded = dir.path().join("reloaded");
    let mut server = FakeServer::start().await;
    let config = format!(
        "{}[services.engine]\nreload = \"touch {}\"\n",
        clear_text(&server),
        path_str(&reloaded)
    );
    let agent = Agent::start(&config);
    let mut session = server.next_session().await;

    session
        .dispatch(service_control("reload", "engine", Operation::Reload))
        .await;
    let (results, _, finished) = dissect(&session.job_events("reload").await);
    assert_eq!(outcome(&finished), Outcome::Ok, "{finished:?}");
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].exit_code, 0);
    assert!(reloaded.exists());

    for (id, service, operation) in [
        ("unknown-service", "cbd", Operation::Reload),
        ("unmapped-operation", "engine", Operation::Restart),
        ("no-operation", "engine", Operation::Unspecified),
    ] {
        session
            .dispatch(service_control(id, service, operation))
            .await;
        let (results, _, finished) = dissect(&session.job_events(id).await);
        assert_eq!(outcome(&finished), Outcome::Rejected, "{id}");
        assert!(results.is_empty());
    }
    agent.stop().await.unwrap();
}

#[tokio::test]
async fn reconnection_resends_unacknowledged_events_without_running_twice() {
    let runs = tempfile::tempdir().unwrap();
    let mut server = FakeServer::start().await;
    let config = clear_text(&server) + "[commands]\nallowlist = ['mktemp -p \\S+', 'sleep 1']\n";
    let agent = Agent::start(&config);
    // Each execution leaves one new file in `runs`.
    let once = || {
        run_commands(
            "once",
            vec![command(&format!("mktemp -p {}", path_str(runs.path())))],
        )
    };
    let executions = || std::fs::read_dir(runs.path()).unwrap().count();

    // First session: the events arrive but the server "crashes" before acking.
    let mut first = server.next_session().await;
    first.auto_ack = false;
    first.dispatch(once()).await;
    let original = first.job_events("once").await;
    dissect(&original);
    assert_eq!(original.len(), 3);
    drop(first);

    // Second session: the same events come back by themselves.
    let mut second = server.next_session().await;
    second.auto_ack = false;
    assert_eq!(second.job_events("once").await, original);

    // The server dispatches the job again: replayed, not executed.
    second.dispatch(once()).await;
    assert_eq!(second.job_events("once").await, original);
    assert_eq!(executions(), 1);
    for event in &original {
        second.ack(event).await;
    }

    // Duplicate dispatch of a running job: ignored.
    second.auto_ack = true;
    let slow = || run_commands("slow", vec![command("sleep 1")]);
    second.dispatch(slow()).await;
    let started = second.next_event().await;
    assert!(matches!(started.body, Some(Body::Started(_))));
    second.dispatch(slow()).await;
    let mut events = vec![started];
    events.extend(second.job_events("slow").await);
    let (results, _, finished) = dissect(&events);
    assert_eq!(outcome(&finished), Outcome::Ok);
    assert_eq!(results.len(), 1);
    second.expect_no_event(Duration::from_millis(300)).await;
    drop(second);

    // Everything was acknowledged: nothing to resend.
    let mut third = server.next_session().await;
    third.expect_no_event(Duration::from_millis(500)).await;
    assert_eq!(executions(), 1);
    agent.stop().await.unwrap();
}

#[tokio::test]
async fn service_control_never_overtakes_the_config_dispatched_before() {
    let dest = tempfile::tempdir().unwrap();
    let mut server = FakeServer::start().await;
    let config = format!(
        "{}[targets.engine]\npath = \"{dest}\"\n[services.engine]\nreload = \"cat {dest}/centengine.cfg\"\n",
        clear_text(&server),
        dest = path_str(dest.path())
    );
    let agent = Agent::start(&config);
    let mut session = server.next_session().await;

    let (digest, blob) = bundle_of(&[("centengine.cfg", "version=2\n")]);
    let size = blob.len();
    server.add_blob(&digest, blob);
    // A slow download gives the reload every chance to run first.
    server.set_fetch_delay(Duration::from_millis(500));

    session
        .dispatch(apply_config("push", "engine", &digest, size))
        .await;
    session
        .dispatch(service_control("reload", "engine", Operation::Reload))
        .await;
    let reload = session.job_events("reload").await;
    let (results, _, finished) = dissect(&reload);
    assert_eq!(outcome(&finished), Outcome::Ok, "{results:?}");
    // The "reload" read the file the push wrote.
    assert_eq!(results[0].stdout, "version=2\n");

    let position = |job: &str, terminal: bool| {
        session
            .arrivals
            .iter()
            .position(|e| e.job_id == job && terminal == matches!(e.body, Some(Body::Finished(_))))
            .unwrap()
    };
    assert!(position("push", true) < position("reload", false));
    agent.stop().await.unwrap();
}

#[tokio::test]
async fn run_commands_jobs_run_concurrently() {
    let mut server = FakeServer::start().await;
    let config = clear_text(&server) + "[commands]\nallowlist = ['sleep 1']\n";
    let agent = Agent::start(&config);
    let mut session = server.next_session().await;

    session
        .dispatch(run_commands("a", vec![command("sleep 1")]))
        .await;
    session
        .dispatch(run_commands("b", vec![command("sleep 1")]))
        .await;
    // Both start before either finishes.
    for _ in 0..2 {
        let event = session.next_event().await;
        assert!(matches!(event.body, Some(Body::Started(_))), "{event:?}");
    }
    assert_eq!(session.next_heartbeat().await.running_jobs, 2);
    agent.stop().await.unwrap();
}

#[tokio::test]
async fn deadline_passing_during_execution_stops_the_job() {
    let mut server = FakeServer::start().await;
    let config = clear_text(&server) + "[commands]\nallowlist = ['sleep 60', 'echo .*']\n";
    let agent = Agent::start(&config);
    let mut session = server.next_session().await;

    // The command timeout (10 s here) is far beyond the job time to live.
    let mut job = run_commands("deadline", vec![command("sleep 60"), command("echo never")]);
    job.ttl_ms = 1000;
    let started = std::time::Instant::now();
    session.dispatch(job).await;
    let (results, _, finished) = dissect(&session.job_events("deadline").await);
    assert_eq!(outcome(&finished), Outcome::Timeout, "{finished:?}");
    assert!(
        finished.message.contains("during execution"),
        "{finished:?}"
    );
    assert_eq!(results.len(), 1);
    assert_ne!(results[0].exit_code, 0);
    let elapsed = started.elapsed();
    assert!(elapsed >= Duration::from_millis(900), "{elapsed:?}");
    assert!(elapsed < Duration::from_secs(8), "{elapsed:?}");
    agent.stop().await.unwrap();
}

#[tokio::test]
async fn stopped_command_gets_sigterm_then_sigkill_after_the_grace() {
    let mut server = FakeServer::start().await;
    let config = format!(
        "kill_grace_ms = 400\n{}[commands]\nallowlist = ['.*']\nshell = true\n",
        clear_text(&server)
    );
    let agent = Agent::start(&config);
    let mut session = server.next_session().await;

    // Handles SIGTERM: it gets the time to say goodbye.
    let mut polite = command("trap 'echo bye; exit 0' TERM; sleep 60 & wait");
    polite.timeout_s = 1;
    session.dispatch(run_commands("polite", vec![polite])).await;
    let (results, _, finished) = dissect(&session.job_events("polite").await);
    assert_eq!(outcome(&finished), Outcome::Timeout);
    assert!(results[0].timed_out);
    assert_eq!(
        (results[0].exit_code, results[0].stdout.as_str()),
        (0, "bye\n")
    );

    // Ignores SIGTERM: killed once the grace has passed.
    let mut stubborn = command("trap '' TERM; exec sleep 60");
    stubborn.timeout_s = 1;
    session
        .dispatch(run_commands("stubborn", vec![stubborn]))
        .await;
    let (results, _, finished) = dissect(&session.job_events("stubborn").await);
    assert_eq!(outcome(&finished), Outcome::Timeout);
    assert_eq!(results[0].exit_code, 128 + 9);
    assert!(results[0].duration_ms >= 1400, "{:?}", results[0]);
    assert!(results[0].duration_ms < 10_000, "{:?}", results[0]);
    agent.stop().await.unwrap();
}

#[tokio::test]
async fn job_whose_time_to_live_ran_out_in_the_queue_is_not_run() {
    let dir = tempfile::tempdir().unwrap();
    let marker = dir.path().join("marker");
    let mut server = FakeServer::start().await;
    // One job at a time: the second one waits behind the first.
    let config = clear_text(&server).replace("max_concurrent_jobs = 4", "max_concurrent_jobs = 1")
        + "[commands]\nallowlist = ['touch .*', 'sleep 1']\n";
    let agent = Agent::start(&config);
    let mut session = server.next_session().await;

    let touch = command(&format!("touch {}", path_str(&marker)));
    session
        .dispatch(run_commands("busy", vec![command("sleep 1")]))
        .await;
    let mut late = run_commands("late", vec![touch.clone()]);
    late.ttl_ms = 200;
    session.dispatch(late).await;
    let (results, _, finished) = dissect(&session.job_events("late").await);
    assert_eq!(outcome(&finished), Outcome::Timeout);
    assert!(
        finished.message.contains("before the job started"),
        "{finished:?}"
    );
    assert!(results.is_empty());
    assert!(!marker.exists());

    // The wall-clock deadline is informative: a poller with a wrong clock,
    // here one that believes the deadline is long gone, still runs the job.
    let now_ms = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap()
        .as_millis() as i64;
    let mut in_time = run_commands("in-time", vec![touch]);
    in_time.deadline_unix_ms = now_ms - 3_600_000;
    in_time.ttl_ms = 60_000;
    session.dispatch(in_time).await;
    let (_, _, finished) = dissect(&session.job_events("in-time").await);
    assert_eq!(outcome(&finished), Outcome::Ok);
    assert!(marker.exists());
    agent.stop().await.unwrap();
}

#[tokio::test]
async fn cancel_stops_a_running_job() {
    let mut server = FakeServer::start().await;
    let config = clear_text(&server) + "[commands]\nallowlist = ['sleep 60']\n";
    let agent = Agent::start(&config);
    let mut session = server.next_session().await;

    session
        .dispatch(run_commands("long", vec![command("sleep 60")]))
        .await;
    let started = session.next_event().await;
    session.cancel("long").await;
    let mut events = vec![started];
    events.extend(session.job_events("long").await);
    let (results, _, finished) = dissect(&events);
    assert_eq!(outcome(&finished), Outcome::Cancelled);
    assert!(results[0].duration_ms < 15_000);
    agent.stop().await.unwrap();
}

#[tokio::test]
async fn shutdown_lets_the_running_job_finish_and_takes_no_new_one() {
    let mut server = FakeServer::start().await;
    let config = clear_text(&server) + "[commands]\nallowlist = ['sleep 1', 'echo .*']\n";
    let mut agent = Agent::start(&config);
    let mut session = server.next_session().await;

    session
        .dispatch(run_commands("running", vec![command("sleep 1")]))
        .await;
    let started = session.next_event().await;
    assert!(matches!(started.body, Some(Body::Started(_))));
    agent.request_stop();
    tokio::time::sleep(Duration::from_millis(200)).await;
    session
        .dispatch(run_commands("too-late", vec![command("echo no")]))
        .await;

    // The agent closes its stream once the last event is acknowledged.
    let mut events = vec![started];
    while let Some(event) = session.try_next_event().await {
        events.push(event);
    }
    assert!(events.iter().all(|e| e.job_id == "running"), "{events:?}");
    let (results, _, finished) = dissect(&events);
    assert_eq!(outcome(&finished), Outcome::Ok);
    assert_eq!(results[0].exit_code, 0);
    agent.stop().await.unwrap();
}

#[tokio::test]
async fn agent_survives_a_server_that_starts_late() {
    // Reserve a port, then free it: nothing listens when the agent starts.
    let listener = std::net::TcpListener::bind("127.0.0.1:0").unwrap();
    let addr = listener.local_addr().unwrap();
    drop(listener);
    let agent = Agent::start(&base_config(&format!("http://{addr}")));
    tokio::time::sleep(Duration::from_millis(300)).await;
    agent.stop().await.unwrap();
}

#[tokio::test]
async fn invalid_settings_are_fatal() {
    let config = base_config("http://127.0.0.1:1") + "[commands]\nallowlist = ['(']\n";
    let config = gorgone_agent::AgentConfig::from_toml(&config).unwrap();
    let error = gorgone_agent::run(config, std::future::pending())
        .await
        .unwrap_err();
    assert!(matches!(error, gorgone_agent::AgentError::Allowlist(_)));
}

/// PEM armour of a DER document, so that `rcgen` needs no PEM encoder.
fn pem(label: &str, der: &[u8]) -> String {
    const ALPHABET: &[u8; 64] = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    let mut base64 = String::new();
    for chunk in der.chunks(3) {
        let bits = chunk
            .iter()
            .fold(0u32, |bits, byte| bits << 8 | u32::from(*byte))
            << (8 * (3 - chunk.len()));
        for index in 0..4 {
            if index <= chunk.len() {
                let sextet = (bits >> (18 - 6 * index)) & 0x3f;
                base64.push(char::from(ALPHABET[sextet as usize]));
            } else {
                base64.push('=');
            }
        }
    }
    format!("-----BEGIN {label}-----\n{base64}\n-----END {label}-----\n")
}

#[test]
fn pem_armour_matches_the_reference_vectors() {
    // RFC 4648, section 10.
    for (plain, encoded) in [("f", "Zg=="), ("fo", "Zm8="), ("foobar", "Zm9vYmFy")] {
        let expected = format!("-----BEGIN X-----\n{encoded}\n-----END X-----\n");
        assert_eq!(pem("X", plain.as_bytes()), expected);
    }
}

#[tokio::test]
async fn tls_with_private_authority_and_client_certificate() {
    let dir = tempfile::tempdir().unwrap();
    let server_cert = rcgen::generate_simple_self_signed(vec!["gorgone.test".to_owned()]).unwrap();
    let client_cert = rcgen::generate_simple_self_signed(vec!["7".to_owned()]).unwrap();
    let cert_pem = |cert: &rcgen::Certificate| pem("CERTIFICATE", cert.der());
    let key_pem = |key: &rcgen::KeyPair| pem("PRIVATE KEY", &key.serialize_der());
    let write = |name: &str, pem: String| {
        let path = dir.path().join(name);
        std::fs::write(&path, pem).unwrap();
        path
    };
    let ca_file = write("ca.pem", cert_pem(&server_cert.cert));
    let cert_file = write("agent.crt", cert_pem(&client_cert.cert));
    let key_file = write("agent.key", key_pem(&client_cert.signing_key));

    let tls = ServerTlsConfig::new()
        .identity(Identity::from_pem(
            cert_pem(&server_cert.cert),
            key_pem(&server_cert.signing_key),
        ))
        .client_ca_root(Certificate::from_pem(cert_pem(&client_cert.cert)));
    let mut server = FakeServer::start_with(Some(tls)).await;

    let config = format!(
        "{}[tls]\nca_file = \"{}\"\ncert_file = \"{}\"\nkey_file = \"{}\"\nserver_name = \"gorgone.test\"\n[commands]\nallowlist = ['echo .*']\n",
        base_config(&format!("https://{}", server.addr)),
        path_str(&ca_file),
        path_str(&cert_file),
        path_str(&key_file),
    );
    let agent = Agent::start(&config);
    let mut session = server.next_session().await;
    assert_eq!(session.authorization.as_deref(), Some("Bearer test-token"));
    session
        .dispatch(run_commands("tls", vec![command("echo secure")]))
        .await;
    let (results, _, finished) = dissect(&session.job_events("tls").await);
    assert_eq!(outcome(&finished), Outcome::Ok);
    assert_eq!(results[0].stdout, "secure\n");
    agent.stop().await.unwrap();
}
