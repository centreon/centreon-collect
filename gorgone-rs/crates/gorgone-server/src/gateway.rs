//! Agent gateway: control sessions and blob transfer.

use std::collections::HashMap;
use std::sync::{Arc, PoisonError};
use std::time::Duration;

use futures_util::{FutureExt, StreamExt};
use gorgone_core::legacy;
use gorgone_proto::NODE_ID_METADATA;
use gorgone_proto::agent::agent_frame::Frame as AgentFrameKind;
use gorgone_proto::agent::agent_gateway_server::AgentGateway;
use gorgone_proto::agent::enroll_response::State as EnrollState;
use gorgone_proto::agent::server_frame::Frame;
use gorgone_proto::agent::{
    AgentFrame, BlobChunk, EnrollRequest, EnrollResponse, EventAck, FetchBlobRequest, Heartbeat,
    JobEvent, RotateCredentialRequest, RotateCredentialResponse, ServerFrame, Welcome,
};
use prost::Message;
use tokio::sync::{Semaphore, mpsc};
use tokio_stream::wrappers::ReceiverStream;
use tokio_util::sync::WaitForCancellationFutureOwned;
use tonic::{Request, Response, Status, Streaming};
use tracing::{debug, info, warn};

use crate::bearer_token;
use crate::dispatcher::Dispatcher;
use crate::error::Error;
use crate::events;
use crate::hub::{Hub, SessionHandle};
use crate::metrics;
use crate::rate_limit::RateLimiter;
use crate::store::{
    EnrolOutcome, EnrolRequest, FAILED_SEQ, MAX_CREDENTIAL_LEN, MIN_CREDENTIAL_LEN, NewEvent,
    NextCredential, SERVER_SEQ_FLOOR, Store, Termination, Transition, hash_token, state,
};

/// Time left to an agent to send its `Hello`.
const HELLO_TIMEOUT: Duration = Duration::from_secs(10);
/// Frames buffered towards one agent before a send has to wait.
const SESSION_BUFFER: usize = 256;
/// Sessions that may be between their opening and their `Hello` at once.
pub(crate) const MAX_OPENING_SESSIONS: usize = 256;
/// Events one job may produce; one more fails it.
pub(crate) const MAX_EVENTS_PER_JOB: i64 = 10_000;
/// Largest stored event payload (JSON), in bytes.
pub(crate) const MAX_EVENT_BYTES: usize = 1 << 20;
/// Events committed together, and their cumulated encoded size.
const MAX_BATCH_EVENTS: usize = 256;
const MAX_BATCH_BYTES: usize = 4 << 20;
/// Targets kept from one heartbeat, and bytes kept of each reported text.
const MAX_APPLIED_TARGETS: usize = 64;
const MAX_STATE_TEXT: usize = 191;

/// Why a session ended when the server closed it.
const CLOSED_BY_SERVER: &str = "closed by the server";

/// Frames towards an agent. The stream ends as soon as the session is
/// closed (replaced, kicked, shutdown), whoever still holds a sender and
/// whatever the store is doing: nothing is delivered after that point.
type SessionFrames = futures_util::stream::TakeUntil<
    ReceiverStream<Result<ServerFrame, Status>>,
    WaitForCancellationFutureOwned,
>;

/// Shared state of the gateway.
pub(crate) struct Gateway {
    pub(crate) store: Store,
    pub(crate) hub: Arc<Hub>,
    pub(crate) dispatcher: Arc<Dispatcher>,
    pub(crate) replica_id: String,
    pub(crate) heartbeat_interval: Duration,
    pub(crate) auto_enroll: bool,
    /// Bounds the sessions that have not sent their `Hello` yet.
    pub(crate) opening: Arc<Semaphore>,
    /// Refused `Enroll` attempts per source address; `None`: no limit.
    pub(crate) enrol_limit: Option<RateLimiter>,
}

/// An authenticated agent call.
#[derive(Debug)]
pub(crate) struct Caller {
    pub(crate) node_id: String,
    /// The central asks the agent to rotate its credential.
    pub(crate) rotation_requested: bool,
}

/// tonic service wrapper around [`Gateway`].
#[derive(Clone)]
pub(crate) struct GatewayService(pub(crate) Arc<Gateway>);

fn internal(err: Error) -> Status {
    warn!(error = %err, "gateway store failure");
    Status::internal("store failure")
}

/// Node id announced in the request metadata.
fn node_id_metadata<T>(request: &Request<T>) -> Result<String, Status> {
    request
        .metadata()
        .get(NODE_ID_METADATA)
        .and_then(|value| value.to_str().ok())
        .filter(|node_id| !node_id.is_empty() && node_id.len() <= 191)
        .map(str::to_owned)
        .ok_or_else(|| Status::unauthenticated("missing node id metadata"))
}

/// Comparison that does not stop at the first difference.
fn constant_time_eq(a: &[u8], b: &[u8]) -> bool {
    a.len() == b.len() && a.iter().zip(b).fold(0u8, |acc, (x, y)| acc | (x ^ y)) == 0
}

/// Checks a credential an agent chose. It is sent back as bearer metadata,
/// so it must be visible ASCII.
fn check_credential(credential: &str) -> Result<(), Status> {
    if (MIN_CREDENTIAL_LEN..=MAX_CREDENTIAL_LEN).contains(&credential.len())
        && credential.bytes().all(|b| b.is_ascii_graphic())
    {
        Ok(())
    } else {
        Err(Status::invalid_argument(format!(
            "credential must be {MIN_CREDENTIAL_LEN} to {MAX_CREDENTIAL_LEN} visible ASCII characters"
        )))
    }
}

