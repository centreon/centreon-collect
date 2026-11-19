//! What the node really runs, as reported in every heartbeat: the digest
//! applied to each target and whether the engine API answers.

use std::collections::BTreeMap;
use std::fs::{self, DirBuilder, File, OpenOptions};
use std::io::{self, Write};
use std::os::unix::fs::{DirBuilderExt, OpenOptionsExt};
use std::path::{Path, PathBuf};
use std::sync::{Mutex, MutexGuard, PoisonError};
use std::time::Duration;

use gorgone_core::bundle;
use gorgone_proto::agent::{AppliedConfig, EngineState};
use tokio::sync::watch;
use tokio_util::sync::CancellationToken;

use crate::config::TargetConfig;
use crate::engine_api::EngineApi;

/// Probe period until a server tells its heartbeat interval.
const DEFAULT_PROBE_PERIOD: Duration = Duration::from_secs(15);

/// Name of the file recording the digest applied to `target`, in its directory.
pub(crate) fn marker_name(target: &str) -> String {
    format!(".gorgone-{target}.digest")
}

struct EngineWatch {
    /// `None` until the first probe ends.
    last: Mutex<Option<EngineState>>,
    period: watch::Sender<Duration>,
}

/// Digests of one target, by side.
#[derive(Default)]
struct Applied {
    /// What the marker of the local directory holds; `None` without `path`.
    local: Option<String>,
    /// Last digest every remote destination accepted; `None` without `urls`.
    remote: Option<String>,
}

/// Which sides a target has, fixed by the configuration.
#[derive(Clone, Copy)]
struct Sides {
    local: bool,
    remote: bool,
}

/// Cache read by the heartbeat, so that a beat touches neither the disk nor
/// the engine.
pub(crate) struct NodeState {
    /// Target name to digests; a target never applied has no entry.
    applied: Mutex<BTreeMap<String, Applied>>,
    /// Targets with a local side, a remote side, or both; one unknown here
    /// is taken as local only.
    sides: BTreeMap<String, Sides>,
    /// Where the remote digests survive a restart; memory only when unset.
    remote_dir: Option<PathBuf>,
    /// Set when the agent reaches the engine through its gRPC API.
    engine: Option<EngineWatch>,
}

fn lock<T>(mutex: &Mutex<T>) -> MutexGuard<'_, T> {
    mutex.lock().unwrap_or_else(PoisonError::into_inner)
}

/// File keeping the remote digest of `target`; target names are free text.
fn remote_file(dir: &Path, target: &str) -> PathBuf {
    dir.join(format!("{}.digest", bundle::digest(target.as_bytes())))
}

fn read_remote(dir: &Path, target: &str) -> Option<String> {
    let text = fs::read_to_string(remote_file(dir, target)).ok()?;
    let digest = text.trim();
    let valid = digest.len() == 64 && digest.bytes().all(|b| b.is_ascii_hexdigit());
    valid.then(|| digest.to_ascii_lowercase())
}

/// Replaces the remote digest file of `target`, durably; `None` removes it.
fn write_remote(dir: &Path, target: &str, digest: Option<&str>) -> io::Result<()> {
    let path = remote_file(dir, target);
    let Some(digest) = digest else {
        return match fs::remove_file(&path) {
            Err(error) if error.kind() != io::ErrorKind::NotFound => Err(error),
            _ => File::open(dir)?.sync_all(),
        };
    };
    DirBuilder::new().recursive(true).mode(0o700).create(dir)?;
    let tmp = path.with_extension("tmp");
    let mut file = OpenOptions::new()
        .write(true)
        .create(true)
        .truncate(true)
        .mode(0o600)
        .open(&tmp)?;
    file.write_all(digest.as_bytes())?;
    file.sync_all()?;
    fs::rename(&tmp, &path)?;
    File::open(dir)?.sync_all()
}

