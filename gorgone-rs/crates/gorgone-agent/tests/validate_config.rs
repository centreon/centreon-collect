//! A configuration is checked on the poller, by the engine that will load
//! it: `ValidateConfig` jobs, and the check `ApplyConfig` runs first, through
//! the engine API or through a local command.

mod support;

use std::path::{Path, PathBuf};
use std::time::Duration;

use gorgone_agent::AgentConfig;
use gorgone_core::bundle::{self, STAGING_PREFIX};
use gorgone_proto::agent::job_event::Body;
use gorgone_proto::agent::job_finished::Outcome;
use support::engine::{EngineCall, FakeEngine, INVALID};
use support::{
    Agent, FakeServer, apply_config, base_config, dissect, outcome, path_str, validate_config,
};

const MARKER: &str = ".gorgone-engine.digest";

/// An engine target holding a live configuration.
struct Target {
    dir: tempfile::TempDir,
}

impl Target {
    fn new() -> Self {
        let dir = tempfile::tempdir().unwrap();
        std::fs::write(dir.path().join("centengine.cfg"), "live\n").unwrap();
        std::fs::write(dir.path().join("old.cfg"), "old\n").unwrap();
        Self { dir }
    }

    fn path(&self) -> &Path {
        self.dir.path()
    }

    /// The path the engine is given: the target path with links resolved.
    fn root(&self) -> PathBuf {
        std::fs::canonicalize(self.path()).unwrap()
    }

    /// `[targets.engine]` checked through its main file.
    fn table(&self) -> String {
        format!(
            "[targets.engine]\npath = \"{}\"\nprune = true\nengine_main_file = \"centengine.cfg\"\n",
            path_str(self.path())
        )
    }

    /// Names in the live directory, sorted.
    fn names(&self) -> Vec<String> {
        let mut names: Vec<String> = std::fs::read_dir(self.path())
            .unwrap()
            .map(|entry| entry.unwrap().file_name().into_string().unwrap())
            .collect();
        names.sort();
        names
    }

    fn main_file(&self) -> String {
        std::fs::read_to_string(self.path().join("centengine.cfg")).unwrap()
    }

    /// A bundle whose main file names `hosts.cfg` by its live path, then
    /// holds `extra`.
    fn bundle(&self, hosts: &str, extra: &str) -> (String, Vec<u8>) {
        let src = tempfile::tempdir().unwrap();
        let main = format!("cfg_file={}/hosts.cfg\n{extra}", path_str(self.path()));
        std::fs::write(src.path().join("centengine.cfg"), main).unwrap();
        std::fs::write(src.path().join("hosts.cfg"), hosts).unwrap();
        let blob = bundle::pack(src.path()).unwrap();
        (bundle::digest(&blob), blob)
    }
}

fn grpc_config(server: &FakeServer, engine: &FakeEngine, target: &Target, tail: &str) -> String {
    format!(
        "{}[engine]\ngrpc_url = \"{}\"\ngrpc_timeout_ms = 500\n{tail}\n{}",
        base_config(&format!("http://{}", server.addr)),
        engine.url(),
        target.table(),
    )
}