/// Cuts a text reported by an agent to [`MAX_STATE_TEXT`] bytes.
fn bounded(text: &str) -> &str {
    let mut end = text.len().min(MAX_STATE_TEXT);
    while !text.is_char_boundary(end) {
        end -= 1;
    }
    &text[..end]
}

/// Stored form of what a heartbeat reports of its node:
/// `{"applied": {target: digest}, "engine": {"reachable", "version"} | null}`.
/// Keys are sorted, so that the same state always gives the same text.
pub(crate) fn node_state(heartbeat: &Heartbeat) -> String {
    let applied: serde_json::Map<String, serde_json::Value> = heartbeat
        .applied
        .iter()
        .filter(|applied| !applied.target.is_empty())
        .take(MAX_APPLIED_TARGETS)
        .map(|applied| {
            (
                bounded(&applied.target).to_owned(),
                bounded(&applied.digest).into(),
            )
        })
        .collect();
    let engine = heartbeat.engine.as_ref().map(|engine| {
        serde_json::json!({ "reachable": engine.reachable, "version": bounded(&engine.version) })
    });
    serde_json::json!({ "applied": applied, "engine": engine }).to_string()
}

/// What was done with an agent event. Both are acknowledged.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub(crate) enum Verdict {
    /// Stored, or already stored.
    Stored,
    /// Not acceptable: acknowledged so the agent stops resending, not stored.
    Dropped(&'static str),
}

impl Gateway {
    /// Checks, from the request metadata alone, that the caller is the agent
    /// of `node_id`. Nothing of the request body is read before this. A
    /// client certificate (mTLS) only proves the CA trusts the agent: the
    /// enrolment token is what identifies the node.
    ///
    /// During a credential rotation both the current credential and the one
    /// the agent proposed are accepted; the first call presenting the new
    /// one makes it the only one and drops the sessions opened with the
    /// former one.
    pub(crate) async fn authenticate<T>(&self, request: &Request<T>) -> Result<Caller, Status> {
        let node_id = node_id_metadata(request)?;
        let given = bearer_token(request.metadata())
            .as_deref()
            .map(hash_token)
            .unwrap_or_default();
        let stored = self
            .store
            .node_credentials(&node_id)
            .await
            .map_err(internal)?;
        match stored {
            Some(stored) if !stored.token_hash.is_empty() => {
                if constant_time_eq(given.as_bytes(), stored.token_hash.as_bytes()) {
                    return Ok(Caller {
                        node_id,
                        rotation_requested: stored.rotation_requested,
                    });
                }
                let proposed = stored
                    .next_token_hash
                    .as_deref()
                    .is_some_and(|next| constant_time_eq(given.as_bytes(), next.as_bytes()));
                if !proposed {
                    return Err(Status::unauthenticated("invalid node token"));
                }
                if self
                    .store
                    .promote_credential(&node_id, &given)
                    .await
                    .map_err(internal)?
                {
                    metrics::CREDENTIAL_ROTATIONS.inc(1);
                    info!(node = %node_id, "credential rotated: the former one is refused from now on");
                    // Sessions opened with the former credential end here;
                    // the other replicas see their rows gone.
                    self.hub.kick(&node_id);
                    return Ok(Caller {
                        node_id,
                        rotation_requested: false,
                    });
                }
                // Promoted concurrently by another call of the same agent.
                let current = self
                    .store
                    .node_credentials(&node_id)
                    .await
                    .map_err(internal)?;
                match current {
                    Some(current)
                        if constant_time_eq(given.as_bytes(), current.token_hash.as_bytes()) =>
                    {
                        Ok(Caller {
                            node_id,
                            rotation_requested: current.rotation_requested,
                        })
                    }
                    _ => Err(Status::unauthenticated("invalid node token")),
                }
            }
            // A node deleted after an enrolment keeps its credential refused.
            _ if self.auto_enroll
                && !self.store.node_revoked(&node_id).await.map_err(internal)? =>
            {
                self.store.ensure_node(&node_id).await.map_err(internal)?;
                Ok(Caller {
                    node_id,
                    rotation_requested: false,
                })
            }
            _ => Err(Status::unauthenticated("unknown node")),
        }
    }

