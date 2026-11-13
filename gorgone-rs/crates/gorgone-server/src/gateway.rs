//! Agent gateway: control sessions and blob transfer.

use std::collections::HashMap;
use std::sync::Arc;
use std::time::Duration;

use futures_util::{FutureExt, StreamExt};
use gorgone_core::legacy;
use gorgone_proto::NODE_ID_METADATA;
use gorgone_proto::agent::agent_frame::Frame as AgentFrameKind;
use gorgone_proto::agent::agent_gateway_server::AgentGateway;
use gorgone_proto::agent::server_frame::Frame;
use gorgone_proto::agent::{
    AgentFrame, BlobChunk, EventAck, FetchBlobRequest, JobEvent, ServerFrame, Welcome,
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
use crate::store::{
    FAILED_SEQ, NewEvent, SERVER_SEQ_FLOOR, Store, Termination, Transition, hash_token, state,
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
    async fn authenticate<T>(&self, request: &Request<T>) -> Result<String, Status> {
        let node_id = node_id_metadata(request)?;
        let token = bearer_token(request.metadata());
        let stored = self
            .store
            .node_token_hash(&node_id)
            .await
            .map_err(internal)?;
        match stored.as_deref() {
            Some(hash) if !hash.is_empty() => {
                let given = token.as_deref().map(hash_token).unwrap_or_default();
                if constant_time_eq(given.as_bytes(), hash.as_bytes()) {
                    Ok(node_id)
                } else {
                    Err(Status::unauthenticated("invalid node token"))
                }
            }
            _ if self.auto_enroll => {
                self.store.ensure_node(&node_id).await.map_err(internal)?;
                Ok(node_id)
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
        for (job_id, terminal) in touched {
            self.hub.notify_job(job_id, terminal);
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
        let verdicts = match self.record_events(node_id, &batch).await {
            Ok(verdicts) => verdicts,
            Err(err) => {
                warn!(node = node_id, events = batch.len(), error = %err, "event batch not committed, retrying one by one");
                let mut verdicts = Vec::with_capacity(batch.len());
                for event in &batch {
                    match self
                        .record_events(node_id, std::slice::from_ref(event))
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
    async fn heartbeat(&self, node_id: &str, handle: &SessionHandle) -> Result<(), &'static str> {
        match self.store.touch_session(node_id, &handle.session_id).await {
            Ok(true) => Ok(()),
            // The node reconnected elsewhere, or we were declared dead.
            Ok(false) => Err("session replaced"),
            Err(err) => {
                warn!(node = node_id, error = %err, "heartbeat not recorded");
                Ok(())
            }
        }
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
        self.dispatcher.trigger(&node_id, true);

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
                    AgentFrameKind::Heartbeat(_) => {
                        // Raced against the closing of the session, like every
                        // store call below: a slow store must not keep a
                        // replaced session alive.
                        let recorded = tokio::select! {
                            () = handle.closed.cancelled() => Err(CLOSED_BY_SERVER),
                            recorded = self.heartbeat(&node_id, &handle) => recorded,
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
        info!(node = %node_id, session = %handle.session_id, reason, "agent session ended");
        handle.closed.cancel();
        self.hub.remove(&node_id, &handle.session_id);
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
        let node_id = gateway.authenticate(&request).await?;
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
        drop(opening);

        let (tx, rx) = mpsc::channel(SESSION_BUFFER);
        let handle = SessionHandle::new(crate::random_id().map_err(internal)?, tx);
        let welcome = ServerFrame {
            frame: Some(Frame::Welcome(Welcome {
                session_id: handle.session_id.clone(),
                heartbeat_interval_s: u32::try_from(gateway.heartbeat_interval.as_secs())
                    .unwrap_or(u32::MAX),
                server_version: env!("CARGO_PKG_VERSION").to_owned(),
            })),
        };
        // Queued first so that it precedes any dispatch.
        handle
            .tx
            .send(Ok(welcome))
            .await
            .map_err(|_| Status::internal("session channel closed"))?;
        gateway
            .store
            .upsert_session(
                &node_id,
                &handle.session_id,
                &gateway.replica_id,
                &hello.agent_version,
            )
            .await
            .map_err(internal)?;
        gateway.hub.register(&node_id, handle.clone());
        info!(
            node = %node_id,
            session = %handle.session_id,
            agent_version = %hello.agent_version,
            "agent session opened"
        );
        let frames = ReceiverStream::new(rx).take_until(handle.closed.clone().cancelled_owned());
        tokio::spawn(Arc::clone(gateway).run_session(node_id, handle, inbound));
        Ok(Response::new(frames))
    }

    async fn fetch_blob(
        &self,
        request: Request<FetchBlobRequest>,
    ) -> Result<Response<Self::FetchBlobStream>, Status> {
        let gateway = &self.0;
        let node_id = gateway.authenticate(&request).await?;

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

    use super::*;
    use crate::dispatcher::DispatcherConfig;
    use crate::peer::PeerClients;
    use crate::store::testing::{self, unique};

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
                sweep_interval: Duration::from_secs(1),
                retention: Duration::from_secs(60),
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
            let (won, _) = store.claim_jobs(node, session, 10).await.unwrap();
            assert_eq!(won.len(), ids.len());
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
        let verdicts = gateway.record_events(&node, &batch).await.unwrap();
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
        let again = gateway.record_events(&node, &batch[..3]).await.unwrap();
        assert_eq!(again, [Verdict::Dropped("job already finished"); 3]);
        assert_eq!(store.events(&first, 0).await.unwrap().len(), 3);
        // A resend to a job still running is a stored duplicate.
        let resent = [event(&second, 1, output("resent"))];
        assert_eq!(
            gateway.record_events(&node, &resent).await.unwrap(),
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
        let verdicts = gateway.record_events(&node, &batch).await.unwrap();
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
}
