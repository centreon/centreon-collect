//! `ApplyConfig`: fetches a content-addressed bundle and syncs it into the
//! directory the administrator mapped to the target, once the engine has
//! accepted it when the target holds an engine configuration.
//! `ValidateConfig`: the same check alone, the live files are not touched.

use std::collections::BTreeMap;
use std::future::Future;
use std::path::Path;
use std::sync::Arc;

use gorgone_core::bundle::{self, ApplyOptions, ApplyReport};
use gorgone_proto::agent::job_event::Body;
use gorgone_proto::agent::job_finished::Outcome;
use gorgone_proto::agent::{ApplyConfig, CommandResult, JobFinished, ValidateConfig};

use super::verify::{self, ConfigChecker};
use super::{Executor, JobContext, command_result, finished};
use crate::config::TargetConfig;
use crate::node_state::{NodeState, marker_name};

/// Paths outside the target named in a result message, at most.
const OUTSIDE_LISTED: usize = 5;

/// Blob download failures.
#[derive(Debug, thiserror::Error)]
pub(crate) enum FetchError {
    #[error("blob download failed: {0}")]
    Rpc(String),
    #[error("blob larger than the {0} bytes allowed")]
    TooLarge(u64),
}

/// Source of the bundles, the server gateway in production.
pub(crate) trait BlobFetcher: Send + Sync {
    /// Downloads the blob `digest`, failing as soon as it exceeds `limit` bytes.
    fn fetch(
        &self,
        digest: &str,
        limit: u64,
    ) -> impl Future<Output = Result<Vec<u8>, FetchError>> + Send;
}

pub(crate) struct ApplyConfigExecutor<F> {
    pub targets: BTreeMap<String, TargetConfig>,
    pub max_bundle_bytes: u64,
    pub max_unpacked_bytes: u64,
    pub fetcher: F,
    /// Told what each job leaves applied, for the heartbeat.
    pub state: Arc<NodeState>,
    /// Checks the targets having an `engine_main_file`.
    pub checker: Option<ConfigChecker>,
    /// Cap on the checker output kept in the job result.
    pub max_output: usize,
}

/// A bundle the engine accepted, with the sentence saying so.
struct Verified {
    blob: Vec<u8>,
    message: String,
}

fn is_sha256_hex(digest: &str) -> bool {
    digest.len() == 64 && digest.bytes().all(|b| b.is_ascii_hexdigit())
}

fn summary(target: &str, report: ApplyReport) -> String {
    if report.unchanged {
        format!("{target}: configuration unchanged")
    } else {
        format!(
            "{target}: {} files written, {} pruned",
            report.written, report.pruned
        )
    }
}

/// End of a result message telling which paths were checked where they are
/// on this node rather than in the bundle.
fn outside_note(outside: &[String], target_path: &Path) -> String {
    if outside.is_empty() {
        return String::new();
    }
    let mut listed = outside[..outside.len().min(OUTSIDE_LISTED)].join(", ");
    if outside.len() > OUTSIDE_LISTED {
        listed.push_str(", ...");
    }
    format!(
        "; {} path(s) outside {} checked as deployed on this node: {listed}",
        outside.len(),
        target_path.display()
    )
}

impl<F: BlobFetcher> ApplyConfigExecutor<F> {
    fn target(&self, name: &str) -> Result<&TargetConfig, JobFinished> {
        self.targets.get(name).ok_or_else(|| {
            finished(
                Outcome::Rejected,
                format!("target {name:?} is not configured on this agent"),
            )
        })
    }

    /// Downloads the bundle `digest`, of `size` bytes when announced.
    async fn download(
        &self,
        digest: &str,
        size: u64,
        ctx: &JobContext<'_>,
    ) -> Result<Vec<u8>, JobFinished> {
        if size > self.max_bundle_bytes {
            return Err(finished(
                Outcome::Failed,
                format!(
                    "bundle of {size} bytes is above the {} bytes allowed",
                    self.max_bundle_bytes
                ),
            ));
        }
        // A size of 0 means "not announced": only the cap applies then.
        let limit = match size {
            0 => self.max_bundle_bytes,
            size => size,
        };
        let fetched = tokio::select! {
            fetched = self.fetcher.fetch(digest, limit) => fetched,
            () = ctx.cancel.cancelled() => {
                return Err(finished(Outcome::Cancelled, "cancelled during download"));
            }
        };
        let blob = fetched.map_err(|error| finished(Outcome::Failed, error.to_string()))?;
        if size != 0 && blob.len() as u64 != size {
            return Err(finished(
                Outcome::Failed,
                format!("blob is {} bytes, {size} were announced", blob.len()),
            ));
        }
        Ok(blob)
    }