    /// Commits a batch of agent events in ONE transaction (the inserts and
    /// the job state changes they imply), then notifies the waiters. Returns
    /// one verdict per event, in order; `Err` means nothing of the batch may
    /// be acknowledged.
    pub(crate) async fn record_events(
        &self,
        node_id: &str,
        component: &str,
        batch: &[JobEvent],
    ) -> Result<Vec<Verdict>, Error> {
        let mut ids: Vec<&str> = batch.iter().map(|event| event.job_id.as_str()).collect();
        ids.sort_unstable();
        ids.dedup();
        // One read for the whole batch; states are then tracked in memory,
        // event after event, as if they were committed one by one.
        let mut jobs = self.store.job_heads(&ids).await?;

        let mut verdicts = Vec::with_capacity(batch.len());
        let mut inserts: Vec<NewEvent<'_>> = Vec::new();
        let mut transitions: Vec<Transition<'_>> = Vec::new();
        let mut overflowing: Vec<&str> = Vec::new();
        // Jobs touched by the batch, and whether they are now terminal.
        let mut touched: HashMap<&str, bool> = HashMap::new();
        for event in batch {
            let verdict = 'verdict: {
                let Some(body) = &event.body else {
                    break 'verdict Verdict::Dropped("empty event");
                };
                // Seq 0 and the topmost values belong to the server.
                let seq = match i64::try_from(event.seq) {
                    Ok(seq) if seq > 0 && seq < SERVER_SEQ_FLOOR => seq,
                    _ => break 'verdict Verdict::Dropped("reserved sequence number"),
                };
                let Some(job) = jobs
                    .get_mut(&event.job_id)
                    .filter(|job| job.node_id == node_id)
                else {
                    break 'verdict Verdict::Dropped("not a job of this node");
                };
                // An agent posts events for the jobs of its own component only.
                if job.component != component {
                    break 'verdict Verdict::Dropped("not a job of this component");
                }
                if state::is_terminal(&job.state) {
                    break 'verdict Verdict::Dropped("job already finished");
                }
                // A queued job was handed to the agent only if it was requeued.
                if job.state == state::QUEUED && job.attempts == 0 {
                    break 'verdict Verdict::Dropped("job never dispatched");
                }
                if seq > MAX_EVENTS_PER_JOB {
                    state::FAILED.clone_into(&mut job.state);
                    overflowing.push(&event.job_id);
                    break 'verdict Verdict::Dropped("too many events");
                }

                let time_ms = if event.time_unix_ms > 0 {
                    event.time_unix_ms
                } else {
                    self.store.now_ms()
                };
                let mapped = events::map(body, time_ms);
                let mut data = mapped.data.to_string();
                if data.len() > MAX_EVENT_BYTES {
                    // Fields are truncated before; this only guards the escaping.
                    data = events::oversized(data.len()).to_string();
                }
                inserts.push(NewEvent {
                    job_id: &event.job_id,
                    seq,
                    time_ms,
                    code: i64::from(mapped.code),
                    data,
                });
                let to = mapped.final_state.unwrap_or(state::RUNNING);
                // `running` is written once; a job already running needs nothing.
                if job.state != to {
                    transitions.push(Transition {
                        job_id: &event.job_id,
                        to,
                    });
                    to.clone_into(&mut job.state);
                }
                touched.insert(&event.job_id, mapped.final_state.is_some());
                Verdict::Stored
            };
            verdicts.push(verdict);
        }

