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

use crate::error::Error;
use crate::events;
use crate::hub::{DrainRole, Hub, SessionHandle, job_notified};
use crate::metrics;
use crate::peer::PeerClients;
use crate::store::{
    FAILED_SEQ, JobRow, LatestConfig, MAX_REQUIRES, NewJob, Store, Termination, Unmet, state,
};

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
    /// Deadline of a `ValidateConfig` job submitted without one.
    pub(crate) validation_deadline: Duration,
    pub(crate) sweep_interval: Duration,
    pub(crate) retention: Duration,
    /// Accept jobs for nodes that are not enrolled (development).
    pub(crate) auto_enroll: bool,
}

/// Optional parts of a submission.
#[derive(Debug, Clone, Copy, Default)]
pub(crate) struct SubmitOptions<'a> {
    /// Caller-chosen id: submitting it again does nothing.
    pub(crate) id: Option<&'a str>,
    /// Jobs of the node that must succeed before this one is dispatched.
    pub(crate) requires: &'a [String],
    /// Latest `ApplyConfig` jobs of the node to require as well.
    pub(crate) latest_config: LatestConfig<'a>,
    /// Time the job may live; the server default when absent.
    pub(crate) deadline: Option<Duration>,
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
            // Stored as a key of the latest push per target.
            if apply.target.len() > 191 {
                return Err(Error::Invalid("apply_config target is too long".into()));
            }
            Ok(kind::APPLY_CONFIG)
        }
        Some(Kind::ValidateConfig(validate)) => {
            if validate.target.is_empty() || validate.digest.is_empty() {
                return Err(Error::Invalid(
                    "validate_config needs target and digest".into(),
                ));
            }
            if validate.target.len() > 191 {
                return Err(Error::Invalid("validate_config target is too long".into()));
            }
            Ok(kind::VALIDATE_CONFIG)
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
        let options = SubmitOptions {
            deadline,
            ..SubmitOptions::default()
        };
        Ok(self.submit_job(node_id, action, options).await?.0)
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
        let options = SubmitOptions {
            id: Some(id),
            deadline,
            ..SubmitOptions::default()
        };
        Ok(self.submit_job(node_id, action, options).await?.1)
    }

    /// Commits a job for `node_id`, then pushes it if a session is reachable.
    /// Returns the job id and whether the job was created: false means a
    /// job with the id given in `options` already existed, of any node, and
    /// nothing was changed.
    pub(crate) async fn submit_job(
        self: &Arc<Self>,
        node_id: &str,
        action: Action,
        options: SubmitOptions<'_>,
    ) -> Result<(String, bool), Error> {
        if node_id.is_empty() || node_id.len() > 191 {
            return Err(Error::Invalid("invalid node id".into()));
        }
        let kind = kind_name(&action)?;
        if !self.accepts_node(node_id).await? {
            return Err(Error::UnknownNode(node_id.to_owned()));
        }
        let id = match options.id {
            // Before the prerequisites are checked: a retry must get the
            // same answer even when one of them was purged since.
            Some(id) if self.store.job(id).await?.is_some() => return Ok((id.to_owned(), false)),
            Some(id) => id.to_owned(),
            None => crate::random_id()?,
        };
        if let LatestConfig::Targets(targets) = options.latest_config
            && (targets.len() > MAX_REQUIRES || targets.iter().any(|t| t.len() > 191))
        {
            return Err(Error::Invalid(format!(
                "requires_latest_config takes at most {MAX_REQUIRES} targets of at most 191 bytes"
            )));
        }
        let requires = self.checked_requires(node_id, options.requires).await?;
        let (blob_digest, config_target) = match &action.kind {
            Some(Kind::ApplyConfig(apply)) => {
                (Some(apply.digest.as_str()), Some(apply.target.as_str()))
            }
            // Needs the blob, but is not a push: the latest push of the
            // target stays what it was.
            Some(Kind::ValidateConfig(validate)) => (Some(validate.digest.as_str()), None),
            _ => (None, None),
        };
        // Someone waits for a validation: it expires fast rather than run
        // long after the node reconnects.
        let default_deadline = if kind == kind::VALIDATE_CONFIG {
            self.config.validation_deadline
        } else {
            self.config.default_deadline
        };
        let created = self
            .store
            .insert_job(NewJob {
                id: &id,
                node_id,
                kind,
                action: &action.encode_to_vec(),
                ttl_ms: millis(options.deadline.unwrap_or(default_deadline)),
                blob_digest,
                accepted_data: &events::accepted().to_string(),
                may_exist: options.id.is_some(),
                requires: &requires,
                latest_config: options.latest_config,
                config_target,
            })
            .await?;
        if created {
            metrics::JOBS_SUBMITTED.add(kind, 1);
            debug!(job = %id, node = node_id, kind, "job queued");
            // The job is committed: delivery happens aside, so that a slow
            // agent never delays the submitter.
            self.trigger(node_id, false);
        }
        Ok((id, created))
    }

    /// The prerequisites without repetition, once each is known to be a job
    /// of the node.
    async fn checked_requires(
        &self,
        node_id: &str,
        requires: &[String],
    ) -> Result<Vec<String>, Error> {
        let mut unique: Vec<String> = Vec::with_capacity(requires.len());
        for id in requires {
            if !unique.contains(id) {
                unique.push(id.clone());
            }
        }
        if unique.len() > MAX_REQUIRES {
            return Err(Error::Invalid(format!(
                "more than {MAX_REQUIRES} prerequisites"
            )));
        }
        if unique.is_empty() {
            return Ok(unique);
        }
        let ids: Vec<&str> = unique.iter().map(String::as_str).collect();
        let heads = self.store.job_heads(&ids).await?;
        // One answer for a missing job and for a job of another node.
        match ids
            .iter()
            .find(|id| heads.get(**id).is_none_or(|head| head.node_id != node_id))
        {
            Some(id) => Err(Error::Invalid(format!(
                "prerequisite {id} is not a job of this node"
            ))),
            None => Ok(unique),
        }
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
        let waiting = self.hub.node_state(node_id);
        loop {
            if session.closed.is_cancelled() {
                return;
            }
            // One statement claims the whole batch, in order.
            let claim = match self
                .store
                .claim_jobs(node_id, &session.session_id, DRAIN_BATCH)
                .await
            {
                Ok(claim) => claim,
                Err(err) => {
                    warn!(node = node_id, error = %err, "claiming queued jobs failed");
                    return;
                }
            };
            for unmet in &claim.unmet {
                self.fail_unmet(node_id, unmet).await;
            }
            if claim.tried > 0
                && claim.jobs.is_empty()
                && !self.still_current(node_id, &session).await
            {
                return;
            }
            let (listed, blocked) = (claim.listed, claim.blocked);
            let mut jobs = claim.jobs.into_iter();
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
            if blocked {
                // Nothing goes before the job at the head of the queue. The
                // event commit asks for a pass once the mark is set; look
                // once more in case the prerequisite finished just before.
                if waiting.block() {
                    return;
                }
                continue;
            }
            waiting.unblock();
            // A short batch emptied the queue: later submits ask for a pass.
            if (listed as i64) < DRAIN_BATCH {
                return;
            }
        }
    }

    /// Ends as `failed`, without sending it, a queued job one of whose
    /// prerequisites did not succeed.
    async fn fail_unmet(&self, node_id: &str, unmet: &Unmet) {
        let failure = Termination {
            to: state::FAILED,
            seq: FAILED_SEQ,
            code: gorgone_core::legacy::ACTION_FINISH_KO,
            data: &events::prerequisite_failed(&unmet.prerequisite, &unmet.state).to_string(),
        };
        match self
            .store
            .terminate(&unmet.job_id, &[state::QUEUED], failure)
            .await
        {
            Ok(true) => {
                info!(
                    job = %unmet.job_id,
                    node = node_id,
                    prerequisite = %unmet.prerequisite,
                    state = %unmet.state,
                    "job failed: prerequisite did not succeed"
                );
                metrics::JOBS_PREREQUISITE_FAILED.inc(1);
                metrics::JOBS_FINISHED.add(state::FAILED, 1);
                self.hub.notify_job(&unmet.job_id, true);
            }
            // Cancelled or expired meanwhile: it is not sent either.
            Ok(false) => {}
            // Still queued: the next pass tries again.
            Err(err) => warn!(job = %unmet.job_id, error = %err, "job not failed"),
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
            metrics::JOBS_FINISHED.add(state::CANCELLED, 1);
            self.hub.notify_job(id, true);
            // Jobs requiring it are now to be failed.
            self.trigger(&job.node_id, false);
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
                    requires: job.requires.clone(),
                }),
            })),
        };
        if !self.hub.send(node_id, session, frame).await {
            return Ok(false);
        }
        metrics::JOBS_DISPATCHED.inc(1);
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
                        metrics::JOBS_FINISHED.add(state, 1);
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