    /// Unpacks `blob` aside and has the engine check it. Emits the result of
    /// the check; the staging directory is gone when this returns.
    async fn check(
        &self,
        target_name: &str,
        target: &TargetConfig,
        main_file: &Path,
        blob: Vec<u8>,
        digest: &str,
        ctx: &JobContext<'_>,
    ) -> Result<Verified, JobFinished> {
        let Some(checker) = &self.checker else {
            // The configuration check at start refuses this.
            return Err(finished(
                Outcome::Rejected,
                "no engine configuration check is configured on this agent",
            ));
        };
        let staged = tokio::task::spawn_blocking({
            let (digest, path, main_file) = (
                digest.to_owned(),
                target.path.clone(),
                main_file.to_path_buf(),
            );
            let max_unpacked = self.max_unpacked_bytes;
            // The staging is returned, not captured: should this job be
            // dropped meanwhile, it is removed once the task ends.
            move || {
                let staged = verify::stage(&blob, &digest, &path, &main_file, max_unpacked);
                (blob, staged)
            }
        })
        .await;
        let (blob, staging) = match staged {
            Ok((blob, Ok(staging))) => (blob, staging),
            Ok((_, Err(error))) => {
                return Err(finished(Outcome::Failed, format!("{target_name}: {error}")));
            }
            Err(error) => {
                return Err(finished(
                    Outcome::Failed,
                    format!("staging task failed: {error}"),
                ));
            }
        };

        let checked = checker
            .check(&staging.verify_file, self.max_output, ctx.cancel)
            .await;
        let note = outside_note(&staging.outside, &target.path);
        // Removed off the runtime threads; the result does not depend on it.
        let _ = tokio::task::spawn_blocking(move || drop(staging)).await;

        let command = format!("verify {target_name}");
        let (result, outcome, message) = match checked {
            Ok(output) => {
                let (result, outcome) = command_result(0, &command, Ok(output));
                let message = match outcome {
                    Outcome::Ok => format!("{target_name}: configuration valid"),
                    Outcome::Timeout => format!("{target_name}: configuration check timed out"),
                    Outcome::Cancelled => format!("{target_name}: configuration check cancelled"),
                    // The exit code only tells valid from invalid.
                    _ => match verify::total_errors(&result.stdout) {
                        Some(errors) => {
                            format!("{target_name}: configuration rejected, {errors} error(s)")
                        }
                        None => format!("{target_name}: configuration rejected"),
                    },
                };
                (result, outcome, message)
            }
            Err(error) => {
                let message = format!("{target_name}: configuration check could not run: {error}");
                let result = CommandResult {
                    command,
                    exit_code: -1,
                    stderr: error,
                    ..CommandResult::default()
                };
                (result, Outcome::Failed, message)
            }
        };
        ctx.sink.emit(Body::CommandResult(result));
        let message = format!("{message}{note}");
        if outcome == Outcome::Ok {
            Ok(Verified { blob, message })
        } else {
            Err(finished(outcome, message))
        }
    }

    /// Runs a `ValidateConfig` action: the bundle is checked as `ApplyConfig`
    /// checks it, and nothing is applied.
    pub(crate) async fn validate(
        &self,
        action: ValidateConfig,
        ctx: &JobContext<'_>,
    ) -> JobFinished {
        let target_name = action.target;
        let target = match self.target(&target_name) {
            Ok(target) => target,
            Err(done) => return done,
        };
        let Some(main_file) = &target.engine_main_file else {
            return finished(
                Outcome::Rejected,
                format!(
                    "target {target_name:?} has no engine_main_file on this agent: \
                     nothing can check it"
                ),
            );
        };
        if !is_sha256_hex(&action.digest) {
            return finished(Outcome::Failed, "digest is not a SHA-256 in hexadecimal");
        }
        let digest = action.digest.to_ascii_lowercase();
        let blob = match self.download(&digest, action.size, ctx).await {
            Ok(blob) => blob,
            Err(done) => return done,
        };
        match self
            .check(&target_name, target, main_file, blob, &digest, ctx)
            .await
        {
            Ok(verified) => finished(Outcome::Ok, verified.message),
            Err(done) => done,
        }
    }
}