        if !inserts.is_empty() {
            let applied = self.store.commit_events(&inserts, &transitions).await?;
            metrics::JOB_EVENTS.inc(inserts.len() as u64);
            metrics::EVENT_BATCH_SIZE.record(inserts.len() as u64);
            for (to, changed) in applied {
                if to != state::RUNNING && changed > 0 {
                    metrics::JOBS_FINISHED.add(to, changed);
                }
            }
        }
        for job_id in overflowing {
            let failure = Termination {
                to: state::FAILED,
                seq: FAILED_SEQ,
                code: legacy::ACTION_FINISH_KO,
                data: &events::too_many_events(MAX_EVENTS_PER_JOB).to_string(),
            };
            if self
                .store
                .terminate(job_id, &state::ACTIVE, failure)
                .await?
            {
                warn!(job = job_id, node = node_id, "job failed: too many events");
                metrics::JOBS_FINISHED.add(state::FAILED, 1);
            }
            touched.insert(job_id, true);
        }
        // Right after the commit: local waiters do not wait for any poll.
        let mut finished = false;
        for (job_id, terminal) in touched {
            self.hub.notify_job(job_id, terminal);
            finished |= terminal;
        }
        // A job of the node ended: what waited for it may go, or must fail,
        // on any component whose drain is here. A component held by another
        // replica sees it at its next sweep.
        if finished {
            for blocked in self.hub.blocked(node_id) {
                self.dispatcher.trigger_component(node_id, &blocked, false);
            }
        }
        Ok(verdicts)
    }

    /// Commits a batch and acknowledges its events in order. When the batch
    /// fails as a whole, its events are committed one by one, so that a bad
    /// one cannot keep the others from being stored. `Err` carries the
    /// reason to close the session; what was committed is acknowledged first.
    async fn commit_and_ack(
        &self,
        node_id: &str,
        handle: &SessionHandle,
        batch: Vec<JobEvent>,
    ) -> Result<(), &'static str> {
        let verdicts = match self.record_events(node_id, &handle.component, &batch).await {
            Ok(verdicts) => verdicts,
            Err(err) => {
                warn!(node = node_id, events = batch.len(), error = %err, "event batch not committed, retrying one by one");
                let mut verdicts = Vec::with_capacity(batch.len());
                for event in &batch {
                    match self
                        .record_events(node_id, &handle.component, std::slice::from_ref(event))
                        .await
                    {
                        Ok(single) => verdicts.extend(single),
                        Err(err) => {
                            // Not acknowledged: closing makes the agent resend it.
                            warn!(node = node_id, job = %event.job_id, seq = event.seq, error = %err, "event not committed");
                            break;
                        }
                    }
                }
                verdicts
            }
        };
        let committed = verdicts.len();
        for (event, verdict) in batch.iter().zip(verdicts) {
            if let Verdict::Dropped(reason) = verdict {
                warn!(node = node_id, job = %event.job_id, seq = event.seq, reason, "agent event dropped");
                metrics::JOB_EVENTS_DROPPED.inc(1);
            }
            let ack = ServerFrame {
                frame: Some(Frame::Ack(EventAck {
                    job_id: event.job_id.clone(),
                    seq: event.seq,
                })),
            };
            if !self.hub.send(node_id, handle, ack).await {
                return Err("agent not reading");
            }
        }
        if committed < batch.len() {
            return Err("store failure");
        }
        Ok(())
    }

    /// Records a heartbeat. `Err` carries the reason to close the session.
    async fn heartbeat(
        &self,
        node_id: &str,
        handle: &SessionHandle,
        heartbeat: &Heartbeat,
    ) -> Result<(), &'static str> {
        match self.store.touch_session(node_id, &handle.session_id).await {
            Ok(true) => {}
            // The node reconnected elsewhere, or we were declared dead.
            Ok(false) => return Err("session replaced"),
            Err(err) => {
                warn!(node = node_id, error = %err, "heartbeat not recorded");
                return Ok(());
            }
        }
        // Written on change only: most heartbeats repeat the previous one.
        let state = node_state(heartbeat);
        let reported = || {
            handle
                .reported
                .lock()
                .unwrap_or_else(PoisonError::into_inner)
        };
        if reported().as_deref() == Some(state.as_str()) {
            return Ok(());
        }
        // The node keeps the state of its engine component, as before
        // components; each component keeps its own.
        let recorded = async {
            if handle.component == gorgone_core::DEFAULT_COMPONENT {
                self.store.set_node_state(node_id, &state).await?;
            }
            self.store
                .set_component_state(node_id, &handle.component, &state)
                .await
        };
        match recorded.await {
            Ok(()) => *reported() = Some(state),
            // Not remembered: the next heartbeat writes it.
            Err(err) => warn!(node = node_id, error = %err, "node state not recorded"),
        }
        Ok(())
    }

    /// Reads the session until it ends, then unregisters it.
    async fn run_session(
        self: Arc<Self>,
        node_id: String,
        handle: SessionHandle,
        mut inbound: Streaming<AgentFrame>,
    ) {
        // Deliver what was queued while the node was away, without blocking
        // the read loop.
        self.dispatcher
            .trigger_component(&node_id, &handle.component, true);

        let idle = self.heartbeat_interval * 3;
        let reason = 'session: loop {
            let received = tokio::select! {
                () = handle.closed.cancelled() => break CLOSED_BY_SERVER,
                received = tokio::time::timeout(idle, inbound.message()) => received,
            };
            let mut frame = match received {
                Err(_) => break "heartbeat timeout",
                Ok(Err(_)) => break "stream error",
                Ok(Ok(None)) => break "closed by the agent",
                Ok(Ok(Some(frame))) => frame.frame,
            };
            // Handles the frame, and with an event every frame that already
            // arrived behind it: under load they are committed together,
            // without ever waiting for more.
            let mut end = None;
            while let Some(current) = frame.take() {
                let event = match current {
                    AgentFrameKind::Event(event) => event,
                    AgentFrameKind::Heartbeat(heartbeat) => {
                        // Raced against the closing of the session, like every
                        // store call below: a slow store must not keep a
                        // replaced session alive.
                        let recorded = tokio::select! {
                            () = handle.closed.cancelled() => Err(CLOSED_BY_SERVER),
                            recorded = self.heartbeat(&node_id, &handle, &heartbeat) => recorded,
                        };
                        if let Err(reason) = recorded {
                            break 'session reason;
                        }
                        continue;
                    }
                    AgentFrameKind::Hello(_) => continue,
                };
                let mut bytes = event.encoded_len();
                let mut batch = vec![event];
                while batch.len() < MAX_BATCH_EVENTS && bytes < MAX_BATCH_BYTES {
                    match inbound.message().now_or_never() {
                        Some(Ok(Some(AgentFrame {
                            frame: Some(AgentFrameKind::Event(event)),
                        }))) => {
                            bytes += event.encoded_len();
                            batch.push(event);
                        }
                        // Another kind of frame: handled after the batch, in order.
                        Some(Ok(Some(other))) => {
                            frame = other.frame;
                            break;
                        }
                        Some(Ok(None)) => {
                            end = Some("closed by the agent");
                            break;
                        }
                        Some(Err(_)) => {
                            end = Some("stream error");
                            break;
                        }
                        None => break,
                    }
                }
                // Dropping the commit mid-way is safe: what is not
                // acknowledged is sent again, and events are idempotent.
                let committed = tokio::select! {
                    () = handle.closed.cancelled() => Err(CLOSED_BY_SERVER),
                    committed = self.commit_and_ack(&node_id, &handle, batch) => committed,
                };
                if let Err(reason) = committed {
                    break 'session reason;
                }
            }
            if let Some(reason) = end {
                break reason;
            }
        };
        info!(node = %node_id, component = %handle.component, session = %handle.session_id, reason, "agent session ended");
        handle.closed.cancel();
        self.hub
            .remove(&node_id, &handle.component, &handle.session_id);
        if let Err(err) = self
            .store
            .remove_session(&node_id, &handle.session_id)
            .await
        {
            warn!(node = %node_id, error = %err, "session not removed from the store");
        }
    }
}

#[tonic::async_trait]
impl AgentGateway for GatewayService {
    type SessionStream = SessionFrames;
    type FetchBlobStream = ReceiverStream<Result<BlobChunk, Status>>;

