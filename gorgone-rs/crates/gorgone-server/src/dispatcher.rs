//! Job submission, dispatch to local sessions and background work.

use std::collections::HashMap;
use std::sync::atomic::Ordering;
use std::sync::{Arc, Mutex, PoisonError};
use std::time::{Duration, Instant};

use futures_util::StreamExt;
use gorgone_core::kind;
use gorgone_proto::agent::action::Kind;
use gorgone_proto::agent::server_frame::Frame;
use gorgone_proto::agent::{Action, Cancel, Dispatch, Job, ServerFrame};
use prost::Message;
use tokio::task::JoinHandle;
use tokio_util::sync::CancellationToken;
use tracing::{debug, info, warn};
use uuid::Uuid;

use crate::error::Error;
use crate::events;
use crate::hub::{DrainRole, Hub, SessionHandle, job_notified};
use crate::peer::PeerClients;
use crate::store::{JobRow, NewJob, Store, state};

/// Jobs claimed by one statement while draining a node.
const DRAIN_BATCH: i64 = 128;
/// Nodes drained at the same time by one sweep.
const DRAIN_CONCURRENCY: usize = 16;
/// Extra time left to an agent after the deadline before the job expires.
const EXPIRE_GRACE_MS: i64 = 30_000;
/// Period and lease name of the leader-only purge.
const PURGE_PERIOD: Duration = Duration::from_secs(60);
const PURGE_LEASE: &str = "purge";
/// Batches of jobs one purge run may delete.
const PURGE_BATCHES: usize = 20;
/// Period of the replica heartbeat. Fixed, so that every replica judges the
/// liveness of the others with the same numbers.
const REPLICA_HEARTBEAT: Duration = Duration::from_secs(2);
/// A replica silent for this long is dead and loses its sessions.
const REPLICA_DEAD_AFTER_MS: i64 = 15_000;
/// Heartbeats between two clock re-synchronisations and peer cache prunings.
const HOUSEKEEPING_EVERY: u32 = 15;
/// Period of the shared poller serving the waiters of jobs whose events are
/// committed by another replica.
const WATCH_POLL: Duration = Duration::from_millis(250);
/// Longest sleep of a waiter before it re-reads the store on its own: a
/// safety net, not the wake-up path.
const WAIT_RECHECK: Duration = Duration::from_secs(5);
/// How long a node found enrolled is trusted to be, sparing one lookup per
/// submit. A node removed on another replica keeps getting jobs for that
/// long; they stay queued and expire.
const NODE_CACHE_TTL: Duration = Duration::from_secs(10);

/// Settings of the dispatcher.
#[derive(Debug, Clone)]
pub(crate) struct DispatcherConfig {
    pub(crate) replica_id: String,
    pub(crate) peer_addr: String,
    pub(crate) default_deadline: Duration,
    pub(crate) sweep_interval: Duration,
    pub(crate) retention: Duration,
    /// Accept jobs for nodes that are not enrolled (development).
    pub(crate) auto_enroll: bool,
}

/// Result of a cancellation request.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub(crate) enum CancelOutcome {
    /// The job was queued and is now `cancelled`.
    Cancelled,
    /// The job is on an agent, which is asked to stop it.
    Requested,
    /// The job is already terminal.
    AlreadyFinished,
    /// No such job.
    NotFound,
}

/// Owns the submit path and the periodic work of a replica.
#[derive(Debug)]
pub(crate) struct Dispatcher {
    store: Store,
    hub: Arc<Hub>,
    peers: PeerClients,
    config: DispatcherConfig,
    /// Nodes recently seen enrolled.
    known_nodes: Mutex<HashMap<String, Instant>>,
}