impl<F: BlobFetcher> Executor for ApplyConfigExecutor<F> {
    type Action = ApplyConfig;

    async fn execute(&self, action: ApplyConfig, ctx: &JobContext<'_>) -> JobFinished {
        let target_name = action.target;
        let target = match self.target(&target_name) {
            Ok(target) => target,
            Err(done) => return done,
        };
        if !is_sha256_hex(&action.digest) {
            return finished(Outcome::Failed, "digest is not a SHA-256 in hexadecimal");
        }
        let digest = action.digest.to_ascii_lowercase();
        let marker = marker_name(&target_name);

        if bundle::applied_digest(&target.path, &marker).as_deref() == Some(digest.as_str()) {
            let unchanged = ApplyReport {
                written: 0,
                pruned: 0,
                unchanged: true,
            };
            self.state.set_applied(&target_name, Some(digest));
            let message = summary(&target_name, unchanged);
            ctx.progress(message.clone());
            return finished(Outcome::Ok, message);
        }

        let mut blob = match self.download(&digest, action.size, ctx).await {
            Ok(blob) => blob,
            Err(done) => return done,
        };
        // Nothing of the live directory is written before the engine agrees.
        if let Some(main_file) = &target.engine_main_file {
            blob = match self
                .check(&target_name, target, main_file, blob, &digest, ctx)
                .await
            {
                Ok(verified) => {
                    ctx.progress(verified.message);
                    verified.blob
                }
                Err(done) => return done,
            };
        }

        let dest = target.path.clone();
        let prune = target.prune;
        let max_unpacked = self.max_unpacked_bytes;
        let applied = tokio::task::spawn_blocking(move || {
            let options = ApplyOptions {
                marker: &marker,
                prune,
                max_unpacked,
            };
            let report = bundle::apply(&blob, &digest, &dest, &options);
            // Read back rather than assumed: a failed apply may leave either.
            (report, bundle::applied_digest(&dest, &marker))
        })
        .await;
        let applied = applied.map(|(report, now_applied)| {
            self.state.set_applied(&target_name, now_applied);
            report
        });
        match applied {
            Ok(Ok(report)) => {
                let message = summary(&target_name, report);
                ctx.progress(message.clone());
                finished(Outcome::Ok, message)
            }
            Ok(Err(error)) => finished(Outcome::Failed, format!("{target_name}: {error}")),
            Err(error) => finished(Outcome::Failed, format!("apply task failed: {error}")),
        }
    }
}

#[cfg(test)]
mod tests {
    use std::collections::HashMap;
    use std::path::PathBuf;
    use std::sync::atomic::{AtomicUsize, Ordering};
    use std::time::Duration;

    use tokio_util::sync::CancellationToken;

    use super::*;
    use crate::executor::testing::{MemorySink, run};

    #[derive(Default)]
    struct MemoryFetcher {
        blobs: HashMap<String, Vec<u8>>,
        calls: AtomicUsize,
    }

    impl MemoryFetcher {
        fn holding(bundles: &[&(String, Vec<u8>)]) -> Self {
            Self {
                blobs: bundles.iter().map(|bundle| (*bundle).clone()).collect(),
                ..Self::default()
            }
        }
    }

    impl BlobFetcher for &MemoryFetcher {
        async fn fetch(&self, digest: &str, limit: u64) -> Result<Vec<u8>, FetchError> {
            self.calls.fetch_add(1, Ordering::SeqCst);
            let blob = self
                .blobs
                .get(digest)
                .ok_or_else(|| FetchError::Rpc("not found".to_owned()))?;
            if blob.len() as u64 > limit {
                return Err(FetchError::TooLarge(limit));
            }
            Ok(blob.clone())
        }
    }