    async fn session(
        &self,
        request: Request<Streaming<AgentFrame>>,
    ) -> Result<Response<Self::SessionStream>, Status> {
        let gateway = &self.0;
        // Bounded: streams that are not established yet cost a store lookup
        // and a pending read.
        let opening = Arc::clone(&gateway.opening)
            .try_acquire_owned()
            .map_err(|_| Status::resource_exhausted("too many sessions being opened"))?;
        // Before the first frame is decoded.
        let Caller {
            node_id,
            rotation_requested,
        } = gateway.authenticate(&request).await?;
        let mut inbound = request.into_inner();

        let first = tokio::time::timeout(HELLO_TIMEOUT, inbound.message())
            .await
            .map_err(|_| Status::deadline_exceeded("no Hello received"))??;
        let Some(AgentFrame {
            frame: Some(AgentFrameKind::Hello(hello)),
        }) = first
        else {
            return Err(Status::invalid_argument("first frame must be Hello"));
        };
        if hello.node_id != node_id {
            return Err(Status::permission_denied(
                "Hello node id differs from the authenticated node id",
            ));
        }
        // An agent from before components drives the engine.
        let component = if hello.component.is_empty() {
            gorgone_core::DEFAULT_COMPONENT.to_owned()
        } else {
            hello.component.clone()
        };
        if !gorgone_core::valid_component(&component) {
            return Err(Status::invalid_argument(
                "component must be 1 to 32 characters among a-z, 0-9, _ and -",
            ));
        }
        // Stored as one JSON text: bounded like the targets of a heartbeat.
        let mut targets: Vec<String> = hello
            .targets
            .iter()
            .filter(|target| !target.is_empty())
            .take(MAX_APPLIED_TARGETS)
            .map(|target| bounded(target).to_owned())
            .collect();
        targets.sort_unstable();
        targets.dedup();
        drop(opening);

        let (tx, rx) = mpsc::channel(SESSION_BUFFER);
        let handle = SessionHandle::new(crate::random_id().map_err(internal)?, component, tx);
        let welcome = ServerFrame {
            frame: Some(Frame::Welcome(Welcome {
                session_id: handle.session_id.clone(),
                heartbeat_interval_s: u32::try_from(gateway.heartbeat_interval.as_secs())
                    .unwrap_or(u32::MAX),
                server_version: env!("CARGO_PKG_VERSION").to_owned(),
                rotate_credential: rotation_requested,
            })),
        };
        // Queued first so that it precedes any dispatch.
        handle
            .tx
            .send(Ok(welcome))
            .await
            .map_err(|_| Status::internal("session channel closed"))?;
        // The component first: jobs submitted once the session is visible
        // are routed knowing it.
        gateway
            .store
            .upsert_component(&node_id, &handle.component, &targets)
            .await
            .map_err(internal)?;
        gateway
            .store
            .upsert_session(
                &node_id,
                &handle.component,
                &handle.session_id,
                &gateway.replica_id,
                &hello.agent_version,
            )
            .await
            .map_err(internal)?;
        gateway.hub.register(&node_id, handle.clone());
        info!(
            node = %node_id,
            component = %handle.component,
            ?targets,
            session = %handle.session_id,
            agent_version = %hello.agent_version,
            rotation_requested,
            "agent session opened"
        );
        let frames = ReceiverStream::new(rx).take_until(handle.closed.clone().cancelled_owned());
        tokio::spawn(Arc::clone(gateway).run_session(node_id, handle, inbound));
        Ok(Response::new(frames))
    }

    async fn enroll(
        &self,
        request: Request<EnrollRequest>,
    ) -> Result<Response<EnrollResponse>, Status> {
        let gateway = &self.0;
        // Bounded like the opening of sessions: each attempt reads the store.
        let _opening = Arc::clone(&gateway.opening)
            .try_acquire_owned()
            .map_err(|_| Status::resource_exhausted("too many sessions being opened"))?;
        let source = request.remote_addr().map(|addr| addr.ip());
        let limit = gateway.enrol_limit.as_ref().zip(source);
        if let Some((limit, source)) = limit
            && !limit.allows(source)
        {
            metrics::ENROLMENTS.add("rate_limited", 1);
            debug!(%source, "enrolment attempt refused: too many refused attempts");
            return Err(Status::resource_exhausted(format!(
                "too many refused enrolment attempts from this address: retry in {:?}",
                limit.retry_after()
            )));
        }
        let refused = |status: Status| {
            if let Some((limit, source)) = limit {
                limit.charge(source);
            }
            status
        };
        let enroll = request.into_inner();
        let credential = enroll.credential.as_str();
        check_credential(credential).map_err(refused)?;
        if enroll.token.is_empty() {
            return Err(refused(Status::unauthenticated("enrolment token missing")));
        }
        let component = if enroll.component.is_empty() {
            gorgone_core::DEFAULT_COMPONENT
        } else {
            enroll.component.as_str()
        };
        if !gorgone_core::valid_component(component) {
            return Err(refused(Status::invalid_argument(
                "component must be 1 to 32 characters among a-z, 0-9, _ and -",
            )));
        }
        let outcome = gateway
            .store
            .enrol(EnrolRequest {
                token: &enroll.token,
                credential,
                name: &enroll.name,
                address: &enroll.address,
                agent_version: &enroll.agent_version,
                component,
            })
            .await
            .map_err(internal)?;
        metrics::ENROLMENTS.add(
            match &outcome {
                EnrolOutcome::Enrolled { .. } => "enrolled",
                EnrolOutcome::Pending { .. } => "pending",
                EnrolOutcome::Rejected { .. } => "rejected",
                EnrolOutcome::Refused(_) => "refused",
            },
            1,
        );
        match outcome {
            EnrolOutcome::Enrolled {
                enrolment_id,
                node_id,
            } => {
                info!(enrolment = %enrolment_id, node = %node_id, name = %bounded(&enroll.name), "node enrolled");
                Ok(Response::new(EnrollResponse {
                    state: EnrollState::Enrolled.into(),
                    node_id,
                    enrolment_id,
                }))
            }
            EnrolOutcome::Pending { enrolment_id } => {
                debug!(enrolment = %enrolment_id, name = %bounded(&enroll.name), "enrolment waits for a node");
                Ok(Response::new(EnrollResponse {
                    state: EnrollState::Pending.into(),
                    node_id: String::new(),
                    enrolment_id,
                }))
            }
            EnrolOutcome::Rejected { enrolment_id } => {
                debug!(enrolment = %enrolment_id, name = %bounded(&enroll.name), "enrolment rejected");
                Ok(Response::new(EnrollResponse {
                    state: EnrollState::Rejected.into(),
                    node_id: String::new(),
                    enrolment_id,
                }))
            }
            EnrolOutcome::Refused(reason) => {
                // The reason stays here: the agent learns nothing about the token.
                warn!(reason, name = %bounded(&enroll.name), address = %bounded(&enroll.address), "enrolment refused");
                Err(refused(Status::permission_denied(
                    "enrolment refused: token unknown, expired, used up, or enrolment revoked",
                )))
            }
        }
    }