#[tokio::test]
async fn engine_checks_the_staged_bundle_and_live_files_stay() {
    let target = Target::new();
    let before = target.names();
    let engine = FakeEngine::start().await;
    let mut server = FakeServer::start().await;
    let agent = Agent::start(&grpc_config(&server, &engine, &target, ""));
    let mut session = server.next_session().await;

    let outside = tempfile::tempdir().unwrap();
    let extra = outside.path().join("extra.cfg");
    std::fs::write(&extra, "define command{}\n").unwrap();
    let (digest, blob) = target.bundle(
        "define host{}\n",
        &format!(
            "cfg_file={}\ncfg_dir={}/objects\nresource_file=resource.cfg\nlog_file={}/engine.log\n",
            path_str(&extra),
            path_str(target.path()),
            path_str(target.path()),
        ),
    );
    server.add_blob(&digest, blob.clone());
    session
        .dispatch(validate_config("valid", "engine", &digest, blob.len()))
        .await;
    let events = session.job_events("valid").await;
    // `started`, the result of the check, `finished`: nothing else.
    assert_eq!(events.len(), 3, "{events:?}");
    let (results, progress, finished) = dissect(&events);
    assert_eq!(outcome(&finished), Outcome::Ok, "{finished:?}");
    assert_eq!(
        finished.message,
        format!(
            "engine: configuration valid; 1 path(s) outside {} checked as deployed on this \
             node: {}",
            path_str(target.path()),
            path_str(&extra)
        )
    );
    assert!(progress.is_empty());
    assert_eq!(
        (results[0].index, results[0].command.as_str()),
        (0, "verify engine")
    );
    assert_eq!(results[0].exit_code, 0);
    assert!(
        results[0]
            .stdout
            .ends_with("Total Warnings: 0\nTotal Errors:   0\n"),
        "{results:?}"
    );

    // The engine was shown a main file inside the target path, the only
    // directory it shares with the agent.
    let paths = engine.verify_paths();
    assert_eq!(paths.len(), 1);
    let shown = Path::new(&paths[0]);
    assert_eq!(shown.file_name().unwrap(), "centengine.cfg.gorgone-verify");
    let staging = shown.parent().unwrap();
    assert_eq!(staging.parent().unwrap(), target.root());
    let staging_name = staging.file_name().unwrap().to_str().unwrap();
    assert!(staging_name.starts_with(STAGING_PREFIX), "{staging_name}");
    // Paths inside the target point to the staged files, the others stay.
    assert_eq!(
        engine.verified(),
        [format!(
            "cfg_file={staged}/hosts.cfg\ncfg_file={}\ncfg_dir={staged}/objects\n\
             resource_file=resource.cfg\nlog_file={}/engine.log\n",
            path_str(&extra),
            path_str(target.path()),
            staged = path_str(staging),
        )]
    );

    // A file the engine refuses, found only in the bundle.
    let (bad, blob) = target.bundle(INVALID, "");
    server.add_blob(&bad, blob);
    session
        .dispatch(validate_config("invalid", "engine", &bad, 0))
        .await;
    let (results, _, finished) = dissect(&session.job_events("invalid").await);
    assert_eq!(outcome(&finished), Outcome::Failed);
    assert_eq!(
        finished.message,
        "engine: configuration rejected, 1 error(s)"
    );
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].exit_code, 1);
    assert!(
        results[0].stdout.contains("Error: invalid object in"),
        "{results:?}"
    );

    // An output above `max_output_bytes` (4096 here) keeps its totals, and
    // the exit code does not count the errors.
    let totals = "\nTotal Warnings: 1\nTotal Errors:   3\n";
    engine.set_verdict(1, &format!("{}{totals}", "é".repeat(4000)));
    session
        .dispatch(validate_config("verbose", "engine", &digest, 0))
        .await;
    let (results, _, finished) = dissect(&session.job_events("verbose").await);
    assert_eq!(
        finished.message.split(';').next(),
        Some("engine: configuration rejected, 3 error(s)")
    );
    assert_eq!(results[0].exit_code, 1);
    let kept = (4096 - 1024) / 2;
    assert!(
        results[0]
            .stdout
            .starts_with(&format!("{}\n[output truncated]\n", "é".repeat(kept))),
        "{results:?}"
    );
    assert!(results[0].stdout.ends_with(totals), "{results:?}");

    // No count in the output, then a status of the engine itself.
    engine.set_verdict(1, "Error: unreadable\n");
    session
        .dispatch(validate_config("uncounted", "engine", &bad, 0))
        .await;
    let (_, _, finished) = dissect(&session.job_events("uncounted").await);
    assert_eq!(outcome(&finished), Outcome::Failed);
    assert_eq!(finished.message, "engine: configuration rejected");
    engine.refuse_verification("configuration check exceeded 60 s");
    session
        .dispatch(validate_config("slow", "engine", &bad, 0))
        .await;
    let (results, _, finished) = dissect(&session.job_events("slow").await);
    assert_eq!(outcome(&finished), Outcome::Failed);
    assert_eq!(
        finished.message,
        "engine: configuration check could not run: DeadlineExceeded: configuration check \
         exceeded 60 s"
    );
    assert_eq!(results[0].exit_code, -1);

    // Whatever the outcome: no staging left, nothing applied, nothing changed.
    assert_eq!(target.names(), before);
    assert_eq!(target.main_file(), "live\n");
    assert!(session.next_heartbeat().await.applied.is_empty());
    agent.stop().await.unwrap();
}