    fn bundle_of(files: &[(&str, &str)]) -> (String, Vec<u8>) {
        let src = tempfile::tempdir().unwrap();
        for (name, content) in files {
            std::fs::write(src.path().join(name), content).unwrap();
        }
        let blob = bundle::pack(src.path()).unwrap();
        (bundle::digest(&blob), blob)
    }

    fn executor<'a>(
        fetcher: &'a MemoryFetcher,
        dest: &Path,
        prune: bool,
    ) -> ApplyConfigExecutor<&'a MemoryFetcher> {
        let target = TargetConfig {
            path: dest.to_path_buf(),
            prune,
            engine_main_file: None,
        };
        ApplyConfigExecutor {
            targets: BTreeMap::from([("engine".to_owned(), target)]),
            max_bundle_bytes: 1 << 20,
            max_unpacked_bytes: 1 << 20,
            fetcher,
            state: Arc::new(NodeState::load(&BTreeMap::new(), false)),
            checker: None,
            max_output: 4096,
        }
    }

    fn action(digest: &str, size: usize) -> ApplyConfig {
        ApplyConfig {
            target: "engine".to_owned(),
            digest: digest.to_owned(),
            size: size as u64,
        }
    }

    fn validation(digest: &str, size: usize) -> ValidateConfig {
        ValidateConfig {
            target: "engine".to_owned(),
            digest: digest.to_owned(),
            size: size as u64,
        }
    }

    /// Stand-in for `centengine -v`: one error per `cfg_file` that is missing
    /// or holds the word `invalid`, counted on its last line; exits with 1
    /// when there is any. Records the files it was given.
    const CHECKER: &str = r#"
errors=0
echo "Checking $1"
for file in $(sed -n 's/^cfg_file=//p' "$1"); do
    if ! [ -f "$file" ]; then
        echo "Error: cannot read $file"
        errors=$((errors + 1))
    elif grep -q invalid "$file"; then
        echo "Error: invalid object in $file"
        errors=$((errors + 1))
    fi