fn kind_name(action: &Action) -> Result<&'static str, Error> {
    match &action.kind {
        Some(Kind::RunCommands(run)) => {
            if run.commands.is_empty() || run.commands.iter().any(|c| c.command.is_empty()) {
                return Err(Error::Invalid(
                    "run_commands needs non-empty commands".into(),
                ));
            }
            Ok(kind::RUN_COMMANDS)
        }
        Some(Kind::EngineCommands(engine)) => {
            if engine.commands.is_empty() {
                return Err(Error::Invalid("engine_commands needs commands".into()));
            }
            Ok(kind::ENGINE_COMMANDS)
        }
        Some(Kind::ApplyConfig(apply)) => {
            if apply.target.is_empty() || apply.digest.is_empty() {
                return Err(Error::Invalid(
                    "apply_config needs target and digest".into(),
                ));
            }
            Ok(kind::APPLY_CONFIG)
        }
        Some(Kind::ServiceControl(control)) => {
            if control.service.is_empty() || control.operation == 0 {
                return Err(Error::Invalid(
                    "service_control needs service and operation".into(),
                ));
            }
            Ok(kind::SERVICE_CONTROL)
        }
        None => Err(Error::Invalid("action without kind".into())),
    }
}

fn millis(duration: Duration) -> i64 {
    i64::try_from(duration.as_millis()).unwrap_or(i64::MAX)
}

impl Dispatcher {
    pub(crate) fn new(
        store: Store,
        hub: Arc<Hub>,
        peers: PeerClients,
        config: DispatcherConfig,
    ) -> Self {
        Self {
            store,
            hub,
            peers,
            config,
            known_nodes: Mutex::default(),
        }
    }

    /// Commits a job for `node_id`, then pushes it if a session is reachable.
    /// Returns the job id (the legacy "token").
    pub(crate) async fn submit(
        self: &Arc<Self>,
        node_id: &str,
        action: Action,
        deadline: Option<Duration>,
    ) -> Result<String, Error> {
        let id = Uuid::now_v7().to_string();
        self.commit(&id, false, node_id, action, deadline).await?;
        Ok(id)
    }

    /// Same as [`Self::submit`] with a caller-chosen id: submitting the same
    /// id again does nothing. Returns whether the job was created.
    pub(crate) async fn submit_with_id(
        self: &Arc<Self>,
        id: &str,
        node_id: &str,
        action: Action,
        deadline: Option<Duration>,
    ) -> Result<bool, Error> {
        self.commit(id, true, node_id, action, deadline).await
    }

    async fn commit(
        self: &Arc<Self>,
        id: &str,
        may_exist: bool,
        node_id: &str,
        action: Action,
        deadline: Option<Duration>,
    ) -> Result<bool, Error> {
        if node_id.is_empty() || node_id.len() > 191 {
            return Err(Error::Invalid("invalid node id".into()));
        }
        let kind = kind_name(&action)?;
        if !self.accepts_node(node_id).await? {
            return Err(Error::UnknownNode(node_id.to_owned()));
        }
        let blob_digest = match &action.kind {
            Some(Kind::ApplyConfig(apply)) => Some(apply.digest.as_str()),
            _ => None,
        };
        let created = self
            .store
            .insert_job(NewJob {
                id,
                node_id,
                kind,
                action: &action.encode_to_vec(),
                ttl_ms: millis(deadline.unwrap_or(self.config.default_deadline)),
                blob_digest,
                accepted_data: &events::accepted().to_string(),
                may_exist,
            })
            .await?;
        if created {
            metrics::counter!("gorgone_jobs_submitted_total", "kind" => kind).increment(1);
            debug!(job = id, node = node_id, kind, "job queued");
            // The job is committed: delivery happens aside, so that a slow
            // agent never delays the submitter.
            self.trigger(node_id, false);
        }
        Ok(created)
    }

    /// True when jobs may be submitted for `node_id`.
    pub(crate) async fn accepts_node(&self, node_id: &str) -> Result<bool, Error> {
        if self.config.auto_enroll {
            return Ok(true);
        }
        let fresh = |seen: &Instant| seen.elapsed() < NODE_CACHE_TTL;
        if self
            .known_nodes
            .lock()
            .unwrap_or_else(PoisonError::into_inner)
            .get(node_id)
            .is_some_and(fresh)
        {
            return Ok(true);
        }
        let exists = self.store.node_exists(node_id).await?;
        if exists {
            let mut known = self
                .known_nodes
                .lock()
                .unwrap_or_else(PoisonError::into_inner);
            if known.len() > 10_000 {
                known.retain(|_, seen| fresh(seen));
            }
            known.insert(node_id.to_owned(), Instant::now());
        }
        Ok(exists)
    }

