//! `ApplyConfig`: fetches a content-addressed bundle and syncs it into the
//! directory the administrator mapped to the target, once the engine has
//! accepted it when the target holds an engine configuration.
//! `ValidateConfig`: the same check alone, the live files are not touched.

use std::collections::BTreeMap;
use std::future::Future;
use std::path::Path;
use std::sync::Arc;
use std::time::{Duration, Instant};

use gorgone_core::bundle::{self, ApplyOptions, ApplyReport};
use gorgone_proto::agent::job_event::Body;
use gorgone_proto::agent::job_finished::Outcome;
use gorgone_proto::agent::{ApplyConfig, CommandResult, JobFinished, ValidateConfig};

use super::verify::{self, ConfigChecker};
use super::{Executor, JobContext, command_result, finished};
use crate::config::{FileMode, Secret, TargetConfig};
use crate::http_post::{self, HttpTarget, PostError, Request};
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
    /// Bound of one `PUT` to a remote destination, connection included.
    pub http_timeout: Duration,
    /// Sent with every `PUT` to a remote destination.
    pub bearer: Option<Secret>,
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
        target_path: &Path,
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
                target_path.to_path_buf(),
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
        let note = outside_note(&staging.outside, target_path);
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
        let (Some(main_file), Some(path)) = (&target.engine_main_file, &target.path) else {
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
            .check(&target_name, path, main_file, blob, &digest, ctx)
            .await
        {
            Ok(verified) => finished(Outcome::Ok, verified.message),
            Err(done) => done,
        }
    }

    /// True when every destination of `target` already holds `digest`.
    fn already_applied(&self, name: &str, target: &TargetConfig, digest: &str) -> bool {
        let local = target.path.as_deref().is_none_or(|path| {
            bundle::applied_digest(path, &marker_name(name)).as_deref() == Some(digest)
        });
        let remote =
            target.urls.is_empty() || self.state.remote_applied(name).as_deref() == Some(digest);
        local && remote
    }

    /// Syncs `blob` into the local directory of the target; returns the
    /// summary of what was written.
    async fn apply_locally(
        &self,
        target_name: &str,
        target: &TargetConfig,
        path: &Path,
        blob: Vec<u8>,
        digest: &str,
    ) -> Result<String, JobFinished> {
        let marker = marker_name(target_name);
        let dest = path.to_path_buf();
        let (prune, max_unpacked) = (target.prune, self.max_unpacked_bytes);
        let file_mode = target.file_mode.map(FileMode::bits);
        let digest = digest.to_owned();
        let applied = tokio::task::spawn_blocking(move || {
            let options = ApplyOptions {
                marker: &marker,
                prune,
                max_unpacked,
            };
            let report = bundle::apply_with_file_mode(&blob, &digest, &dest, &options, file_mode);
            // Read back rather than assumed: a failed apply may leave either.
            (report, bundle::applied_digest(&dest, &marker))
        })
        .await;
        let applied = applied.map(|(report, now_applied)| {
            self.state.set_applied(target_name, now_applied);
            report
        });
        match applied {
            Ok(Ok(report)) => Ok(summary(target_name, report)),
            Ok(Err(error)) => Err(finished(Outcome::Failed, format!("{target_name}: {error}"))),
            Err(error) => Err(finished(
                Outcome::Failed,
                format!("apply task failed: {error}"),
            )),
        }
    }

    /// `PUT`s `tar` to every URL, in order, one `CommandResult` each from
    /// `first_index`. A failure never skips the URLs after it: they are other
    /// destinations to keep up to date.
    async fn push(
        &self,
        urls: &[String],
        tar: &[u8],
        first_index: usize,
        ctx: &JobContext<'_>,
    ) -> Pushed {
        let mut pushed = Pushed {
            accepted: 0,
            total: urls.len(),
            report: String::new(),
            cancelled: false,
        };
        let request = Request {
            method: "PUT",
            content_type: Some(TAR_CONTENT_TYPE),
            body: tar,
            bearer: self.bearer.as_ref().map(Secret::expose),
        };
        for (index, url) in urls.iter().enumerate() {
            let started = Instant::now();
            let answer = match HttpTarget::parse(url) {
                // Refused when the configuration is loaded.
                Err(reason) => Err(PostError::Failed(reason)),
                Ok(destination) => tokio::select! {
                    biased;
                    () = ctx.cancel.cancelled() => {
                        pushed.cancelled = true;
                        return pushed;
                    }
                    answer = http_post::send(&destination, request, self.http_timeout) => answer,
                },
            };
            let mut result = CommandResult {
                index: u32::try_from(first_index + index).unwrap_or(u32::MAX),
                command: format!("PUT {url}"),
                exit_code: 1,
                duration_ms: u64::try_from(started.elapsed().as_millis()).unwrap_or(u64::MAX),
                ..CommandResult::default()
            };
            match answer {
                Ok(status) => {
                    if status.accepted() {
                        pushed.accepted += 1;
                        result.exit_code = 0;
                    }
                    result.stdout = status.line;
                }
                Err(error) => {
                    result.timed_out = matches!(error, PostError::Timeout(_));
                    result.stderr = error.to_string();
                }
            }
            // One of the two is empty.
            pushed
                .report
                .push_str(&format!("; {url}: {}{}", result.stdout, result.stderr));
            ctx.sink.emit(Body::CommandResult(result));
        }
        pushed
    }

    /// Records what the remote destinations now hold; returns a note when
    /// it could not be kept on disk.
    async fn remember_remote(&self, target_name: &str, digest: Option<String>) -> String {
        let (state, name) = (self.state.clone(), target_name.to_owned());
        let kept =
            tokio::task::spawn_blocking(move || state.set_remote_applied(&name, digest)).await;
        let error = match kept {
            Ok(Ok(())) => return String::new(),
            Ok(Err(error)) => error.to_string(),
            Err(error) => error.to_string(),
        };
        // Only the short-circuit after a restart is lost: the next push resends.
        tracing::warn!(target = %target_name, %error, "cannot keep the remote digest on disk");
        format!(" (applied digest not kept on disk: {error})")
    }
}