#[tokio::test]
async fn apply_config_writes_nothing_unless_the_engine_agrees() {
    let target = Target::new();
    let before = target.names();
    let engine = FakeEngine::start().await;
    let mut server = FakeServer::start().await;
    let agent = Agent::start(&grpc_config(&server, &engine, &target, ""));
    let mut session = server.next_session().await;

    let (bad, blob) = target.bundle(INVALID, "");
    server.add_blob(&bad, blob.clone());
    session
        .dispatch(apply_config("bad", "engine", &bad, blob.len()))
        .await;
    let (results, progress, finished) = dissect(&session.job_events("bad").await);
    assert_eq!(outcome(&finished), Outcome::Failed, "{finished:?}");
    assert_eq!(
        finished.message,
        "engine: configuration rejected, 1 error(s)"
    );
    assert_eq!(
        (results[0].command.as_str(), results[0].exit_code),
        ("verify engine", 1)
    );
    assert!(results[0].stdout.contains("Error: invalid object in"));
    assert!(progress.is_empty());
    assert_eq!(target.names(), before);
    assert_eq!(target.main_file(), "live\n");
    assert!(bundle::applied_digest(target.path(), MARKER).is_none());
    assert!(session.next_heartbeat().await.applied.is_empty());

    let (good, blob) = target.bundle("define host{}\n", "");
    server.add_blob(&good, blob.clone());
    session
        .dispatch(apply_config("good", "engine", &good, blob.len()))
        .await;
    let events = session.job_events("good").await;
    let (results, progress, finished) = dissect(&events);
    assert_eq!(outcome(&finished), Outcome::Ok, "{finished:?}");
    assert_eq!(finished.message, "engine: 2 files written, 1 pruned");
    assert_eq!(
        progress,
        ["engine: configuration valid", finished.message.as_str()]
    );
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].exit_code, 0);
    // Checked first, applied after.
    assert!(
        matches!(events[1].body, Some(Body::CommandResult(_))),
        "{events:?}"
    );
    // The staged files were checked while the live ones were still the old
    // ones, and the bundle was downloaded once for both steps.
    assert_eq!(engine.verify_paths().len(), 2);
    assert_eq!(
        server
            .fetches()
            .iter()
            .filter(|fetch| fetch.digest == good)
            .count(),
        1
    );
    assert_eq!(target.names(), [MARKER, "centengine.cfg", "hosts.cfg"]);
    // The live main file is the one of the bundle, paths unchanged.
    assert_eq!(
        target.main_file(),
        format!("cfg_file={}/hosts.cfg\n", path_str(target.path()))
    );
    assert_eq!(
        bundle::applied_digest(target.path(), MARKER).as_deref(),
        Some(good.as_str())
    );

    // Already applied: the engine is not asked again.
    session
        .dispatch(apply_config("again", "engine", &good, 0))
        .await;
    let (results, _, finished) = dissect(&session.job_events("again").await);
    assert_eq!(finished.message, "engine: configuration unchanged");
    assert!(results.is_empty());
    assert_eq!(engine.verify_paths().len(), 2);
    agent.stop().await.unwrap();
}

#[tokio::test]
async fn silent_engine_or_cancelled_check_applies_nothing_and_leaves_no_staging() {
    let target = Target::new();
    let before = target.names();
    let engine = FakeEngine::start().await;
    let mut server = FakeServer::start().await;
    let agent = Agent::start(&grpc_config(
        &server,
        &engine,
        &target,
        "verify_timeout_ms = 300",
    ));
    let mut session = server.next_session().await;
    let (digest, blob) = target.bundle("define host{}\n", "");
    server.add_blob(&digest, blob);

    // The check has its own timeout, apart from `grpc_timeout_ms`.
    engine.set_delay(Duration::from_secs(30));
    session
        .dispatch(apply_config("silent", "engine", &digest, 0))
        .await;
    let (results, _, finished) = dissect(&session.job_events("silent").await);
    assert_eq!(outcome(&finished), Outcome::Failed, "{finished:?}");
    assert_eq!(
        finished.message,
        "engine: configuration check could not run: no answer from the engine API within 300 ms"
    );
    assert_eq!(results[0].exit_code, -1);
    assert_eq!(target.names(), before);
    agent.stop().await.unwrap();

    let agent = Agent::start(&grpc_config(&server, &engine, &target, ""));
    let mut session = server.next_session().await;
    session
        .dispatch(apply_config("cancelled", "engine", &digest, 0))
        .await;
    // The engine holds the call: the staging directory exists meanwhile.
    let deadline = tokio::time::Instant::now() + support::PATIENCE;
    while engine.verify_paths().len() < 2 {
        assert!(tokio::time::Instant::now() < deadline, "no check in time");
        tokio::time::sleep(Duration::from_millis(10)).await;
    }
    assert_eq!(target.names().len(), before.len() + 1);
    session.cancel("cancelled").await;
    let (_, _, finished) = dissect(&session.job_events("cancelled").await);
    assert_eq!(outcome(&finished), Outcome::Cancelled, "{finished:?}");
    assert_eq!(target.names(), before);
    assert_eq!(target.main_file(), "live\n");
    assert!(bundle::applied_digest(target.path(), MARKER).is_none());
    assert!(matches!(engine.calls()[0], EngineCall::Verify(_)));
    agent.stop().await.unwrap();
}