    /// Drops what is remembered of a node that was just removed here.
    pub(crate) fn forget_node(&self, node_id: &str) {
        self.known_nodes
            .lock()
            .unwrap_or_else(PoisonError::into_inner)
            .remove(node_id);
    }

    /// Starts, in the background, the delivery work of a node: a drain when
    /// its session is here, a wake-up of the replica holding it otherwise.
    /// `full` also looks at cancellations and at jobs of a former session.
    pub(crate) fn trigger(self: &Arc<Self>, node_id: &str, full: bool) {
        let this = Arc::clone(self);
        let node_id = node_id.to_owned();
        if self.holds(&node_id) {
            // No task when a drain is already under way: it goes again.
            if let Some(role) = DrainRole::take(self.hub.node_state(&node_id), full) {
                tokio::spawn(async move { this.run_drain(role, &node_id).await });
            }
        } else {
            tokio::spawn(async move { this.wake_owner(&node_id).await });
        }
    }

    /// True when this replica holds the session of the node.
    pub(crate) fn holds(&self, node_id: &str) -> bool {
        self.hub.get(node_id).is_some()
    }

    /// Session on a peer: wake it. No session: the job stays queued.
    async fn wake_owner(&self, node_id: &str) {
        let session = match self.store.session(node_id).await {
            Ok(session) => session,
            Err(err) => {
                warn!(node = node_id, error = %err, "session lookup failed");
                return;
            }
        };
        let Some(session) = session.filter(|s| s.replica_id != self.config.replica_id) else {
            return;
        };
        let woke = async {
            let addr = self.store.replica_addr(&session.replica_id).await?;
            match addr {
                Some(addr) => self.peers.wake(&addr, node_id).await,
                None => Ok(false),
            }
        };
        match woke.await {
            Ok(true) => {}
            // The owner sweeps its nodes anyway.
            Ok(false) => debug!(node = node_id, "peer does not hold the node any more"),
            Err(err) => warn!(node = node_id, error = %err, "peer wake-up failed"),
        }
    }

    /// Drains the node now, unless another task is doing it (it will then
    /// run one more pass). Used by the sweep.
    async fn drain(&self, node_id: &str, full: bool) {
        if let Some(role) = DrainRole::take(self.hub.node_state(node_id), full) {
            self.run_drain(role, node_id).await;
        }
    }

    /// Runs passes until no request is left: requests arriving while a pass
    /// commits are served together by the next one (group commit).
    async fn run_drain(&self, mut role: DrainRole, node_id: &str) {
        loop {
            let full = role.begin_pass();
            self.drain_pass(node_id, full).await;
            if !role.again() {
                return;
            }
        }
    }

