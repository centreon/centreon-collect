//! In-memory view of the agent sessions held by this replica, and of the
//! jobs somebody is waiting for.
//!
//! Losing it loses no job: the store knows every job and every session.

use std::collections::HashMap;
use std::sync::atomic::{AtomicBool, AtomicU8, Ordering};
use std::sync::{Arc, Mutex, MutexGuard, PoisonError};
use std::time::Duration;

use gorgone_proto::agent::ServerFrame;
use tokio::sync::{broadcast, mpsc};
use tokio_util::sync::CancellationToken;
use tonic::Status;
use tracing::warn;

/// Sending side of one agent session.
#[derive(Debug, Clone)]
pub(crate) struct SessionHandle {
    pub(crate) session_id: String,
    pub(crate) tx: mpsc::Sender<Result<ServerFrame, Status>>,
    /// Cancelled when the session is replaced, kicked or the server stops.
    pub(crate) closed: CancellationToken,
    /// Set once the work owed to a new session (jobs of the former one,
    /// pending cancellations) was done.
    pub(crate) primed: Arc<AtomicBool>,
}

impl SessionHandle {
    pub(crate) fn new(session_id: String, tx: mpsc::Sender<Result<ServerFrame, Status>>) -> Self {
        Self {
            session_id,
            tx,
            closed: CancellationToken::new(),
            primed: Arc::default(),
        }
    }
}

/// Nobody drains the node.
const IDLE: u8 = 0;
/// One task drains the node.
const DRAINING: u8 = 1;
/// One task drains the node and must run one more pass.
const DRAINING_AGAIN: u8 = 2;

/// Dispatch state of one node on this replica: at most one task drains it,
/// which is what keeps the order, and requests made meanwhile are coalesced
/// into one more pass of that task.
#[derive(Debug, Default)]
pub(crate) struct NodeState {
    phase: AtomicU8,
    /// The next pass must also look at cancellations and stale dispatches.
    full: AtomicBool,
}

/// Change of a job, told to the local waiters.
#[derive(Debug, Clone)]
pub(crate) struct JobUpdate {
    pub(crate) job_id: Arc<str>,
    /// The job reached a final state.
    pub(crate) terminal: bool,
}

/// A job with waiters, and what the shared poller last saw of it.
#[derive(Debug, Default)]
struct Watched {
    waiters: usize,
    version: Option<(String, i64)>,
}

/// Local sessions and job notifications.
#[derive(Debug)]
pub(crate) struct Hub {
    sessions: Mutex<HashMap<String, SessionHandle>>,
    nodes: Mutex<HashMap<String, Arc<NodeState>>>,
    job_updates: broadcast::Sender<JobUpdate>,
    watched: Mutex<HashMap<String, Watched>>,
    send_timeout: Duration,
}

fn lock<T>(mutex: &Mutex<T>) -> MutexGuard<'_, T> {
    // The maps stay consistent even if a holder panicked.
    mutex.lock().unwrap_or_else(PoisonError::into_inner)
}

impl Hub {
    /// `send_timeout` bounds how long a frame may wait for a slow agent.
    pub(crate) fn new(send_timeout: Duration) -> Self {
        Self {
            sessions: Mutex::default(),
            nodes: Mutex::default(),
            job_updates: broadcast::channel(4096).0,
            watched: Mutex::default(),
            send_timeout,
        }
    }

    fn publish_gauge(sessions: &HashMap<String, SessionHandle>) {
        metrics::gauge!("gorgone_agent_sessions").set(sessions.len() as f64);
    }

    /// Makes `handle` the session of `node_id`; a previous one is closed.
    pub(crate) fn register(&self, node_id: &str, handle: SessionHandle) {
        let mut sessions = lock(&self.sessions);
        if let Some(old) = sessions.insert(node_id.to_owned(), handle) {
            old.closed.cancel();
        }
        Self::publish_gauge(&sessions);
    }

    /// Forgets the session of `node_id` if it is still `session_id`.
    pub(crate) fn remove(&self, node_id: &str, session_id: &str) {
        let mut sessions = lock(&self.sessions);
        if sessions
            .get(node_id)
            .is_some_and(|h| h.session_id == session_id)
        {
            sessions.remove(node_id);
        }
        Self::publish_gauge(&sessions);
    }

    /// Closes and forgets the session of `node_id`, whatever it is.
    pub(crate) fn kick(&self, node_id: &str) {
        let mut sessions = lock(&self.sessions);
        if let Some(old) = sessions.remove(node_id) {
            old.closed.cancel();
        }
        Self::publish_gauge(&sessions);
    }

    /// Closes `session`, and forgets it if it is still the one of the node.
    pub(crate) fn kick_session(&self, node_id: &str, session: &SessionHandle) {
        session.closed.cancel();
        self.remove(node_id, &session.session_id);
    }