/// Stand-in for `centengine -v`: one error per `cfg_file` that is missing or
/// holds the word `invalid`, counted on its last line; exits with 1 when
/// there is any.
const CHECKER: &str = r#"
errors=0
echo "Checking $1"
for file in $(sed -n 's/^cfg_file=//p' "$1"); do
    if ! [ -f "$file" ] || grep -q invalid "$file"; then
        echo "Error: $file" >&2
        errors=$((errors + 1))
    fi
done
echo "Total Errors:   $errors"
[ "$errors" -eq 0 ]
"#;

#[tokio::test]
async fn local_command_checks_when_the_engine_api_is_not_used() {
    let target = Target::new();
    let before = target.names();
    // What an agent killed during a check left behind.
    let leftover = target.path().join(format!("{STAGING_PREFIX}dead"));
    std::fs::create_dir_all(leftover.join("objects")).unwrap();
    std::fs::write(leftover.join("objects/hosts.cfg"), "h").unwrap();
    let plain = tempfile::tempdir().unwrap();
    let plain_leftover = plain.path().join(format!("{STAGING_PREFIX}dead"));
    std::fs::create_dir(&plain_leftover).unwrap();

    let tools = tempfile::tempdir().unwrap();
    let script = tools.path().join("check.sh");
    std::fs::write(&script, CHECKER).unwrap();
    let mut server = FakeServer::start().await;
    // Through `sh`: executing a file just written can fail as busy.
    let config = format!(
        "{}[engine]\nverify_command = \"/bin/sh {} {{config}}\"\n{}\
         [targets.plain]\npath = \"{}\"\nprune = true\n",
        base_config(&format!("http://{}", server.addr)),
        path_str(&script),
        target.table(),
        path_str(plain.path()),
    );
    let agent = Agent::start(&config);
    let mut session = server.next_session().await;
    // Swept at start, in every target.
    assert!(!leftover.exists() && !plain_leftover.exists());
    assert_eq!(target.names(), before);

    let (bad, blob) = target.bundle("invalid\n", "cfg_file=/nonexistent/extra.cfg\n");
    server.add_blob(&bad, blob.clone());
    session
        .dispatch(validate_config("bad", "engine", &bad, blob.len()))
        .await;
    let (results, _, finished) = dissect(&session.job_events("bad").await);
    assert_eq!(outcome(&finished), Outcome::Failed, "{finished:?}");
    assert_eq!(
        finished.message,
        format!(
            "engine: configuration rejected, 2 error(s); 1 path(s) outside {} checked as \
             deployed on this node: /nonexistent/extra.cfg",
            path_str(target.path())
        )
    );
    assert_eq!(results[0].exit_code, 1);
    let staged = results[0]
        .stdout
        .strip_prefix("Checking ")
        .and_then(|rest| rest.strip_suffix("/centengine.cfg.gorgone-verify\nTotal Errors:   2\n"))
        .unwrap_or_else(|| panic!("{results:?}"));
    assert_eq!(Path::new(staged).parent().unwrap(), target.root());
    assert_eq!(
        results[0].stderr,
        format!("Error: {staged}/hosts.cfg\nError: /nonexistent/extra.cfg\n")
    );

    session
        .dispatch(apply_config("push-bad", "engine", &bad, 0))
        .await;
    let (_, _, finished) = dissect(&session.job_events("push-bad").await);
    assert_eq!(outcome(&finished), Outcome::Failed);
    assert_eq!(target.names(), before);
    assert_eq!(target.main_file(), "live\n");

    let (good, blob) = target.bundle("define host{}\n", "");
    server.add_blob(&good, blob);
    session
        .dispatch(validate_config("good", "engine", &good, 0))
        .await;
    let (results, _, finished) = dissect(&session.job_events("good").await);
    assert_eq!(outcome(&finished), Outcome::Ok, "{finished:?}");
    assert_eq!(finished.message, "engine: configuration valid");
    assert_eq!(results[0].exit_code, 0);
    assert_eq!(target.names(), before);

    session
        .dispatch(apply_config("push-good", "engine", &good, 0))
        .await;
    let (results, _, finished) = dissect(&session.job_events("push-good").await);
    assert_eq!(outcome(&finished), Outcome::Ok, "{finished:?}");
    assert_eq!(results.len(), 1);
    assert_eq!(target.names(), [MARKER, "centengine.cfg", "hosts.cfg"]);

    // A target without `engine_main_file`: applied unchecked, as before, and
    // nothing to validate. An unknown target is refused as well.
    session
        .dispatch(apply_config("plain", "plain", &bad, 0))
        .await;
    let (results, progress, finished) = dissect(&session.job_events("plain").await);
    assert_eq!(outcome(&finished), Outcome::Ok, "{finished:?}");
    assert!(results.is_empty());
    assert_eq!(progress, ["plain: 2 files written, 0 pruned"]);
    for (id, name) in [("no-main", "plain"), ("unknown", "nowhere")] {
        session.dispatch(validate_config(id, name, &good, 0)).await;
        let (results, _, finished) = dissect(&session.job_events(id).await);
        assert_eq!(outcome(&finished), Outcome::Rejected, "{finished:?}");
        assert!(results.is_empty());
    }
    agent.stop().await.unwrap();
}