    /// One pass: sends to the session of the node the pending cancellations
    /// (full pass), then the queued jobs in submission order.
    async fn drain_pass(&self, node_id: &str, full: bool) {
        // Read while holding the drain role: this is the session to serve.
        let Some(session) = self.hub.get(node_id) else {
            return;
        };
        // A new session first gets what its predecessor left.
        let first = !session.primed.swap(true, Ordering::SeqCst);
        if full || first {
            // Jobs handed to a former session that never answered go first.
            match self
                .store
                .requeue_stale_dispatched(node_id, &session.session_id)
                .await
            {
                Ok(0) => {}
                Ok(requeued) => {
                    info!(
                        node = node_id,
                        requeued, "requeued jobs of a former session"
                    );
                }
                Err(err) => warn!(node = node_id, error = %err, "requeue of stale jobs failed"),
            }
            self.send_cancels(node_id, &session).await;
        }
        loop {
            if session.closed.is_cancelled() {
                return;
            }
            // One statement claims the whole batch, in order.
            let (jobs, listed) = match self
                .store
                .claim_jobs(node_id, &session.session_id, DRAIN_BATCH)
                .await
            {
                Ok(claimed) => claimed,
                Err(err) => {
                    warn!(node = node_id, error = %err, "claiming queued jobs failed");
                    return;
                }
            };
            if listed > 0 && jobs.is_empty() && !self.still_current(node_id, &session).await {
                return;
            }
            let mut jobs = jobs.into_iter();
            while let Some(job) = jobs.next() {
                match self.send_job(node_id, &session, &job).await {
                    Ok(true) => {}
                    // Session gone, or store failure: what was claimed and
                    // not sent goes back to the queue for the next session.
                    Ok(false) | Err(_) => {
                        for unsent in std::iter::once(job).chain(jobs) {
                            if let Err(err) = self
                                .store
                                .unclaim_job(&unsent.id, &session.session_id)
                                .await
                            {
                                warn!(job = %unsent.id, error = %err, "job not given back");
                            }
                        }
                        return;
                    }
                }
            }
            // A short batch emptied the queue: later submits ask for a pass.
            if (listed as i64) < DRAIN_BATCH {
                return;
            }
        }
    }

    /// False, after closing the local session, when the store no longer
    /// knows it: the node reconnected elsewhere, or the session was dropped.
    async fn still_current(&self, node_id: &str, session: &SessionHandle) -> bool {
        match self.store.session(node_id).await {
            Ok(current)
                if current
                    .as_ref()
                    .is_some_and(|s| s.session_id == session.session_id) =>
            {
                true
            }
            Ok(_) => {
                info!(node = node_id, session = %session.session_id, "session is not current any more, closing it");
                self.hub.kick_session(node_id, session);
                false
            }
            Err(err) => {
                warn!(node = node_id, error = %err, "session lookup failed");
                false
            }
        }
    }

    /// Sends the `Cancel` frames this session has not received yet.
    async fn send_cancels(&self, node_id: &str, session: &SessionHandle) {
        let pending = match self
            .store
            .pending_cancels(node_id, &session.session_id)
            .await
        {
            Ok(pending) => pending,
            Err(err) => {
                warn!(node = node_id, error = %err, "listing cancellations failed");
                return;
            }
        };
        for job_id in pending {
            let frame = ServerFrame {
                frame: Some(Frame::Cancel(Cancel {
                    job_id: job_id.clone(),
                })),
            };
            if !self.hub.send(node_id, session, frame).await {
                return;
            }
            if let Err(err) = self
                .store
                .mark_cancel_sent(&job_id, &session.session_id)
                .await
            {
                // Sent again at the next drain: the agent ignores a repeat.
                warn!(job = %job_id, error = %err, "cancellation not recorded as sent");
            }
            debug!(job = %job_id, node = node_id, "cancel sent");
        }
    }

    /// Cancels a job: at once when still queued, otherwise by asking the
    /// agent through the replica holding its session.
    pub(crate) async fn cancel(self: &Arc<Self>, id: &str) -> Result<CancelOutcome, Error> {
        let Some(job) = self.store.job(id).await? else {
            return Ok(CancelOutcome::NotFound);
        };
        if self
            .store
            .cancel_queued(id, &events::cancelled().to_string())
            .await?
        {
            metrics::counter!("gorgone_jobs_finished_total", "state" => state::CANCELLED)
                .increment(1);
            self.hub.notify_job(id, true);
            return Ok(CancelOutcome::Cancelled);
        }
        if self.store.request_cancel(id).await? {
            // Local session: sent now. Peer: woken up, or its next sweep.
            self.trigger(&job.node_id, true);
            return Ok(CancelOutcome::Requested);
        }
        Ok(CancelOutcome::AlreadyFinished)
    }