/// Content type of the bundles sent to the remote destinations.
const TAR_CONTENT_TYPE: &str = "application/x-tar";

/// What [`ApplyConfigExecutor::push`] did.
struct Pushed {
    accepted: usize,
    total: usize,
    /// `; <url>: <status line or error>` per destination called.
    report: String,
    /// Stopped by the cancellation of the job.
    cancelled: bool,
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

        if self.already_applied(&target_name, target, &digest) {
            let unchanged = ApplyReport {
                written: 0,
                pruned: 0,
                unchanged: true,
            };
            if target.path.is_some() {
                self.state.set_applied(&target_name, Some(digest));
            }
            let message = summary(&target_name, unchanged);
            ctx.progress(message.clone());
            return finished(Outcome::Ok, message);
        }

        let mut blob = match self.download(&digest, action.size, ctx).await {
            Ok(blob) => blob,
            Err(done) => return done,
        };
        // Nothing of the live directory is written before the engine agrees.
        let mut first_index = 0;
        if let Some(main_file) = &target.engine_main_file {
            let Some(path) = &target.path else {
                // The configuration check at start refuses this.
                return finished(
                    Outcome::Rejected,
                    format!("{target_name}: engine_main_file requires a local path"),
                );
            };
            blob = match self
                .check(&target_name, path, main_file, blob, &digest, ctx)
                .await
            {
                Ok(verified) => {
                    ctx.progress(verified.message);
                    verified.blob
                }
                Err(done) => return done,
            };
            first_index = 1;
        }

        // Converted before anything is written: a bundle no destination can
        // take fails the job with every destination untouched.
        let tar = if target.urls.is_empty() {
            None
        } else {
            let max_unpacked = self.max_unpacked_bytes;
            let converted = tokio::task::spawn_blocking(move || {
                let tar = bundle::to_tar(&blob, max_unpacked);
                (blob, tar)
            })
            .await;
            match converted {
                Ok((kept, Ok(tar))) => {
                    blob = kept;
                    Some(tar)
                }
                Ok((_, Err(error))) => {
                    return finished(Outcome::Failed, format!("{target_name}: {error}"));
                }
                Err(error) => {
                    return finished(Outcome::Failed, format!("tar task failed: {error}"));
                }
            }
        };

        let local = match &target.path {
            Some(path) => {
                match self
                    .apply_locally(&target_name, target, path, blob, &digest)
                    .await
                {
                    Ok(message) => {
                        ctx.progress(message.clone());
                        Some(message)
                    }
                    // The remote destinations keep what they hold.
                    Err(done) => return done,
                }
            }
            None => None,
        };
        let Some(tar) = tar else {
            return finished(Outcome::Ok, local.unwrap_or_default());
        };