#[tokio::test]
async fn validation_keeps_its_place_among_the_ordered_jobs() {
    let target = Target::new();
    let engine = FakeEngine::start().await;
    let mut server = FakeServer::start().await;
    let agent = Agent::start(&grpc_config(&server, &engine, &target, ""));
    let mut session = server.next_session().await;
    let (first, blob) = target.bundle("define host{}\n", "");
    server.add_blob(&first, blob);
    let (second, blob) = target.bundle("define host{}\ndefine service{}\n", "");
    server.add_blob(&second, blob);

    // Dispatched together: the validation must see the first push applied
    // and end before the second one starts.
    engine.set_delay(Duration::from_millis(200));
    session
        .dispatch(apply_config("one", "engine", &first, 0))
        .await;
    session
        .dispatch(validate_config("check", "engine", &second, 0))
        .await;
    session
        .dispatch(apply_config("two", "engine", &second, 0))
        .await;
    session.job_events("two").await;
    let finished: Vec<&str> = session
        .arrivals
        .iter()
        .filter(|event| matches!(event.body, Some(Body::Finished(_))))
        .map(|event| event.job_id.as_str())
        .collect();
    assert_eq!(finished, ["one", "check", "two"]);
    let started: Vec<&str> = session
        .arrivals
        .iter()
        .filter(|event| matches!(event.body, Some(Body::Started(_))))
        .map(|event| event.job_id.as_str())
        .collect();
    assert_eq!(started, ["one", "check", "two"]);
    assert_eq!(engine.verify_paths().len(), 3);
    agent.stop().await.unwrap();
}

#[test]
fn checked_target_needs_a_way_to_check() {
    let base = base_config("http://127.0.0.1:1");
    let target = "[targets.engine]\npath = \"/etc/centreon-engine\"\nengine_main_file = \"centengine.cfg\"\n";
    for (engine, needle) in [
        ("", "requires engine.grpc_url or engine.verify_command"),
        (
            "[engine]\nverify_command = \"/usr/sbin/centengine -v\"\n",
            "must hold {config}",
        ),
        (
            "[engine]\ngrpc_url = \"http://127.0.0.1:1\"\nverify_timeout_ms = 0\n",
            "verify_timeout_ms",
        ),
    ] {
        let error = AgentConfig::from_toml(&format!("{base}{engine}{target}")).unwrap_err();
        assert!(error.to_string().contains(needle), "{error}");
    }
    let unsafe_main = target.replace("centengine.cfg", "../centengine.cfg");
    let error = AgentConfig::from_toml(&format!(
        "{base}[engine]\ngrpc_url = \"http://127.0.0.1:1\"\n{unsafe_main}"
    ))
    .unwrap_err();
    assert!(
        error.to_string().contains("relative to the target"),
        "{error}"
    );
}