    /// Sends a claimed job. Returns false when the session did not take it.
    async fn send_job(
        &self,
        node_id: &str,
        session: &SessionHandle,
        job: &JobRow,
    ) -> Result<bool, Error> {
        let action = match Action::decode(job.action.as_slice()) {
            Ok(action) => action,
            Err(err) => {
                // Never deliverable: fail it instead of retrying forever.
                warn!(job = %job.id, error = %err, "corrupt stored action");
                self.store.finish_job(&job.id, state::FAILED).await?;
                return Ok(true);
            }
        };
        // Relative time left: the agent clock need not agree with ours.
        let ttl_ms = job.deadline_at.saturating_sub(self.store.now_ms()).max(1);
        let frame = ServerFrame {
            frame: Some(Frame::Dispatch(Dispatch {
                job: Some(Job {
                    id: job.id.clone(),
                    deadline_unix_ms: job.deadline_at,
                    action: Some(action),
                    ttl_ms: u64::try_from(ttl_ms).unwrap_or(1),
                }),
            })),
        };
        if !self.hub.send(node_id, session, frame).await {
            return Ok(false);
        }
        metrics::counter!("gorgone_jobs_dispatched_total").increment(1);
        debug!(job = %job.id, session = %session.session_id, "job dispatched");
        Ok(true)
    }

    /// Waits until the job is terminal or `timeout` elapsed, and returns its
    /// last known row. A waiter sleeps until the job is notified terminal:
    /// by the local commit itself when the session is here (no polling
    /// period involved), by the shared poller otherwise.
    pub(crate) async fn wait_job(
        &self,
        id: &str,
        timeout: Duration,
    ) -> Result<Option<JobRow>, Error> {
        let mut updates = self.hub.subscribe();
        let mut watch = None;
        let until = Instant::now() + timeout;
        loop {
            let Some(job) = self.store.job(id).await? else {
                return Ok(None);
            };
            let left = until.saturating_duration_since(Instant::now());
            if state::is_terminal(&job.state) || left.is_zero() {
                return Ok(Some(job));
            }
            // Only a job whose events are committed elsewhere needs the
            // shared poller; re-evaluated at each wake-up, the session may move.
            if watch.is_none() && !self.holds(&job.node_id) {
                watch = Some(self.hub.watch(id));
            }
            let nap = left.min(WAIT_RECHECK);
            let _ = tokio::time::timeout(nap, job_notified(&mut updates, id, true)).await;
        }
    }

    /// Spawns the background tasks of the replica; they stop on `shutdown`.
    pub(crate) fn spawn_background(
        self: &Arc<Self>,
        shutdown: &CancellationToken,
    ) -> Vec<JoinHandle<()>> {
        vec![
            tokio::spawn(Arc::clone(self).heartbeat_loop(shutdown.clone())),
            tokio::spawn(Arc::clone(self).sweep_loop(shutdown.clone())),
            tokio::spawn(Arc::clone(self).purge_loop(shutdown.clone())),
            tokio::spawn(Arc::clone(self).watch_loop(shutdown.clone())),
        ]
    }

    /// Replica liveness, on its own task: nothing the sweep does can delay it.
    async fn heartbeat_loop(self: Arc<Self>, shutdown: CancellationToken) {
        let mut tick = tokio::time::interval(REPLICA_HEARTBEAT);
        tick.set_missed_tick_behavior(tokio::time::MissedTickBehavior::Delay);
        let mut beats = 0u32;
        loop {
            tokio::select! {
                () = shutdown.cancelled() => return,
                _ = tick.tick() => {}
            }
            if let Err(err) = self
                .store
                .heartbeat_replica(&self.config.replica_id, &self.config.peer_addr)
                .await
            {
                warn!(error = %err, "replica heartbeat failed");
            }
            beats = beats.wrapping_add(1);
            if beats.is_multiple_of(HOUSEKEEPING_EVERY) {
                if let Err(err) = self.store.sync_clock().await {
                    warn!(error = %err, "clock synchronisation failed");
                }
                // Replicas come and go: forget the channels to those that left.
                match self.store.replica_addrs().await {
                    Ok(addrs) => self.peers.retain(&addrs),
                    Err(err) => warn!(error = %err, "listing replicas failed"),
                }
            }
        }
    }