    async fn rotate_credential(
        &self,
        request: Request<RotateCredentialRequest>,
    ) -> Result<Response<RotateCredentialResponse>, Status> {
        let gateway = &self.0;
        let _opening = Arc::clone(&gateway.opening)
            .try_acquire_owned()
            .map_err(|_| Status::resource_exhausted("too many sessions being opened"))?;
        // Promotes the proposed credential when the call presents it.
        let node_id = gateway.authenticate(&request).await?.node_id;
        let new_credential = request.into_inner().new_credential;
        check_credential(&new_credential)?;
        match gateway
            .store
            .propose_credential(&node_id, &hash_token(&new_credential))
            .await
            .map_err(internal)?
        {
            NextCredential::Recorded => {
                info!(node = %node_id, "new credential proposed: both are accepted until it is used");
                Ok(Response::new(RotateCredentialResponse { done: false }))
            }
            NextCredential::Current => Ok(Response::new(RotateCredentialResponse { done: true })),
            NextCredential::NoCredential => Err(Status::failed_precondition(
                "this node has no credential to rotate",
            )),
            NextCredential::UnknownNode => Err(Status::unauthenticated("unknown node")),
        }
    }

    async fn fetch_blob(
        &self,
        request: Request<FetchBlobRequest>,
    ) -> Result<Response<Self::FetchBlobStream>, Status> {
        let gateway = &self.0;
        let node_id = gateway.authenticate(&request).await?.node_id;

        let digest = request.into_inner().digest.to_ascii_lowercase();
        if digest.len() != 64 || !digest.bytes().all(|b| b.is_ascii_hexdigit()) {
            return Err(Status::invalid_argument("digest must be a hex SHA-256"));
        }
        // Only a bundle one of the node's own pending jobs refers to; the
        // same answer hides whether another node's blob exists.
        let needed = gateway
            .store
            .blob_needed(Some(&node_id), &digest)
            .await
            .map_err(internal)?;
        let stored = gateway.store.blob_size(&digest).await.map_err(internal)?;
        if !needed || stored.is_none() {
            return Err(Status::not_found("unknown blob"));
        }
        debug!(node = %node_id, digest = %digest, "blob requested");

        let (tx, rx) = mpsc::channel(2);
        let store = gateway.store.clone();
        tokio::spawn(async move {
            let mut idx = 0;
            loop {
                let item = match store.blob_chunk(&digest, idx).await {
                    Ok(Some(data)) => Ok(BlobChunk { data }),
                    Ok(None) => return,
                    Err(err) => Err(internal(err)),
                };
                let failed = item.is_err();
                if tx.send(item).await.is_err() || failed {
                    return;
                }
                idx += 1;
            }
        });
        Ok(Response::new(ReceiverStream::new(rx)))
    }
}

#[cfg(test)]
mod tests {
    use gorgone_proto::agent::job_event::Body;
    use gorgone_proto::agent::job_finished::Outcome;
    use gorgone_proto::agent::{CommandResult, JobFinished, JobStarted};

    use gorgone_proto::agent::agent_gateway_server::AgentGateway as _;

    use super::*;
    use crate::dispatcher::DispatcherConfig;
    use crate::peer::PeerClients;
    use crate::store::testing::{self, ENGINE, unique};

    async fn gateway() -> (Arc<Gateway>, tempfile::TempDir) {
        let (store, dir) = testing::store().await;
        let hub = Arc::new(Hub::new(Duration::from_secs(1)));
        let dispatcher = Arc::new(Dispatcher::new(
            store.clone(),
            Arc::clone(&hub),
            PeerClients::default(),
            DispatcherConfig {
                replica_id: "test-replica".into(),
                peer_addr: "http://127.0.0.1:1".into(),
                default_deadline: Duration::from_secs(60),
                validation_deadline: Duration::from_secs(120),
                sweep_interval: Duration::from_secs(1),
                retention: Duration::from_secs(60),
                enrolment_token_retention: Duration::from_secs(60),
                auto_enroll: true,
            },
        ));
        let gateway = Gateway {
            store,
            hub,
            dispatcher,
            replica_id: "test-replica".into(),
            heartbeat_interval: Duration::from_secs(5),
            auto_enroll: true,
            opening: Arc::new(Semaphore::new(MAX_OPENING_SESSIONS)),
            enrol_limit: None,
        };
        (Arc::new(gateway), dir)
    }