    pub(crate) fn get(&self, node_id: &str) -> Option<SessionHandle> {
        lock(&self.sessions).get(node_id).cloned()
    }

    /// Nodes with a session on this replica.
    pub(crate) fn nodes(&self) -> Vec<String> {
        lock(&self.sessions).keys().cloned().collect()
    }

    /// Closes every session (shutdown).
    pub(crate) fn close_all(&self) {
        for (_, handle) in lock(&self.sessions).drain() {
            handle.closed.cancel();
        }
    }

    /// Dispatch state of one node on this replica.
    pub(crate) fn node_state(&self, node_id: &str) -> Arc<NodeState> {
        let mut nodes = lock(&self.nodes);
        // Bounded by the nodes seen; drop the entries nobody uses.
        if nodes.len() > 4096 {
            nodes.retain(|_, state| Arc::strong_count(state) > 1);
        }
        Arc::clone(nodes.entry(node_id.to_owned()).or_default())
    }

    /// Queues `frame` for the agent. A session that does not take it within
    /// the send timeout is stalled: it is kicked so that it never blocks
    /// the work of other nodes. Returns false when the frame was not queued.
    pub(crate) async fn send(
        &self,
        node_id: &str,
        session: &SessionHandle,
        frame: ServerFrame,
    ) -> bool {
        if session.closed.is_cancelled() {
            return false;
        }
        // Fast path: room in the buffer, no timer needed.
        let frame = match session.tx.try_send(Ok(frame)) {
            Ok(()) => return true,
            Err(mpsc::error::TrySendError::Closed(_)) => return false,
            Err(mpsc::error::TrySendError::Full(frame)) => frame,
        };
        match tokio::time::timeout(self.send_timeout, session.tx.send(frame)).await {
            Ok(Ok(())) => true,
            Ok(Err(_)) => false,
            Err(_) => {
                warn!(node = node_id, session = %session.session_id, "agent does not read its session, closing it");
                metrics::counter!("gorgone_agent_sessions_stalled_total").increment(1);
                self.kick_session(node_id, session);
                false
            }
        }
    }

    /// Tells local waiters that `job_id` changed; called right after the
    /// commit, so a local waiter never depends on a polling period.
    pub(crate) fn notify_job(&self, job_id: &str, terminal: bool) {
        // No receiver is not an error: nobody waits.
        let _ = self.job_updates.send(JobUpdate {
            job_id: Arc::from(job_id),
            terminal,
        });
    }

    pub(crate) fn subscribe(&self) -> broadcast::Receiver<JobUpdate> {
        self.job_updates.subscribe()
    }

    /// Registers interest in `job_id`: until the guard is dropped, the shared
    /// poller notifies its changes even when another replica commits them.
    pub(crate) fn watch(self: &Arc<Self>, job_id: &str) -> WatchGuard {
        lock(&self.watched)
            .entry(job_id.to_owned())
            .or_default()
            .waiters += 1;
        WatchGuard {
            hub: Arc::clone(self),
            job_id: job_id.to_owned(),
        }
    }

    /// Jobs somebody waits for.
    pub(crate) fn watched_jobs(&self) -> Vec<String> {
        lock(&self.watched).keys().cloned().collect()
    }

    /// Records what the poller read of a watched job, and notifies the
    /// waiters when it differs from the previous read.
    pub(crate) fn observe(&self, job_id: &str, state: String, events: i64, terminal: bool) {
        let changed = {
            let mut watched = lock(&self.watched);
            let Some(entry) = watched.get_mut(job_id) else {
                return;
            };
            let version = Some((state, events));
            let changed = entry.version != version;
            entry.version = version;
            changed
        };
        // The first read counts as a change: a waiter may have registered
        // after its own read of the store.
        if changed {
            self.notify_job(job_id, terminal);
        }
    }
}

/// Keeps a job watched; see [`Hub::watch`].
#[derive(Debug)]
pub(crate) struct WatchGuard {
    hub: Arc<Hub>,
    job_id: String,
}

impl Drop for WatchGuard {
    fn drop(&mut self) {
        let mut watched = lock(&self.hub.watched);
        if let Some(entry) = watched.get_mut(&self.job_id) {
            entry.waiters -= 1;
            if entry.waiters == 0 {
                watched.remove(&self.job_id);
            }
        }
    }
}

/// Waits until `job_id` is notified on `updates` (only by its final state
/// when `terminal_only`). Never returns when the channel is closed: callers
/// always race it against a timer.
pub(crate) async fn job_notified(
    updates: &mut broadcast::Receiver<JobUpdate>,
    job_id: &str,
    terminal_only: bool,
) {
    loop {
        match updates.recv().await {
            Ok(update) if &*update.job_id == job_id && (update.terminal || !terminal_only) => {
                return;
            }
            Ok(_) => {}
            // Missed notifications: let the caller re-read the store.
            Err(broadcast::error::RecvError::Lagged(_)) => return,
            Err(broadcast::error::RecvError::Closed) => std::future::pending::<()>().await,
        }
    }
}