        let pushed = self.push(&target.urls, &tar, first_index, ctx).await;
        let all = !pushed.cancelled && pushed.accepted == pushed.total;
        // After a partial push the destinations may hold different bundles:
        // only a complete one is remembered, so that a retry is never skipped.
        let note = self
            .remember_remote(&target_name, all.then(|| digest.clone()))
            .await;
        let Pushed {
            accepted,
            total,
            report,
            cancelled,
        } = pushed;
        let (outcome, remote) = if cancelled {
            (
                Outcome::Cancelled,
                format!("cancelled, {accepted} of {total} destinations accepted{report}"),
            )
        } else {
            let outcome = if all { Outcome::Ok } else { Outcome::Failed };
            (
                outcome,
                format!("{accepted} of {total} destinations accepted{report}"),
            )
        };
        let message = match local {
            Some(local) => format!("{local}; {remote}{note}"),
            None => format!("{target_name}: {remote}{note}"),
        };
        if outcome == Outcome::Ok {
            ctx.progress(message.clone());
        }
        finished(outcome, message)
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
    use crate::http_post::testing::{FakeHttp, refused_url};

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
            prune,
            ..TargetConfig::local(dest)
        };
        executor_for(fetcher, target, None)
    }

    /// Executor of the one target `engine`, its state kept in `state_dir`.
    fn executor_for<'a>(
        fetcher: &'a MemoryFetcher,
        target: TargetConfig,
        state_dir: Option<&Path>,
    ) -> ApplyConfigExecutor<&'a MemoryFetcher> {
        let targets = BTreeMap::from([("engine".to_owned(), target)]);
        ApplyConfigExecutor {
            state: Arc::new(NodeState::load(&targets, false, state_dir)),
            targets,
            max_bundle_bytes: 1 << 20,
            max_unpacked_bytes: 1 << 20,
            fetcher,
            checker: None,
            max_output: 4096,
            http_timeout: Duration::from_secs(5),
            bearer: None,
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

    const NO_CONTENT: &[u8] = b"HTTP/1.1 204 No Content\r\n\r\n";
    const DENIED: &[u8] = b"HTTP/1.1 401 Unauthorized\r\nContent-Length: 0\r\n\r\n";

    fn remote(urls: &[String]) -> TargetConfig {
        TargetConfig {
            path: None,
            urls: urls.to_vec(),
            ..TargetConfig::local("")
        }
    }

    /// Files of a plain tar: name, mode, content.
    fn tar_files(body: &[u8]) -> Vec<(String, u32, String)> {
        use std::io::Read;
        let mut archive = tar::Archive::new(body);
        archive
            .entries()
            .unwrap()
            .map(|entry| {
                let mut entry = entry.unwrap();
                let mut content = String::new();
                entry.read_to_string(&mut content).unwrap();
                let name = entry.path().unwrap().display().to_string();
                (name, entry.header().mode().unwrap(), content)
            })
            .collect()
    }

    fn header<'a>(head: &'a str, name: &str) -> Option<&'a str> {
        head.lines()
            .find_map(|line| line.strip_prefix(name)?.strip_prefix(": "))
    }

    #[tokio::test]
    async fn remote_target_puts_a_plain_tar_to_every_url() {
        let (digest, blob) =
            bundle_of(&[("central-broker.json", "{}\n"), ("watchdog.json", "w\n")]);
        let fetcher = MemoryFetcher::holding(&[&(digest.clone(), blob)]);
        let first = FakeHttp::start(Some(NO_CONTENT)).await;
        let second = FakeHttp::start(Some(b"HTTP/1.1 200 OK\r\n\r\n")).await;
        let urls = [first.url("/files/broker"), second.url("/files/broker")];
        let exec = executor_for(&fetcher, remote(&urls), None);

        let (done, sink) = run(&exec, action(&digest, 0)).await;
        assert_eq!(done.outcome(), Outcome::Ok, "{}", done.message);
        assert_eq!(
            done.message,
            format!(
                "engine: 2 of 2 destinations accepted; {}: HTTP/1.1 204 No Content; \
                 {}: HTTP/1.1 200 OK",
                urls[0], urls[1]
            )
        );
        assert_eq!(sink.progress(), [done.message.as_str()]);
        let results = sink.command_results();
        let summary: Vec<(u32, &str, i32)> = results
            .iter()
            .map(|r| (r.index, r.command.as_str(), r.exit_code))
            .collect();
        let commands = urls.clone().map(|url| format!("PUT {url}"));
        assert_eq!(
            summary,
            [(0, commands[0].as_str(), 0), (1, commands[1].as_str(), 0)]
        );
        for server in [&first, &second] {
            let requests = server.full_requests();
            assert_eq!(requests.len(), 1);
            let (head, body) = &requests[0];
            assert!(head.starts_with("PUT /files/broker HTTP/1.1\r\n"), "{head}");
            assert_eq!(header(head, "Content-Type"), Some("application/x-tar"));
            assert_eq!(header(head, "Authorization"), None);
            assert_eq!(
                tar_files(body),
                [
                    ("central-broker.json".to_owned(), 0o644, "{}\n".to_owned()),
                    ("watchdog.json".to_owned(), 0o644, "w\n".to_owned()),
                ]
            );
        }
        assert_eq!(exec.state.applied()[0].digest, digest);

        // Already accepted everywhere: neither downloaded nor sent again.
        let (done, sink) = run(&exec, action(&digest, 0)).await;
        assert_eq!(done.outcome(), Outcome::Ok);
        assert_eq!(done.message, "engine: configuration unchanged");
        assert!(sink.command_results().is_empty());
        assert_eq!(fetcher.calls.load(Ordering::SeqCst), 1);
        assert_eq!(first.requests().len(), 1);
    }

    #[tokio::test]
    async fn bearer_token_is_sent_when_configured() {
        let (digest, blob) = bundle_of(&[("a.json", "a\n")]);
        let fetcher = MemoryFetcher::holding(&[&(digest.clone(), blob)]);
        let server = FakeHttp::start(Some(NO_CONTENT)).await;
        let mut exec = executor_for(&fetcher, remote(&[server.url("/files/a")]), None);
        exec.bearer = Some(Secret::new("s3cr3t-t0ken"));
        let (done, sink) = run(&exec, action(&digest, 0)).await;
        assert_eq!(done.outcome(), Outcome::Ok, "{}", done.message);
        let (head, _) = &server.full_requests()[0];
        assert_eq!(header(head, "Authorization"), Some("Bearer s3cr3t-t0ken"));
        // Never reported.
        assert!(!done.message.contains("s3cr3t"));
        assert!(!format!("{:?}", sink.command_results()).contains("s3cr3t"));
    }

    #[tokio::test]
    async fn refused_destination_fails_the_job_and_forgets_the_digest() {
        let (first, first_blob) = bundle_of(&[("a.json", "1\n")]);
        let (second, second_blob) = bundle_of(&[("a.json", "2\n")]);
        let fetcher =
            MemoryFetcher::holding(&[&(first.clone(), first_blob), &(second.clone(), second_blob)]);
        let ok = FakeHttp::start(Some(NO_CONTENT)).await;
        let denied = FakeHttp::start(Some(DENIED)).await;
        let silent = FakeHttp::start(None).await;
        let down = refused_url("/files/a").await;

        let mut exec = executor_for(&fetcher, remote(&[ok.url("/files/a")]), None);
        let (done, _) = run(&exec, action(&first, 0)).await;
        assert_eq!(done.outcome(), Outcome::Ok, "{}", done.message);
        assert_eq!(exec.state.applied()[0].digest, first);

        let urls = [
            denied.url("/files/a"),
            silent.url("/files/a"),
            down.clone(),
            ok.url("/files/a"),
        ];
        if let Some(target) = exec.targets.get_mut("engine") {
            target.urls = urls.to_vec();
        }
        exec.http_timeout = Duration::from_millis(300);
        let (done, sink) = run(&exec, action(&second, 0)).await;
        assert_eq!(done.outcome(), Outcome::Failed, "{}", done.message);
        assert!(
            done.message
                .starts_with("engine: 1 of 4 destinations accepted; "),
            "{}",
            done.message
        );
        assert!(
            done.message.contains("HTTP/1.1 401 Unauthorized"),
            "{}",
            done.message
        );
        assert!(
            done.message.contains("no answer within 300 ms"),
            "{}",
            done.message
        );
        let results = sink.command_results();
        let codes: Vec<(i32, bool)> = results.iter().map(|r| (r.exit_code, r.timed_out)).collect();
        assert_eq!(codes, [(1, false), (1, true), (1, false), (0, false)]);
        assert!(
            results[2].stderr.starts_with("connection to"),
            "{results:?}"
        );
        assert!(sink.progress().is_empty());
        // The destinations may now differ: nothing is reported, and the first
        // bundle is sent again rather than skipped.
        assert!(exec.state.applied().is_empty());
        assert_eq!(exec.state.remote_applied("engine"), None);
        if let Some(target) = exec.targets.get_mut("engine") {
            target.urls = vec![ok.url("/files/a")];
        }
        let (done, _) = run(&exec, action(&first, 0)).await;
        assert_eq!(done.outcome(), Outcome::Ok, "{}", done.message);
        assert_eq!(ok.requests().len(), 3);
    }

    #[tokio::test]
    async fn remote_digest_survives_a_restart_with_a_state_directory() {
        let (digest, blob) = bundle_of(&[("a.json", "a\n")]);
        let fetcher = MemoryFetcher::holding(&[&(digest.clone(), blob)]);
        let server = FakeHttp::start(Some(NO_CONTENT)).await;
        let state = tempfile::tempdir().unwrap();
        let target = remote(&[server.url("/files/a")]);

        let exec = executor_for(&fetcher, target.clone(), Some(state.path()));
        let (done, _) = run(&exec, action(&digest, 0)).await;
        assert_eq!(done.outcome(), Outcome::Ok, "{}", done.message);
        drop(exec);

        let restarted = executor_for(&fetcher, target.clone(), Some(state.path()));
        assert_eq!(restarted.state.applied()[0].digest, digest);
        let (done, _) = run(&restarted, action(&digest, 0)).await;
        assert_eq!(done.message, "engine: configuration unchanged");
        assert_eq!(server.requests().len(), 1);
        assert_eq!(fetcher.calls.load(Ordering::SeqCst), 1);

        // Memory only: a restart sends again.
        let volatile = executor_for(&fetcher, target, None);
        assert!(volatile.state.applied().is_empty());
    }

    #[tokio::test]
    async fn mixed_target_writes_locally_with_its_mode_and_sends_remotely() {
        let (digest, blob) = bundle_of(&[("engine-context.json", "{\"k\":1}\n")]);
        let fetcher = MemoryFetcher::holding(&[&(digest.clone(), blob)]);
        let server = FakeHttp::start(Some(NO_CONTENT)).await;
        let denied = FakeHttp::start(Some(DENIED)).await;
        let dest = tempfile::tempdir().unwrap();
        let target = TargetConfig {
            urls: vec![denied.url("/files/engine_context")],
            file_mode: Some(FileMode::parse("0640").unwrap()),
            ..TargetConfig::local(dest.path())
        };
        let mut exec = executor_for(&fetcher, target, None);
        let mode = |name: &str| {
            use std::os::unix::fs::PermissionsExt;
            std::fs::metadata(dest.path().join(name))
                .unwrap()
                .permissions()
                .mode()
                & 0o7777
        };

        // Local side written, remote side refused: the job fails, nothing
        // is reported as applied.
        let (done, sink) = run(&exec, action(&digest, 0)).await;
        assert_eq!(done.outcome(), Outcome::Failed, "{}", done.message);
        assert!(
            done.message
                .starts_with("engine: 1 files written, 0 pruned; 0 of 1 destinations accepted; "),
            "{}",
            done.message
        );
        assert_eq!(sink.progress(), ["engine: 1 files written, 0 pruned"]);
        assert_eq!(mode("engine-context.json"), 0o640);
        assert_eq!(mode(".gorgone-engine.digest"), 0o644);
        assert!(exec.state.applied().is_empty());

        // Retried once the destination accepts: no local write, one PUT.
        if let Some(target) = exec.targets.get_mut("engine") {
            target.urls = vec![server.url("/files/engine_context")];
        }
        let (done, _) = run(&exec, action(&digest, 0)).await;
        assert_eq!(done.outcome(), Outcome::Ok, "{}", done.message);
        assert_eq!(
            done.message,
            format!(
                "engine: configuration unchanged; 1 of 1 destinations accepted; {}: \
                 HTTP/1.1 204 No Content",
                server.url("/files/engine_context")
            )
        );
        assert_eq!(exec.state.applied()[0].digest, digest);
        let (_, body) = &server.full_requests()[0];
        // The tar keeps the bundle mode: the destination decides its own.
        assert_eq!(
            tar_files(body),
            [(
                "engine-context.json".to_owned(),
                0o644,
                "{\"k\":1}\n".to_owned()
            )]
        );
        let (done, _) = run(&exec, action(&digest, 0)).await;
        assert_eq!(done.message, "engine: configuration unchanged");
        assert_eq!(server.requests().len(), 1);
    }

    #[tokio::test]
    async fn validation_needs_a_local_target() {
        let (digest, blob) = bundle_of(&[("a.json", "a\n")]);
        let fetcher = MemoryFetcher::holding(&[&(digest.clone(), blob)]);
        let mut target = remote(&["http://broker:8080/files/a".to_owned()]);
        target.engine_main_file = Some(PathBuf::from("centengine.cfg"));
        let exec = executor_for(&fetcher, target, None);
        let (done, _) = exec_validate(&exec, validation(&digest, 0)).await;
        assert_eq!(done.outcome(), Outcome::Rejected);
        let (done, _) = run(&exec, action(&digest, 0)).await;
        assert_eq!(done.outcome(), Outcome::Rejected);
        assert!(
            done.message.contains("requires a local path"),
            "{}",
            done.message
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
