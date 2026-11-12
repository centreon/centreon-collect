//! Job submission, dispatch to local sessions and background sweeps.

use std::sync::Arc;
use std::time::{Duration, Instant};

use gorgone_core::kind;
use gorgone_proto::agent::action::Kind;
use gorgone_proto::agent::server_frame::Frame;
use gorgone_proto::agent::{Action, Cancel, Dispatch, Job, ServerFrame};
use prost::Message;
use tokio_util::sync::CancellationToken;
use tracing::{debug, info, warn};
use uuid::Uuid;

use crate::error::Error;
use crate::events;
use crate::hub::{Hub, job_notified};
use crate::peer::PeerClients;
use crate::store::{JobRow, Store, now_ms, state};

/// Jobs claimed per store round trip while draining a node.
const DRAIN_BATCH: i64 = 100;
/// Extra time left to an agent after the deadline before the job expires.
const EXPIRE_GRACE_MS: i64 = 30_000;
/// Period and lease name of the leader-only purge.
const PURGE_PERIOD: Duration = Duration::from_secs(60);
const PURGE_LEASE: &str = "purge";
/// Store poll period of a waiter that gets no local notification.
const WAIT_POLL: Duration = Duration::from_millis(250);

/// Settings of the dispatcher.
#[derive(Debug, Clone)]
pub(crate) struct DispatcherConfig {
    pub(crate) replica_id: String,
    pub(crate) peer_addr: String,
    pub(crate) default_deadline: Duration,
    pub(crate) sweep_interval: Duration,
    pub(crate) retention: Duration,
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
        }
    }

    /// Commits a job for `node_id`, then pushes it if a session is reachable.
    /// Returns the job id (the legacy "token").
    pub(crate) async fn submit(
        &self,
        node_id: &str,
        action: Action,
        deadline: Option<Duration>,
    ) -> Result<String, Error> {
        if node_id.is_empty() || node_id.len() > 191 {
            return Err(Error::Invalid("invalid node id".into()));
        }
        let kind = kind_name(&action)?;
        let id = Uuid::now_v7().to_string();
        let deadline = deadline.unwrap_or(self.config.default_deadline);
        let deadline_at =
            now_ms().saturating_add(i64::try_from(deadline.as_millis()).unwrap_or(i64::MAX));
        self.store
            .insert_job(
                &id,
                node_id,
                kind,
                &action.encode_to_vec(),
                deadline_at,
                &events::accepted().to_string(),
            )
            .await?;
        metrics::counter!("gorgone_jobs_submitted_total", "kind" => kind).increment(1);
        debug!(job = %id, node = node_id, kind, "job queued");
        // The job is committed: delivery problems from here are not errors.
        self.route(node_id).await;
        Ok(id)
    }

    /// Local session: push now. Session on a peer: wake it. Else stay queued.
    async fn route(&self, node_id: &str) {
        if self.drain(node_id).await {
            return;
        }
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

    /// Claims and sends the queued jobs of `node_id` when its session is held
    /// by this replica. Returns whether it is.
    pub(crate) async fn drain(&self, node_id: &str) -> bool {
        let Some(session) = self.hub.get(node_id) else {
            return false;
        };
        let _guard = session.drain_lock.lock().await;
        self.send_cancels(node_id, &session.tx).await;
        loop {
            let jobs = match self.store.queued_jobs(node_id, DRAIN_BATCH).await {
                Ok(jobs) => jobs,
                Err(err) => {
                    warn!(node = node_id, error = %err, "listing queued jobs failed");
                    return true;
                }
            };
            if jobs.is_empty() {
                return true;
            }
            for job in jobs {
                match self.dispatch(&session.session_id, &session.tx, job).await {
                    Ok(true) => {}
                    // Session gone: its jobs stay queued for the next one.
                    Ok(false) => return true,
                    Err(err) => {
                        warn!(node = node_id, error = %err, "dispatch failed");
                        return true;
                    }
                }
            }
        }
    }

    /// Sends the `Cancel` frames asked for jobs running on this session.
    async fn send_cancels(
        &self,
        node_id: &str,
        tx: &tokio::sync::mpsc::Sender<Result<ServerFrame, tonic::Status>>,
    ) {
        let pending = match self.store.pending_cancels(node_id).await {
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
            if tx.send(Ok(frame)).await.is_err() {
                return;
            }
            if let Err(err) = self.store.mark_cancel_sent(&job_id).await {
                // Sent again at the next drain: the agent ignores a repeat.
                warn!(job = %job_id, error = %err, "cancellation not recorded as sent");
            }
            debug!(job = %job_id, node = node_id, "cancel sent");
        }
    }

    /// Cancels a job: at once when still queued, otherwise by asking the
    /// agent through the replica holding its session.
    pub(crate) async fn cancel(&self, id: &str) -> Result<CancelOutcome, Error> {
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
            self.hub.notify_job(id);
            return Ok(CancelOutcome::Cancelled);
        }
        if self.store.request_cancel(id).await? {
            // Local session: sent now. Peer: woken up, or its next sweep.
            self.route(&job.node_id).await;
            return Ok(CancelOutcome::Requested);
        }
        Ok(CancelOutcome::AlreadyFinished)
    }

    /// Returns false when the session no longer accepts frames.
    async fn dispatch(
        &self,
        session_id: &str,
        tx: &tokio::sync::mpsc::Sender<Result<ServerFrame, tonic::Status>>,
        job: JobRow,
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
        if !self.store.claim_job(&job.id, session_id).await? {
            return Ok(true);
        }
        let frame = ServerFrame {
            frame: Some(Frame::Dispatch(Dispatch {
                job: Some(Job {
                    id: job.id.clone(),
                    deadline_unix_ms: job.deadline_at,
                    action: Some(action),
                }),
            })),
        };
        if tx.send(Ok(frame)).await.is_err() {
            self.store.unclaim_job(&job.id, session_id).await?;
            return Ok(false);
        }
        metrics::counter!("gorgone_jobs_dispatched_total").increment(1);
        debug!(job = %job.id, session = session_id, "job dispatched");
        Ok(true)
    }

    /// Waits until the job is terminal or `timeout` elapsed, and returns its
    /// last known row. Local notifications wake the waiter early; otherwise
    /// the store is polled.
    pub(crate) async fn wait_job(
        &self,
        id: &str,
        timeout: Duration,
    ) -> Result<Option<JobRow>, Error> {
        let mut updates = self.hub.subscribe();
        let until = Instant::now() + timeout;
        loop {
            let Some(job) = self.store.job(id).await? else {
                return Ok(None);
            };
            let left = until.saturating_duration_since(Instant::now());
            if state::is_terminal(&job.state) || left.is_zero() {
                return Ok(Some(job));
            }
            let _ = tokio::time::timeout(left.min(WAIT_POLL), job_notified(&mut updates, id)).await;
        }
    }

    /// Background loop of the replica, until `shutdown`.
    pub(crate) async fn run(self: Arc<Self>, shutdown: CancellationToken) {
        let mut tick = tokio::time::interval(self.config.sweep_interval);
        tick.set_missed_tick_behavior(tokio::time::MissedTickBehavior::Delay);
        let mut last_purge: Option<Instant> = None;
        loop {
            tokio::select! {
                () = shutdown.cancelled() => return,
                _ = tick.tick() => {}
            }
            self.sweep().await;
            if last_purge.is_none_or(|at| at.elapsed() >= PURGE_PERIOD) {
                last_purge = Some(Instant::now());
                self.purge().await;
            }
        }
    }

    async fn sweep(&self) {
        let store = &self.store;
        if let Err(err) = store
            .heartbeat_replica(&self.config.replica_id, &self.config.peer_addr)
            .await
        {
            warn!(error = %err, "replica heartbeat failed");
        }
        // A replica is dead after several missed sweeps, with a floor that
        // tolerates a slow store.
        let dead_after = (self.config.sweep_interval * 5).max(Duration::from_secs(10));
        let cutoff = now_ms().saturating_sub(i64::try_from(dead_after.as_millis()).unwrap_or(0));
        match store.drop_dead_replicas(cutoff).await {
            Ok(0) => {}
            Ok(dropped) => info!(dropped, "dropped sessions of dead replicas"),
            Err(err) => warn!(error = %err, "dropping dead replicas failed"),
        }
        match store.requeue_orphans().await {
            Ok(0) => {}
            Ok(requeued) => info!(requeued, "requeued jobs of lost sessions"),
            Err(err) => warn!(error = %err, "requeue failed"),
        }
        match store
            .expire_jobs(EXPIRE_GRACE_MS, &events::expired().to_string())
            .await
        {
            Ok(expired) => {
                for id in &expired {
                    metrics::counter!("gorgone_jobs_finished_total", "state" => state::EXPIRED)
                        .increment(1);
                    self.hub.notify_job(id);
                }
            }
            Err(err) => warn!(error = %err, "expiring jobs failed"),
        }
        match store
            .cancel_requeued(&events::cancelled().to_string())
            .await
        {
            Ok(cancelled) => {
                for id in &cancelled {
                    metrics::counter!("gorgone_jobs_finished_total", "state" => state::CANCELLED)
                        .increment(1);
                    self.hub.notify_job(id);
                }
            }
            Err(err) => warn!(error = %err, "cancelling requeued jobs failed"),
        }
        for node_id in self.hub.nodes() {
            self.drain(&node_id).await;
        }
    }

    /// Singleton work: only the replica holding the lease purges.
    async fn purge(&self) {
        let ttl = i64::try_from((PURGE_PERIOD * 2).as_millis()).unwrap_or(i64::MAX);
        match self
            .store
            .acquire_lease(PURGE_LEASE, &self.config.replica_id, ttl)
            .await
        {
            Ok(true) => {}
            Ok(false) => return,
            Err(err) => {
                warn!(error = %err, "purge lease failed");
                return;
            }
        }
        let retention = i64::try_from(self.config.retention.as_millis()).unwrap_or(i64::MAX);
        match self.store.purge(now_ms().saturating_sub(retention)).await {
            Ok(0) => {}
            Ok(purged) => info!(purged, "purged old jobs"),
            Err(err) => warn!(error = %err, "purge failed"),
        }
        // Peers come and go: do not keep channels to addresses never reused.
        self.peers.clear();
    }
}