/// The right to drain a node; see [`NodeState`].
pub(crate) struct DrainRole {
    state: Arc<NodeState>,
    held: bool,
}

impl DrainRole {
    /// Takes the role; `None` when another task has it, which will then run
    /// one more pass.
    pub(crate) fn take(state: Arc<NodeState>, full: bool) -> Option<Self> {
        if full {
            state.full.store(true, Ordering::SeqCst);
        }
        // Either become the drainer, or tell the drainer to go again.
        let previous = state
            .phase
            .fetch_update(Ordering::SeqCst, Ordering::SeqCst, |phase| {
                Some(if phase == IDLE {
                    DRAINING
                } else {
                    DRAINING_AGAIN
                })
            })
            .unwrap_or(IDLE);
        (previous == IDLE).then_some(Self { state, held: true })
    }

    /// A pass starts: requests made from now on ask for another one.
    /// Returns whether a full pass was asked for.
    pub(crate) fn begin_pass(&mut self) -> bool {
        self.state.phase.store(DRAINING, Ordering::SeqCst);
        self.state.full.swap(false, Ordering::SeqCst)
    }

    /// True when a request arrived during the pass: the role is kept and
    /// another pass is due. Otherwise the role is released.
    pub(crate) fn again(&mut self) -> bool {
        let released = self
            .state
            .phase
            .compare_exchange(DRAINING, IDLE, Ordering::SeqCst, Ordering::SeqCst)
            .is_ok();
        self.held = !released;
        self.held
    }
}

impl Drop for DrainRole {
    fn drop(&mut self) {
        // Dropped mid-way (shutdown): do not leave the node without drainer.
        if self.held {
            self.state.phase.store(IDLE, Ordering::SeqCst);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[tokio::test]
    async fn watched_jobs_are_notified_on_change_only() {
        let hub = Arc::new(Hub::new(Duration::from_secs(1)));
        let mut updates = hub.subscribe();
        let first = hub.watch("job");
        let second = hub.watch("job");
        assert_eq!(hub.watched_jobs(), ["job"]);

        hub.observe("job", "queued".into(), 1, false);
        hub.observe("job", "queued".into(), 1, false);
        hub.observe("job", "succeeded".into(), 2, true);
        hub.observe("other", "running".into(), 2, false);
        let update = updates.recv().await.unwrap();
        assert_eq!((&*update.job_id, update.terminal), ("job", false));
        let update = updates.recv().await.unwrap();
        assert_eq!((&*update.job_id, update.terminal), ("job", true));
        assert!(updates.try_recv().is_err());

        drop(first);
        assert_eq!(hub.watched_jobs(), ["job"]);
        drop(second);
        assert!(hub.watched_jobs().is_empty());
    }

    #[tokio::test]
    async fn terminal_only_waiters_skip_intermediate_updates() {
        let hub = Hub::new(Duration::from_secs(1));
        let mut updates = hub.subscribe();
        hub.notify_job("job", false);
        hub.notify_job("other", true);
        let waiting = tokio::time::timeout(
            Duration::from_millis(50),
            job_notified(&mut updates, "job", true),
        );
        assert!(waiting.await.is_err());
        hub.notify_job("job", true);
        job_notified(&mut updates, "job", true).await;
    }

    #[tokio::test]
    async fn stalled_session_is_kicked_by_the_send_timeout() {
        let hub = Hub::new(Duration::from_millis(50));
        let (tx, _rx) = mpsc::channel(1);
        let session = SessionHandle::new("s".into(), tx);
        hub.register("node", session.clone());
        // The buffer takes one frame; nobody reads the second one.
        assert!(hub.send("node", &session, ServerFrame::default()).await);
        assert!(!hub.send("node", &session, ServerFrame::default()).await);
        assert!(session.closed.is_cancelled());
        assert!(hub.get("node").is_none());
    }

    #[test]
    fn drain_requests_are_coalesced() {
        let state = Arc::new(NodeState::default());
        let take = |full| DrainRole::take(Arc::clone(&state), full);
        let mut role = take(false).unwrap();
        // While the role is held, other requests only leave a mark.
        assert!(take(true).is_none());
        assert!(role.begin_pass(), "the full request is carried over");
        assert!(take(false).is_none());
        assert!(role.again(), "a request during the pass asks for one more");
        assert!(!role.begin_pass());
        assert!(!role.again());
        drop(role);
        // Released: the next request takes the role, and gives it back if
        // it is dropped before finishing.
        let role = take(false).unwrap();
        drop(role);
        assert!(take(false).is_some());
    }
}