    fn event(job_id: &str, seq: u64, body: Body) -> JobEvent {
        JobEvent {
            job_id: job_id.to_owned(),
            seq,
            time_unix_ms: 1_000,
            body: Some(body),
        }
    }

    fn output(stdout: &str) -> Body {
        Body::CommandResult(CommandResult {
            stdout: stdout.into(),
            ..CommandResult::default()
        })
    }

    fn finished(outcome: Outcome) -> Body {
        Body::Finished(JobFinished {
            outcome: outcome.into(),
            message: String::new(),
        })
    }

    /// Two jobs dispatched to `node`, and one dispatched to another node.
    async fn dispatched(gateway: &Gateway) -> (String, [String; 2], String) {
        let store = &gateway.store;
        let (node, other) = (unique("node"), unique("node"));
        let jobs = [unique("job"), unique("job")];
        let foreign = unique("job");
        for (node, session, ids) in [
            (&node, "s1", &jobs[..]),
            (&other, "s2", std::slice::from_ref(&foreign)),
        ] {
            store.open_session(node, session).await;
            for id in ids {
                store.queue(id, node, 60_000).await;
            }
            let claim = store.claim_jobs(node, ENGINE, session, 10).await.unwrap();
            assert_eq!(claim.jobs.len(), ids.len());
        }
        (node, jobs, foreign)
    }

    #[tokio::test]
    async fn batch_is_committed_as_sequential_events_would_be() {
        let (gateway, _dir) = gateway().await;
        let (node, [first, second], foreign) = dispatched(&gateway).await;
        let mut updates = gateway.hub.subscribe();

        let batch = [
            event(&first, 1, Body::Started(JobStarted {})),
            event(&first, 2, output("one")),
            // A duplicate inside the batch: acknowledged, first one kept.
            event(&first, 2, output("duplicate")),
            // An event for a job of another node: dropped, the rest goes on.
            event(&foreign, 1, finished(Outcome::Ok)),
            event(&second, 1, output("other job")),
            event(&first, 3, finished(Outcome::Ok)),
            // After the terminal event of the same batch.
            event(&first, 4, output("late")),
            event("no-such-job", 1, output("x")),
            event(&second, 0, output("reserved")),
        ];
        let verdicts = gateway.record_events(&node, ENGINE, &batch).await.unwrap();
        assert_eq!(
            verdicts,
            [
                Verdict::Stored,
                Verdict::Stored,
                Verdict::Stored,
                Verdict::Dropped("not a job of this node"),
                Verdict::Stored,
                Verdict::Stored,
                Verdict::Dropped("job already finished"),
                Verdict::Dropped("not a job of this node"),
                Verdict::Dropped("reserved sequence number"),
            ]
        );

        let store = &gateway.store;
        let stored = store.events(&first, 0).await.unwrap();
        assert_eq!(
            stored.iter().map(|e| (e.seq, e.code)).collect::<Vec<_>>(),
            [(1, 4), (2, 100), (3, 2)]
        );
        assert!(stored[1].data.contains("\"one\""));
        assert_eq!(
            store.job(&first).await.unwrap().unwrap().state,
            state::SUCCEEDED
        );
        assert_eq!(
            store.job(&second).await.unwrap().unwrap().state,
            state::RUNNING
        );
        // The foreign job is untouched.
        assert!(store.events(&foreign, 0).await.unwrap().is_empty());
        assert_eq!(
            store.job(&foreign).await.unwrap().unwrap().state,
            state::DISPATCHED
        );

        // Waiters are told once per job, with the terminal flag.
        let mut told = HashMap::new();
        while let Ok(update) = updates.try_recv() {
            told.insert(update.job_id.to_string(), update.terminal);
        }
        assert_eq!(told.len(), 2);
        assert_eq!((told[&first], told[&second]), (true, false));

        // The same batch resent after a reconnection changes nothing.
        let again = gateway
            .record_events(&node, ENGINE, &batch[..3])
            .await
            .unwrap();
        assert_eq!(again, [Verdict::Dropped("job already finished"); 3]);
        assert_eq!(store.events(&first, 0).await.unwrap().len(), 3);
        // A resend to a job still running is a stored duplicate.
        let resent = [event(&second, 1, output("resent"))];
        assert_eq!(
            gateway.record_events(&node, ENGINE, &resent).await.unwrap(),
            [Verdict::Stored]
        );
        assert!(
            store.events(&second, 0).await.unwrap()[0]
                .data
                .contains("other job")
        );
    }

