//! What the node really runs, as reported in every heartbeat: the digest
//! applied to each target and whether the engine API answers.

use std::collections::BTreeMap;
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

/// Cache read by the heartbeat, so that a beat touches neither the disk nor
/// the engine.
pub(crate) struct NodeState {
    /// Target name to applied digest; a target never applied has no entry.
    applied: Mutex<BTreeMap<String, String>>,
    /// Set when the agent reaches the engine through its gRPC API.
    engine: Option<EngineWatch>,
}

fn lock<T>(mutex: &Mutex<T>) -> MutexGuard<'_, T> {
    mutex.lock().unwrap_or_else(PoisonError::into_inner)
}

impl NodeState {
    /// Reads the digest markers of `targets`: blocking, call it off the
    /// runtime threads.
    pub(crate) fn load(targets: &BTreeMap<String, TargetConfig>, engine_api: bool) -> Self {
        let applied = targets
            .iter()
            .filter_map(|(name, target)| {
                let digest = bundle::applied_digest(&target.path, &marker_name(name))?;
                Some((name.clone(), digest))
            })
            .collect();
        Self {
            applied: Mutex::new(applied),
            engine: engine_api.then(|| EngineWatch {
                last: Mutex::new(None),
                period: watch::Sender::new(DEFAULT_PROBE_PERIOD),
            }),
        }
    }

    /// Records what the marker of `target` holds after an `ApplyConfig` job.
    pub(crate) fn set_applied(&self, target: &str, digest: Option<String>) {
        let mut applied = lock(&self.applied);
        match digest {
            Some(digest) => applied.insert(target.to_owned(), digest),
            None => applied.remove(target),
        };
    }

    /// Applied digests, sorted by target name.
    pub(crate) fn applied(&self) -> Vec<AppliedConfig> {
        lock(&self.applied)
            .iter()
            .map(|(target, digest)| AppliedConfig {
                target: target.clone(),
                digest: digest.clone(),
            })
            .collect()
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
            let target = TargetConfig {
                path,
                prune: false,
                engine_main_file: None,
            };
            (name.to_owned(), target)
        };
        let targets = BTreeMap::from([target("engine"), target("broker")]);
        std::fs::write(targets["engine"].path.join(marker_name("engine")), "abc\n").unwrap();

        let state = NodeState::load(&targets, false);
        let pairs = |state: &NodeState| -> Vec<(String, String)> {
            state
                .applied()
                .into_iter()
                .map(|applied| (applied.target, applied.digest))
                .collect()
        };
        assert_eq!(pairs(&state), [("engine".to_owned(), "abc".to_owned())]);
        assert!(state.engine().is_none());

        state.set_applied("broker", Some("def".to_owned()));
        state.set_applied("engine", None);
        assert_eq!(pairs(&state), [("broker".to_owned(), "def".to_owned())]);
    }
}
