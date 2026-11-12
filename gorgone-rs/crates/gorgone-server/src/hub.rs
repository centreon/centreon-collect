//! In-memory view of the agent sessions held by this replica.
//!
//! Losing it loses no job: the store knows every job and every session.

use std::collections::HashMap;
use std::sync::{Arc, Mutex, MutexGuard, PoisonError};

use gorgone_proto::agent::ServerFrame;
use tokio::sync::{broadcast, mpsc};
use tokio_util::sync::CancellationToken;
use tonic::Status;

/// Sending side of one agent session.
#[derive(Debug, Clone)]
pub(crate) struct SessionHandle {
    pub(crate) session_id: String,
    pub(crate) tx: mpsc::Sender<Result<ServerFrame, Status>>,
    /// Cancelled when the session is replaced or the server stops.
    pub(crate) closed: CancellationToken,
    /// Serialises drains so that jobs reach the agent in submission order.
    pub(crate) drain_lock: Arc<tokio::sync::Mutex<()>>,
}

/// Local sessions and job notifications.
#[derive(Debug)]
pub(crate) struct Hub {
    sessions: Mutex<HashMap<String, SessionHandle>>,
    job_updates: broadcast::Sender<Arc<str>>,
}

impl Hub {
    pub(crate) fn new() -> Self {
        Self {
            sessions: Mutex::new(HashMap::new()),
            job_updates: broadcast::channel(1024).0,
        }
    }

    fn sessions(&self) -> MutexGuard<'_, HashMap<String, SessionHandle>> {
        // The map stays consistent even if a holder panicked.
        self.sessions.lock().unwrap_or_else(PoisonError::into_inner)
    }

    /// Makes `handle` the session of `node_id`; a previous one is closed.
    pub(crate) fn register(&self, node_id: &str, handle: SessionHandle) {
        if let Some(old) = self.sessions().insert(node_id.to_owned(), handle) {
            old.closed.cancel();
        }
        metrics::gauge!("gorgone_agent_sessions").set(self.sessions().len() as f64);
    }

    /// Forgets the session of `node_id` if it is still `session_id`.
    pub(crate) fn remove(&self, node_id: &str, session_id: &str) {
        let mut sessions = self.sessions();
        if sessions
            .get(node_id)
            .is_some_and(|h| h.session_id == session_id)
        {
            sessions.remove(node_id);
        }
        metrics::gauge!("gorgone_agent_sessions").set(sessions.len() as f64);
    }

    /// Closes and forgets the session of `node_id`, whatever it is.
    pub(crate) fn kick(&self, node_id: &str) {
        if let Some(old) = self.sessions().remove(node_id) {
            old.closed.cancel();
        }
    }

    pub(crate) fn get(&self, node_id: &str) -> Option<SessionHandle> {
        self.sessions().get(node_id).cloned()
    }

    /// Nodes with a session on this replica.
    pub(crate) fn nodes(&self) -> Vec<String> {
        self.sessions().keys().cloned().collect()
    }

    /// Closes every session (shutdown).
    pub(crate) fn close_all(&self) {
        for (_, handle) in self.sessions().drain() {
            handle.closed.cancel();
        }
    }

    /// Tells local waiters that `job_id` changed.
    pub(crate) fn notify_job(&self, job_id: &str) {
        // No receiver is not an error: nobody waits.
        let _ = self.job_updates.send(Arc::from(job_id));
    }

    pub(crate) fn subscribe(&self) -> broadcast::Receiver<Arc<str>> {
        self.job_updates.subscribe()
    }
}

/// Waits until `job_id` is notified on `updates`. Never returns when the
/// channel is closed: callers always race it against a timer.
pub(crate) async fn job_notified(updates: &mut broadcast::Receiver<Arc<str>>, job_id: &str) {
    loop {
        match updates.recv().await {
            Ok(id) if &*id == job_id => return,
            Ok(_) => {}
            // Missed notifications: let the caller re-read the store.
            Err(broadcast::error::RecvError::Lagged(_)) => return,
            Err(broadcast::error::RecvError::Closed) => std::future::pending::<()>().await,
        }
    }
}