    #[tokio::test]
    async fn event_cap_fails_the_job_without_blocking_the_batch() {
        let (gateway, _dir) = gateway().await;
        let (node, [first, second], _) = dispatched(&gateway).await;
        let over = u64::try_from(MAX_EVENTS_PER_JOB).unwrap() + 1;
        let batch = [
            event(&first, over, output("too many")),
            event(&first, 1, output("after the cap")),
            event(&second, 1, finished(Outcome::Failed)),
        ];
        let verdicts = gateway.record_events(&node, ENGINE, &batch).await.unwrap();
        assert_eq!(
            verdicts,
            [
                Verdict::Dropped("too many events"),
                Verdict::Dropped("job already finished"),
                Verdict::Stored,
            ]
        );
        let store = &gateway.store;
        assert_eq!(
            store.job(&first).await.unwrap().unwrap().state,
            state::FAILED
        );
        let stored = store.events(&first, 0).await.unwrap();
        assert_eq!(stored.len(), 1);
        assert_eq!(stored[0].seq, FAILED_SEQ);
        assert_eq!(
            store.job(&second).await.unwrap().unwrap().state,
            state::FAILED
        );
    }

    /// A request of the agent of `node` presenting `credential`.
    fn signed<T>(body: T, node: &str, credential: &str) -> Request<T> {
        let mut request = Request::new(body);
        let metadata = request.metadata_mut();
        metadata.insert(NODE_ID_METADATA, node.parse().unwrap());
        metadata.insert(
            "authorization",
            format!("Bearer {credential}").parse().unwrap(),
        );
        request
    }

    async fn rotate(
        service: &GatewayService,
        node: &str,
        presented: &str,
        new: &str,
    ) -> Result<bool, tonic::Code> {
        let request = RotateCredentialRequest {
            new_credential: new.to_owned(),
        };
        service
            .rotate_credential(signed(request, node, presented))
            .await
            .map(|answer| answer.into_inner().done)
            .map_err(|status| status.code())
    }

    /// Every point where an agent may stop during a rotation leaves it a
    /// credential the server accepts, and a repeated step changes nothing.
    #[tokio::test]
    async fn rotation_crash_windows_always_leave_a_working_credential() {
        let (gateway, _dir) = gateway().await;
        let service = GatewayService(Arc::clone(&gateway));
        let node = unique("node");
        let old = gateway.store.enrol_node(&node, "rotated").await.unwrap();
        let new = "a-new-credential-chosen-by-the-agent-0001";
        let auth = |credential: &str| {
            let request = signed((), &node, credential);
            let gateway = Arc::clone(&gateway);
            async move { gateway.authenticate(&request).await }
        };
        assert!(auth(&old).await.is_ok());
        // Not proposed yet (agent stopped after writing it): refused, the
        // former one works.
        assert_eq!(
            auth(new).await.unwrap_err().code(),
            tonic::Code::Unauthenticated
        );
        assert!(auth(&old).await.is_ok());
        // Malformed: refused before the store is touched.
        assert_eq!(
            rotate(&service, &node, &old, "short").await,
            Err(tonic::Code::InvalidArgument)
        );

        // Proposed (the answer may be lost): both work, repeating is harmless.
        assert_eq!(rotate(&service, &node, &old, new).await, Ok(false));
        assert_eq!(rotate(&service, &node, &old, new).await, Ok(false));
        assert!(auth(&old).await.is_ok());

        // The agent presents the new one: it becomes the only one.
        assert_eq!(rotate(&service, &node, new, new).await, Ok(true));
        assert_eq!(
            auth(&old).await.unwrap_err().code(),
            tonic::Code::Unauthenticated
        );
        assert_eq!(
            rotate(&service, &node, &old, new).await,
            Err(tonic::Code::Unauthenticated)
        );
        // Restarted before it updated its file: it repeats both steps, the
        // first one is refused, the second one confirms.
        assert_eq!(rotate(&service, &node, new, new).await, Ok(true));
        assert!(auth(new).await.is_ok());

        // A session opened with the proposed credential promotes it too.
        let newer = "a-newer-credential-chosen-by-the-agent-02";
        assert_eq!(rotate(&service, &node, new, newer).await, Ok(false));
        let caller = auth(newer).await.unwrap();
        assert!(!caller.rotation_requested);
        assert_eq!(
            auth(new).await.unwrap_err().code(),
            tonic::Code::Unauthenticated
        );

        // The request of the central travels with the next authentication.
        gateway.store.request_rotation(&node).await.unwrap();
        assert!(auth(newer).await.unwrap().rotation_requested);
    }

    #[tokio::test]
    async fn refused_enrolments_are_rate_limited_per_address() {
        let (gateway, _dir) = gateway().await;
        let mut gateway = Arc::into_inner(gateway).unwrap();
        gateway.enrol_limit = RateLimiter::new(60, 2);
        let service = GatewayService(Arc::new(gateway));
        let attempt = |token: &str| {
            let mut request = Request::new(EnrollRequest {
                token: token.to_owned(),
                credential: "credential-of-an-agent-of-32-characters".to_owned(),
                ..EnrollRequest::default()
            });
            request
                .extensions_mut()
                .insert(tonic::transport::server::TcpConnectInfo {
                    local_addr: None,
                    remote_addr: Some("192.0.2.7:4000".parse().unwrap()),
                });
            let service = service.clone();
            async move {
                service
                    .enroll(request)
                    .await
                    .map(|_| ())
                    .map_err(|s| s.code())
            }
        };
        assert_eq!(attempt("unknown").await, Err(tonic::Code::PermissionDenied));
        assert_eq!(attempt("unknown").await, Err(tonic::Code::PermissionDenied));
        // Out of budget: refused before the store is read.
        assert_eq!(
            attempt("unknown").await,
            Err(tonic::Code::ResourceExhausted)
        );
    }
}