impl NodeState {
    /// Reads the digest markers of the local targets and, from `remote_dir`,
    /// the digests of the remote ones: blocking, call it off the runtime
    /// threads.
    pub(crate) fn load(
        targets: &BTreeMap<String, TargetConfig>,
        engine_api: bool,
        remote_dir: Option<&Path>,
    ) -> Self {
        let sides = targets
            .iter()
            .map(|(name, target)| {
                let sides = Sides {
                    local: target.path.is_some(),
                    remote: !target.urls.is_empty(),
                };
                (name.clone(), sides)
            })
            .collect();
        let applied = targets
            .iter()
            .map(|(name, target)| {
                let local = target
                    .path
                    .as_deref()
                    .and_then(|path| bundle::applied_digest(path, &marker_name(name)));
                let remote = remote_dir
                    .filter(|_| !target.urls.is_empty())
                    .and_then(|dir| read_remote(dir, name));
                (name.clone(), Applied { local, remote })
            })
            .collect();
        Self {
            applied: Mutex::new(applied),
            sides,
            remote_dir: remote_dir.map(Path::to_path_buf),
            engine: engine_api.then(|| EngineWatch {
                last: Mutex::new(None),
                period: watch::Sender::new(DEFAULT_PROBE_PERIOD),
            }),
        }
    }

    /// Records what the marker of `target` holds after an `ApplyConfig` job.
    pub(crate) fn set_applied(&self, target: &str, digest: Option<String>) {
        lock(&self.applied)
            .entry(target.to_owned())
            .or_default()
            .local = digest;
    }

    /// Last digest every remote destination of `target` accepted.
    pub(crate) fn remote_applied(&self, target: &str) -> Option<String> {
        lock(&self.applied).get(target)?.remote.clone()
    }

    /// Records the digest every remote destination of `target` accepted, or
    /// `None` when they may now differ, and keeps it in `remote_dir`.
    /// Blocking: call it off the runtime threads. Memory is updated even
    /// when the disk write fails.
    pub(crate) fn set_remote_applied(
        &self,
        target: &str,
        digest: Option<String>,
    ) -> io::Result<()> {
        let written = match &self.remote_dir {
            Some(dir) => write_remote(dir, target, digest.as_deref()),
            None => Ok(()),
        };
        lock(&self.applied)
            .entry(target.to_owned())
            .or_default()
            .remote = digest;
        written
    }

    /// Applied digests, sorted by target name. A target with both sides
    /// counts as applied only when both hold the same digest.
    pub(crate) fn applied(&self) -> Vec<AppliedConfig> {
        let local_only = Sides {
            local: true,
            remote: false,
        };
        lock(&self.applied)
            .iter()
            .filter_map(|(target, applied)| {
                let sides = self.sides.get(target).copied().unwrap_or(local_only);
                let digest = match (sides.local, sides.remote) {
                    (true, true) if applied.local == applied.remote => applied.local.clone(),
                    (true, true) => None,
                    (false, true) => applied.remote.clone(),
                    (_, false) => applied.local.clone(),
                }?;
                Some(AppliedConfig {
                    target: target.clone(),
                    digest,
                })
            })
            .collect()
    }

    /// Replaces the engine state, for an engine that is not probed over its
    /// API (the built-in scheduler).
    pub(crate) fn set_engine(&self, state: EngineState) {
        if let Some(engine) = &self.engine {
            let previous = lock(&engine.last).replace(state.clone());
            if previous.as_ref() != Some(&state) {
                tracing::info!(
                    reachable = state.reachable,
                    version = %state.version,
                    "engine state changed"
                );
            }
        }
    }

    /// Last known state of the engine API; `None` without `engine.grpc_url`,
    /// and until the first probe ends.
    pub(crate) fn engine(&self) -> Option<EngineState> {
        lock(&self.engine.as_ref()?.last).clone()
    }

    /// Aligns the engine probes on the heartbeat interval of the session.
    pub(crate) fn set_heartbeat_period(&self, period: Duration) {
        if let Some(engine) = &self.engine {
            engine.period.send_if_modified(|current| {
                let changed = *current != period;
                *current = period;
                changed
            });
        }
    }