done
echo "$1" >> "$0.calls"
sleep 0
if [ -e "$0.quiet" ]; then exit 3; fi
echo "Total Warnings: 0"
echo "Total Errors:   $errors"
[ "$errors" -eq 0 ]
"#;

    /// An engine target checked by [`CHECKER`], with a live configuration.
    struct Checked {
        fetcher: MemoryFetcher,
        dest: tempfile::TempDir,
        script: PathBuf,
        _tools: tempfile::TempDir,
    }

    impl Checked {
        fn new() -> Self {
            let tools = tempfile::tempdir().unwrap();
            let script = tools.path().join("check.sh");
            std::fs::write(&script, CHECKER).unwrap();
            let dest = tempfile::tempdir().unwrap();
            std::fs::write(dest.path().join("centengine.cfg"), "live\n").unwrap();
            std::fs::write(dest.path().join("old.cfg"), "old\n").unwrap();
            Self {
                fetcher: MemoryFetcher::default(),
                dest,
                script,
                _tools: tools,
            }
        }

        fn executor(&self, timeout: Duration) -> ApplyConfigExecutor<&MemoryFetcher> {
            let mut exec = executor(&self.fetcher, self.dest.path(), true);
            if let Some(target) = exec.targets.get_mut("engine") {
                target.engine_main_file = Some(PathBuf::from("centengine.cfg"));
            }
            // Through `sh`: executing a file just written can fail as busy.
            let argv = ["/bin/sh", self.script.to_str().unwrap(), "{config}"];
            exec.checker = Some(ConfigChecker::Command {
                argv: argv.map(str::to_owned).to_vec(),
                timeout,
                kill_grace: Duration::from_millis(200),
            });
            exec
        }

        /// Main files the checker was given, in order.
        fn checked_files(&self) -> Vec<String> {
            let calls = format!("{}.calls", self.script.display());
            let text = std::fs::read_to_string(calls).unwrap_or_default();
            text.lines().map(str::to_owned).collect()
        }

        /// Names in the live directory, sorted.
        fn live(&self) -> Vec<String> {
            let mut names: Vec<String> = std::fs::read_dir(self.dest.path())
                .unwrap()
                .map(|entry| entry.unwrap().file_name().into_string().unwrap())
                .collect();
            names.sort();
            names
        }

        fn live_main(&self) -> String {
            std::fs::read_to_string(self.dest.path().join("centengine.cfg")).unwrap()
        }
    }

    /// A bundle whose main file names `hosts.cfg` by its live path.
    fn engine_bundle(live: &Path, hosts: &str, extra: &str) -> (String, Vec<u8>) {
        let main = format!("cfg_file={}/hosts.cfg\n{extra}", live.display());
        bundle_of(&[("centengine.cfg", &main), ("hosts.cfg", hosts)])
    }

    #[tokio::test]
    async fn applies_then_skips_the_download() {
        let (digest, blob) = bundle_of(&[("centengine.cfg", "a=1\n"), ("hosts.cfg", "h\n")]);
        let size = blob.len();
        let fetcher = MemoryFetcher {
            blobs: HashMap::from([(digest.clone(), blob)]),
            ..MemoryFetcher::default()
        };
        let dest = tempfile::tempdir().unwrap();
        std::fs::write(dest.path().join("stale.cfg"), "old").unwrap();
        let exec = executor(&fetcher, dest.path(), true);

        let (done, sink) = run(&exec, action(&digest, size)).await;
        assert_eq!(done.outcome(), Outcome::Ok, "{}", done.message);
        assert_eq!(done.message, "engine: 2 files written, 1 pruned");
        // No `engine_main_file`: nothing is checked, as before.
        assert_eq!(sink.progress(), [done.message.as_str()]);
        assert!(sink.command_results().is_empty());
        assert_eq!(
            std::fs::read_to_string(dest.path().join("centengine.cfg")).unwrap(),
            "a=1\n"
        );
        assert!(!dest.path().join("stale.cfg").exists());
        assert!(dest.path().join(".gorgone-engine.digest").exists());
        let applied = exec.state.applied();
        assert_eq!(applied.len(), 1);
        assert_eq!(applied[0].target, "engine");
        assert_eq!(applied[0].digest, digest);

        let (done, _) = run(&exec, action(&digest.to_uppercase(), size)).await;
        assert_eq!(done.outcome(), Outcome::Ok);
        assert_eq!(done.message, "engine: configuration unchanged");
        assert_eq!(fetcher.calls.load(Ordering::SeqCst), 1);
    }

    #[tokio::test]
    async fn refuses_unknown_target_without_download() {
        let fetcher = MemoryFetcher::default();
        let dest = tempfile::tempdir().unwrap();
        let exec = executor(&fetcher, dest.path(), false);
        let mut unknown = action(&"a".repeat(64), 0);
        unknown.target = "../etc".to_owned();
        let (done, _) = run(&exec, unknown).await;
        assert_eq!(done.outcome(), Outcome::Rejected);
        assert_eq!(fetcher.calls.load(Ordering::SeqCst), 0);
    }

    #[tokio::test]
    async fn fails_on_bad_digest_size_or_content() {
        let (digest, blob) = bundle_of(&[("a.cfg", "a\n")]);
        let size = blob.len();
        let other = "b".repeat(64);
        let fetcher = MemoryFetcher {
            blobs: HashMap::from([(digest.clone(), blob.clone()), (other.clone(), blob)]),
            ..MemoryFetcher::default()
        };
        let dest = tempfile::tempdir().unwrap();
        let exec = executor(&fetcher, dest.path(), false);

        for (action, needle) in [
            (action("not-a-digest", size), "SHA-256"),
            (action(&"c".repeat(64), size), "not found"),
            // Content that does not hash to the requested digest.
            (action(&other, size), "digest mismatch"),
            (action(&digest, size - 1), "larger than"),
            (action(&digest, size + 1), "were announced"),
            (action(&digest, 2 << 20), "above"),
        ] {
            let (done, _) = run(&exec, action).await;
            assert_eq!(done.outcome(), Outcome::Failed, "{needle}");
            assert!(done.message.contains(needle), "{}", done.message);
        }
        assert!(!dest.path().join("a.cfg").exists());
        assert!(exec.state.applied().is_empty());

        // Unannounced size: only the cap applies.
        let (done, _) = run(&exec, action(&digest, 0)).await;
        assert_eq!(done.outcome(), Outcome::Ok, "{}", done.message);
    }

    #[tokio::test]
    async fn validation_checks_the_staged_files_and_leaves_the_live_ones() {
        let mut node = Checked::new();
        let valid = engine_bundle(node.dest.path(), "define host{}\n", "");
        let invalid = engine_bundle(
            node.dest.path(),
            "invalid\n",
            "cfg_file=/nonexistent/extra.cfg\n",
        );
        node.fetcher = MemoryFetcher::holding(&[&valid, &invalid]);
        let exec = node.executor(Duration::from_secs(20));
        let before = node.live();

        let (done, sink) = exec_validate(&exec, validation(&valid.0, valid.1.len())).await;
        assert_eq!(done.outcome(), Outcome::Ok, "{}", done.message);
        assert_eq!(done.message, "engine: configuration valid");
        let results = sink.command_results();
        assert_eq!(results.len(), 1);
        assert_eq!(
            (results[0].index, results[0].command.as_str()),
            (0, "verify engine")
        );
        assert_eq!(results[0].exit_code, 0);
        assert!(results[0].stdout.starts_with("Checking "), "{results:?}");
        assert!(sink.progress().is_empty());

        // The live directory has no hosts.cfg: only the staged one was read,
        // through the derived main file inside the target path.
        let checked = node.checked_files();
        assert_eq!(checked.len(), 1);
        let root = std::fs::canonicalize(node.dest.path()).unwrap();
        let staged = Path::new(&checked[0]);
        assert_eq!(staged.file_name().unwrap(), "centengine.cfg.gorgone-verify");
        let staging = staged.parent().unwrap();
        assert_eq!(staging.parent().unwrap(), root);
        assert!(
            staging
                .file_name()
                .unwrap()
                .to_str()
                .unwrap()
                .starts_with(bundle::STAGING_PREFIX)
        );

        let (done, sink) = exec_validate(&exec, validation(&invalid.0, 0)).await;
        assert_eq!(done.outcome(), Outcome::Failed, "{}", done.message);
        assert_eq!(
            done.message,
            format!(
                "engine: configuration rejected, 2 error(s); 1 path(s) outside {} checked \
                 as deployed on this node: /nonexistent/extra.cfg",
                node.dest.path().display()
            )
        );
        let results = sink.command_results();
        assert_eq!(results.len(), 1);
        assert_eq!(results[0].exit_code, 1);
        assert!(
            results[0].stdout.ends_with("Total Errors:   2\n")
                && results[0].stdout.contains("Error: invalid object in")
                && results[0]
                    .stdout
                    .contains("Error: cannot read /nonexistent/extra.cfg"),
            "{results:?}"
        );

        // Nothing staged is left, nothing live changed, nothing applied.
        assert_eq!(node.live(), before);
        assert_eq!(node.live_main(), "live\n");
        assert!(exec.state.applied().is_empty());
    }

    async fn exec_validate(
        exec: &ApplyConfigExecutor<&MemoryFetcher>,
        action: ValidateConfig,
    ) -> (JobFinished, MemorySink) {
        let sink = MemorySink::default();
        let cancel = CancellationToken::new();
        let ctx = JobContext {
            sink: &sink,
            cancel: &cancel,
        };
        let done = exec.validate(action, &ctx).await;
        (done, sink)
    }

    #[tokio::test]
    async fn validation_is_refused_without_main_file_or_target() {
        let (digest, blob) = bundle_of(&[("centengine.cfg", "a=1\n")]);
        let fetcher = MemoryFetcher::holding(&[&(digest.clone(), blob)]);
        let dest = tempfile::tempdir().unwrap();
        let exec = executor(&fetcher, dest.path(), false);

        let (done, sink) = exec_validate(&exec, validation(&digest, 0)).await;
        assert_eq!(done.outcome(), Outcome::Rejected);
        assert!(
            done.message.contains("engine_main_file"),
            "{}",
            done.message
        );
        let mut unknown = validation(&digest, 0);
        unknown.target = "nowhere".to_owned();
        let (done, _) = exec_validate(&exec, unknown).await;
        assert_eq!(done.outcome(), Outcome::Rejected);
        assert!(sink.command_results().is_empty());
        assert_eq!(fetcher.calls.load(Ordering::SeqCst), 0);
        assert_eq!(std::fs::read_dir(dest.path()).unwrap().count(), 0);
    }

    #[tokio::test]
    async fn rejected_bundle_is_not_applied() {
        let mut node = Checked::new();
        let invalid = engine_bundle(node.dest.path(), "invalid\n", "");
        node.fetcher = MemoryFetcher::holding(&[&invalid]);
        let exec = node.executor(Duration::from_secs(20));
        let before = node.live();

        let (done, sink) = run(&exec, action(&invalid.0, invalid.1.len())).await;
        assert_eq!(done.outcome(), Outcome::Failed, "{}", done.message);
        assert_eq!(done.message, "engine: configuration rejected, 1 error(s)");
        let results = sink.command_results();
        assert_eq!(results.len(), 1);
        assert_eq!(
            (results[0].command.as_str(), results[0].exit_code),
            ("verify engine", 1)
        );
        assert!(results[0].stdout.contains("Error: invalid object"));
        assert!(sink.progress().is_empty());

        // A checker that fails without telling how many errors it found.
        std::fs::write(format!("{}.quiet", node.script.display()), "").unwrap();
        let (done, sink) = run(&exec, action(&invalid.0, 0)).await;
        assert_eq!(done.outcome(), Outcome::Failed);
        assert_eq!(done.message, "engine: configuration rejected");
        assert_eq!(sink.command_results()[0].exit_code, 3);

        assert_eq!(node.live(), before);
        assert_eq!(node.live_main(), "live\n");
        assert!(!node.dest.path().join(".gorgone-engine.digest").exists());
        assert!(exec.state.applied().is_empty());
        assert_eq!(node.fetcher.calls.load(Ordering::SeqCst), 2);
    }

    #[tokio::test]
    async fn accepted_bundle_is_checked_then_applied_once() {
        let mut node = Checked::new();
        let valid = engine_bundle(node.dest.path(), "define host{}\n", "");
        node.fetcher = MemoryFetcher::holding(&[&valid]);
        let exec = node.executor(Duration::from_secs(20));

        let (done, sink) = run(&exec, action(&valid.0, valid.1.len())).await;
        assert_eq!(done.outcome(), Outcome::Ok, "{}", done.message);
        assert_eq!(done.message, "engine: 2 files written, 1 pruned");
        assert_eq!(
            sink.progress(),
            ["engine: configuration valid", done.message.as_str()]
        );
        let results = sink.command_results();
        assert_eq!(results.len(), 1);
        assert_eq!(results[0].exit_code, 0);
        // One download serves the check and the apply.
        assert_eq!(node.fetcher.calls.load(Ordering::SeqCst), 1);
        assert_eq!(node.checked_files().len(), 1);
        // The live main file is the one of the bundle, not the derived one.
        assert_eq!(
            node.live(),
            [".gorgone-engine.digest", "centengine.cfg", "hosts.cfg"]
        );
        assert_eq!(
            node.live_main(),
            format!("cfg_file={}/hosts.cfg\n", node.dest.path().display())
        );
        assert_eq!(exec.state.applied()[0].digest, valid.0);

        // Already applied: neither downloaded nor checked again.
        let (done, _) = run(&exec, action(&valid.0, 0)).await;
        assert_eq!(done.message, "engine: configuration unchanged");
        assert_eq!(node.fetcher.calls.load(Ordering::SeqCst), 1);
        assert_eq!(node.checked_files().len(), 1);
    }

    #[tokio::test]
    async fn staging_is_removed_whatever_ends_the_check() {
        let mut node = Checked::new();
        let valid = engine_bundle(node.dest.path(), "define host{}\n", "");
        let no_main = bundle_of(&[("hosts.cfg", "h\n")]);
        node.fetcher = MemoryFetcher::holding(&[&valid, &no_main]);
        let before = node.live();

        // A bundle without the main file never reaches the checker.
        let exec = node.executor(Duration::from_secs(20));
        let (done, sink) = run(&exec, action(&no_main.0, 0)).await;
        assert_eq!(done.outcome(), Outcome::Failed);
        assert!(
            done.message.contains("does not hold the engine main file"),
            "{}",
            done.message
        );
        assert!(sink.command_results().is_empty());
        assert_eq!(node.live(), before);

        // A checker that cannot start.
        let mut broken = node.executor(Duration::from_secs(20));
        broken.checker = Some(ConfigChecker::Command {
            argv: vec!["/nonexistent/centengine".to_owned(), "{config}".to_owned()],
            timeout: Duration::from_secs(20),
            kill_grace: Duration::ZERO,
        });
        let (done, sink) = run(&broken, action(&valid.0, 0)).await;
        assert_eq!(done.outcome(), Outcome::Failed);
        assert!(done.message.contains("could not run"), "{}", done.message);
        assert_eq!(sink.command_results()[0].exit_code, -1);
        assert_eq!(node.live(), before);

        // A checker slower than its timeout, then a cancelled job.
        let mut slow = Checked::new();
        std::fs::write(&slow.script, CHECKER.replace("sleep 0", "sleep 30")).unwrap();
        let valid = engine_bundle(slow.dest.path(), "define host{}\n", "");
        slow.fetcher = MemoryFetcher::holding(&[&valid]);
        let before = slow.live();
        let exec = slow.executor(Duration::from_millis(300));
        let (done, sink) = run(&exec, action(&valid.0, 0)).await;
        assert_eq!(done.outcome(), Outcome::Timeout, "{}", done.message);
        assert_eq!(done.message, "engine: configuration check timed out");
        assert!(sink.command_results()[0].timed_out);
        assert_eq!(slow.live(), before);

        let exec = slow.executor(Duration::from_secs(20));
        let sink = MemorySink::default();
        let cancel = CancellationToken::new();
        let ctx = JobContext {
            sink: &sink,
            cancel: &cancel,
        };
        let stopper = cancel.clone();
        tokio::spawn(async move {
            tokio::time::sleep(Duration::from_millis(300)).await;
            stopper.cancel();
        });
        let done = exec.execute(action(&valid.0, 0), &ctx).await;
        assert_eq!(done.outcome(), Outcome::Cancelled, "{}", done.message);
        assert_eq!(slow.live(), before);
        assert!(exec.state.applied().is_empty());
    }

    #[tokio::test]
    async fn leftover_staging_survives_no_apply_and_breaks_no_prune() {
        let mut node = Checked::new();
        let valid = engine_bundle(node.dest.path(), "define host{}\n", "");
        node.fetcher = MemoryFetcher::holding(&[&valid]);
        let leftover = node
            .dest
            .path()
            .join(format!("{}dead", bundle::STAGING_PREFIX));
        std::fs::create_dir_all(leftover.join("objects")).unwrap();
        std::fs::write(leftover.join("objects/hosts.cfg"), "invalid\n").unwrap();

        // Without check nothing sweeps it, and pruning leaves it alone.
        let unchecked = executor(&node.fetcher, node.dest.path(), true);
        let (done, _) = run(&unchecked, action(&valid.0, 0)).await;
        assert_eq!(done.outcome(), Outcome::Ok, "{}", done.message);
        assert_eq!(done.message, "engine: 2 files written, 1 pruned");
        assert!(leftover.join("objects/hosts.cfg").exists());

        // The next check starts from a clean target.
        let exec = node.executor(Duration::from_secs(20));
        let (done, _) = exec_validate(&exec, validation(&valid.0, 0)).await;
        assert_eq!(done.outcome(), Outcome::Ok, "{}", done.message);
        assert_eq!(
            node.live(),
            [".gorgone-engine.digest", "centengine.cfg", "hosts.cfg"]
        );
    }

    #[test]
    fn outside_paths_are_listed_up_to_a_point() {
        let path = Path::new("/etc/centreon-engine");
        assert_eq!(outside_note(&[], path), "");
        let many: Vec<String> = (0..7).map(|n| format!("/opt/{n}.cfg")).collect();
        assert_eq!(
            outside_note(&many, path),
            "; 7 path(s) outside /etc/centreon-engine checked as deployed on this node: \
             /opt/0.cfg, /opt/1.cfg, /opt/2.cfg, /opt/3.cfg, /opt/4.cfg, ..."
        );
    }
}