    async fn sweep_loop(self: Arc<Self>, shutdown: CancellationToken) {
        let mut tick = tokio::time::interval(self.config.sweep_interval);
        tick.set_missed_tick_behavior(tokio::time::MissedTickBehavior::Delay);
        loop {
            tokio::select! {
                () = shutdown.cancelled() => return,
                _ = tick.tick() => {}
            }
            self.sweep().await;
        }
    }

    async fn sweep(&self) {
        let store = &self.store;
        match store.drop_dead_replicas(REPLICA_DEAD_AFTER_MS).await {
            Ok(0) => {}
            Ok(dropped) => info!(dropped, "dropped sessions of dead replicas"),
            Err(err) => warn!(error = %err, "dropping dead replicas failed"),
        }
        match store.requeue_orphans().await {
            Ok(0) => {}
            Ok(requeued) => info!(requeued, "requeued jobs of lost sessions"),
            Err(err) => warn!(error = %err, "requeue failed"),
        }
        let finished = [
            (
                state::EXPIRED,
                store
                    .expire_jobs(EXPIRE_GRACE_MS, &events::expired().to_string())
                    .await,
            ),
            (
                state::CANCELLED,
                store
                    .cancel_requeued(&events::cancelled().to_string())
                    .await,
            ),
        ];
        for (state, ids) in finished {
            match ids {
                Ok(ids) => {
                    for id in &ids {
                        metrics::counter!("gorgone_jobs_finished_total", "state" => state)
                            .increment(1);
                        self.hub.notify_job(id, true);
                    }
                }
                Err(err) => warn!(state, error = %err, "sweep transition failed"),
            }
        }
        // Concurrently: one slow node does not delay the others.
        futures_util::stream::iter(self.hub.nodes())
            .for_each_concurrent(DRAIN_CONCURRENCY, |node_id| async move {
                self.drain(&node_id, true).await;
            })
            .await;
    }

    /// Singleton work: only the replica holding the lease purges.
    async fn purge_loop(self: Arc<Self>, shutdown: CancellationToken) {
        let mut tick = tokio::time::interval(PURGE_PERIOD);
        tick.set_missed_tick_behavior(tokio::time::MissedTickBehavior::Delay);
        loop {
            tokio::select! {
                () = shutdown.cancelled() => return,
                _ = tick.tick() => {}
            }
            match self
                .store
                .acquire_lease(
                    PURGE_LEASE,
                    &self.config.replica_id,
                    millis(PURGE_PERIOD * 2),
                )
                .await
            {
                Ok(true) => {}
                Ok(false) => continue,
                Err(err) => {
                    warn!(error = %err, "purge lease failed");
                    continue;
                }
            }
            match self
                .store
                .purge(millis(self.config.retention), PURGE_BATCHES)
                .await
            {
                Ok(0) => {}
                Ok(purged) => info!(purged, "purged old jobs"),
                Err(err) => warn!(error = %err, "purge failed"),
            }
        }
    }

    /// One poller for all the waiters of this replica: it reads the watched
    /// jobs in one batch and notifies those that changed.
    async fn watch_loop(self: Arc<Self>, shutdown: CancellationToken) {
        let mut tick = tokio::time::interval(WATCH_POLL);
        tick.set_missed_tick_behavior(tokio::time::MissedTickBehavior::Delay);
        loop {
            tokio::select! {
                () = shutdown.cancelled() => return,
                _ = tick.tick() => {}
            }
            let watched = self.hub.watched_jobs();
            if watched.is_empty() {
                continue;
            }
            match self.store.job_versions(&watched).await {
                Ok(versions) => {
                    for (id, state, events) in versions {
                        let terminal = state::is_terminal(&state);
                        self.hub.observe(&id, state, events, terminal);
                    }
                }
                Err(err) => warn!(error = %err, "reading watched jobs failed"),
            }
        }
    }
}