    /// Probes the engine once per heartbeat interval until `stop` is
    /// cancelled, so that a heartbeat never waits for the engine.
    pub(crate) async fn watch_engine(&self, api: EngineApi, stop: CancellationToken) {
        let Some(engine) = &self.engine else {
            return;
        };
        let mut period = engine.period.subscribe();
        loop {
            let every = *period.borrow_and_update();
            let probed = tokio::select! {
                probed = api.probe(every / 2) => probed,
                () = stop.cancelled() => return,
            };
            let previous = lock(&engine.last).replace(probed.clone());
            if previous.as_ref() != Some(&probed) {
                tracing::info!(
                    reachable = probed.reachable,
                    version = %probed.version,
                    "engine API state changed"
                );
            }
            tokio::select! {
                () = tokio::time::sleep(every) => {}
                // A new interval takes effect now, not after the old one.
                changed = period.changed() => {
                    if changed.is_err() {
                        return;
                    }
                }
                () = stop.cancelled() => return,
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn loads_only_applied_targets_and_follows_updates() {
        let dir = tempfile::tempdir().unwrap();
        let target = |name: &str| {
            let path = dir.path().join(name);
            std::fs::create_dir(&path).unwrap();
            (name.to_owned(), TargetConfig::local(path))
        };
        let targets = BTreeMap::from([target("engine"), target("broker")]);
        let engine_path = targets["engine"].path.clone().unwrap();
        std::fs::write(engine_path.join(marker_name("engine")), "abc\n").unwrap();

        let state = NodeState::load(&targets, false, None);
        assert_eq!(pairs(&state), [("engine".to_owned(), "abc".to_owned())]);
        assert!(state.engine().is_none());

        state.set_applied("broker", Some("def".to_owned()));
        state.set_applied("engine", None);
        assert_eq!(pairs(&state), [("broker".to_owned(), "def".to_owned())]);
    }

    fn pairs(state: &NodeState) -> Vec<(String, String)> {
        state
            .applied()
            .into_iter()
            .map(|applied| (applied.target, applied.digest))
            .collect()
    }

    #[test]
    fn remote_digests_are_kept_on_disk_and_combined_with_local_ones() {
        let dir = tempfile::tempdir().unwrap();
        let remote_dir = dir.path().join("state/remote-applied");
        let local = dir.path().join("broker");
        let urls = vec!["http://broker:8080/files/broker".to_owned()];
        let targets = BTreeMap::from([
            (
                "remote".to_owned(),
                TargetConfig {
                    path: None,
                    urls: urls.clone(),
                    ..TargetConfig::local("")
                },
            ),
            (
                "both".to_owned(),
                TargetConfig {
                    urls,
                    ..TargetConfig::local(&local)
                },
            ),
        ]);
        let (a, b) = ("a".repeat(64), "b".repeat(64));
        let state = NodeState::load(&targets, false, Some(&remote_dir));
        assert!(pairs(&state).is_empty());

        state.set_remote_applied("remote", Some(a.clone())).unwrap();
        state.set_remote_applied("both", Some(a.clone())).unwrap();
        // Remote side alone: not applied everywhere yet.
        assert_eq!(pairs(&state), [("remote".to_owned(), a.clone())]);
        state.set_applied("both", Some(b.clone()));
        assert_eq!(pairs(&state), [("remote".to_owned(), a.clone())]);
        state.set_applied("both", Some(a.clone()));
        let both = [
            ("both".to_owned(), a.clone()),
            ("remote".to_owned(), a.clone()),
        ];
        assert_eq!(pairs(&state), both);
        assert_eq!(state.remote_applied("remote"), Some(a.clone()));

        // A restart reads the remote digests back, and the local marker.
        std::fs::create_dir_all(&local).unwrap();
        std::fs::write(local.join(marker_name("both")), &a).unwrap();
        let restarted = NodeState::load(&targets, false, Some(&remote_dir));
        assert_eq!(pairs(&restarted), both);

        // Forgotten on disk too; a damaged file reads as nothing applied.
        restarted.set_remote_applied("both", None).unwrap();
        restarted.set_remote_applied("both", None).unwrap();
        std::fs::write(remote_file(&remote_dir, "remote"), "garbage").unwrap();
        let restarted = NodeState::load(&targets, false, Some(&remote_dir));
        assert!(pairs(&restarted).is_empty());
        // Without a state directory, memory only.
        let volatile = NodeState::load(&targets, false, None);
        volatile
            .set_remote_applied("remote", Some(b.clone()))
            .unwrap();
        assert_eq!(pairs(&volatile), [("remote".to_owned(), b)]);
    }
}
