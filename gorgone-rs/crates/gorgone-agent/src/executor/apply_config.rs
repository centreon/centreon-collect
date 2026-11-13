//! `ApplyConfig`: fetches a content-addressed bundle and syncs it into the
//! directory the administrator mapped to the target.

use std::collections::BTreeMap;
use std::future::Future;
use std::sync::Arc;

use gorgone_core::bundle::{self, ApplyOptions, ApplyReport};
use gorgone_proto::agent::job_finished::Outcome;
use gorgone_proto::agent::{ApplyConfig, JobFinished};

use super::{Executor, JobContext, finished};
use crate::config::TargetConfig;
use crate::node_state::{NodeState, marker_name};

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

impl<F: BlobFetcher> Executor for ApplyConfigExecutor<F> {
    type Action = ApplyConfig;

    async fn execute(&self, action: ApplyConfig, ctx: &JobContext<'_>) -> JobFinished {
        let target_name = action.target;
        let Some(target) = self.targets.get(&target_name) else {
            return finished(
                Outcome::Rejected,
                format!("target {target_name:?} is not configured on this agent"),
            );
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

        if action.size > self.max_bundle_bytes {
            return finished(
                Outcome::Failed,
                format!(
                    "bundle of {} bytes is above the {} bytes allowed",
                    action.size, self.max_bundle_bytes
                ),
            );
        }
        // A size of 0 means "not announced": only the cap applies then.
        let limit = match action.size {
            0 => self.max_bundle_bytes,
            size => size,
        };
        let fetched = tokio::select! {
            fetched = self.fetcher.fetch(&digest, limit) => fetched,
            () = ctx.cancel.cancelled() => {
                return finished(Outcome::Cancelled, "cancelled during download");
            }
        };
        let blob = match fetched {
            Ok(blob) => blob,
            Err(error) => return finished(Outcome::Failed, error.to_string()),
        };
        if action.size != 0 && blob.len() as u64 != action.size {
            return finished(
                Outcome::Failed,
                format!(
                    "blob is {} bytes, {} were announced",
                    blob.len(),
                    action.size
                ),
            );
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
    use std::path::Path;
    use std::sync::atomic::{AtomicUsize, Ordering};

    use super::*;
    use crate::executor::testing::run;

    #[derive(Default)]
    struct MemoryFetcher {
        blobs: HashMap<String, Vec<u8>>,
        calls: AtomicUsize,
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
        };
        ApplyConfigExecutor {
            targets: BTreeMap::from([("engine".to_owned(), target)]),
            max_bundle_bytes: 1 << 20,
            max_unpacked_bytes: 1 << 20,
            fetcher,
            state: Arc::new(NodeState::load(&BTreeMap::new(), false)),
        }
    }

    fn action(digest: &str, size: usize) -> ApplyConfig {
        ApplyConfig {
            target: "engine".to_owned(),
            digest: digest.to_owned(),
            size: size as u64,
        }
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
        assert_eq!(sink.progress(), [done.message.as_str()]);
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
}
