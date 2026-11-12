//! Agent gateway: control sessions and blob transfer.

use std::sync::Arc;
use std::time::Duration;

use gorgone_proto::NODE_ID_METADATA;
use gorgone_proto::agent::agent_frame::Frame as AgentFrameKind;
use gorgone_proto::agent::agent_gateway_server::AgentGateway;
use gorgone_proto::agent::server_frame::Frame;
use gorgone_proto::agent::{
    AgentFrame, BlobChunk, EventAck, FetchBlobRequest, JobEvent, ServerFrame, Welcome,
};
use tokio::sync::mpsc;
use tokio_stream::wrappers::ReceiverStream;
use tokio_util::sync::CancellationToken;
use tonic::{Request, Response, Status, Streaming};
use tracing::{debug, info, warn};
use uuid::Uuid;

use crate::bearer_token;
use crate::dispatcher::Dispatcher;
use crate::error::Error;
use crate::events;
use crate::hub::{Hub, SessionHandle};
use crate::store::{CANCELLED_SEQ, Store, hash_token, now_ms};

/// Time left to an agent to send its `Hello`.
const HELLO_TIMEOUT: Duration = Duration::from_secs(10);
/// Frames buffered towards one agent before the dispatcher waits.
const SESSION_BUFFER: usize = 256;

/// Shared state of the gateway.
pub(crate) struct Gateway {
    pub(crate) store: Store,
    pub(crate) hub: Arc<Hub>,
    pub(crate) dispatcher: Arc<Dispatcher>,
    pub(crate) replica_id: String,
    pub(crate) heartbeat_interval: Duration,
    pub(crate) auto_enroll: bool,
}

/// tonic service wrapper around [`Gateway`].
#[derive(Clone)]
pub(crate) struct GatewayService(pub(crate) Arc<Gateway>);

fn internal(err: Error) -> Status {
    warn!(error = %err, "gateway store failure");
    Status::internal("store failure")
}

/// Common name of the client certificate: `None` without mTLS,
/// `Some(None)` when the certificate has no readable CN.
fn peer_common_name<T>(request: &Request<T>) -> Option<Option<String>> {
    let certs = request.peer_certs()?;
    let leaf = certs.first()?;
    let name = x509_parser::parse_x509_certificate(leaf.as_ref())
        .ok()
        .and_then(|(_, cert)| {
            cert.subject()
                .iter_common_name()
                .next()
                .and_then(|cn| cn.as_str().ok())
                .map(str::to_owned)
        });
    Some(name)
}

/// Comparison that does not stop at the first difference.
fn constant_time_eq(a: &[u8], b: &[u8]) -> bool {
    a.len() == b.len() && a.iter().zip(b).fold(0u8, |acc, (x, y)| acc | (x ^ y)) == 0
}

impl Gateway {
    /// Checks that the caller is the agent of `node_id`.
    async fn authenticate(
        &self,
        node_id: &str,
        token: Option<&str>,
        cert_cn: Option<Option<String>>,
    ) -> Result<(), Status> {
        if node_id.is_empty() || node_id.len() > 191 {
            return Err(Status::invalid_argument("invalid node id"));
        }
        // With mTLS a certificate issued for this node is sufficient: the
        // token becomes optional.
        if let Some(cn) = cert_cn {
            if cn.as_deref() != Some(node_id) {
                return Err(Status::permission_denied(
                    "certificate common name does not match the node id",
                ));
            }
            return self.store.ensure_node(node_id).await.map_err(internal);
        }
        let stored = self
            .store
            .node_token_hash(node_id)
            .await
            .map_err(internal)?;
        match stored.as_deref() {
            Some(hash) if !hash.is_empty() => {
                let given = token.map(hash_token).unwrap_or_default();
                if constant_time_eq(given.as_bytes(), hash.as_bytes()) {
                    Ok(())
                } else {
                    Err(Status::unauthenticated("invalid node token"))
                }
            }
            _ if self.auto_enroll => self.store.ensure_node(node_id).await.map_err(internal),
            _ => Err(Status::unauthenticated("unknown node")),
        }
    }

    /// Commits one agent event. `Ok` means it can be acknowledged.
    async fn record_event(&self, node_id: &str, event: &JobEvent) -> Result<(), Error> {
        let Some(body) = &event.body else {
            return Ok(());
        };
        // Seq 0 and the topmost values belong to the server.
        let seq = match i64::try_from(event.seq) {
            Ok(seq) if seq > 0 && seq < CANCELLED_SEQ => seq,
            _ => {
                warn!(job = %event.job_id, seq = event.seq, "event with reserved seq dropped");
                return Ok(());
            }
        };
        match self.store.job(&event.job_id).await? {
            Some(job) if job.node_id == node_id => {}
            // Purged or foreign job: acknowledge so the agent stops resending.
            _ => {
                warn!(job = %event.job_id, node = node_id, "event for an unknown job dropped");
                return Ok(());
            }
        }
        let time_ms = if event.time_unix_ms > 0 {
            event.time_unix_ms
        } else {
            now_ms()
        };
        let mapped = events::map(body, time_ms);
        let inserted = self
            .store
            .insert_event(
                &event.job_id,
                seq,
                time_ms,
                i64::from(mapped.code),
                &mapped.data.to_string(),
            )
            .await?;
        match mapped.final_state {
            Some(final_state) => {
                if self.store.finish_job(&event.job_id, final_state).await? {
                    metrics::counter!("gorgone_jobs_finished_total", "state" => final_state)
                        .increment(1);
                }
            }
            None => self.store.mark_running(&event.job_id).await?,
        }
        if inserted {
            metrics::counter!("gorgone_job_events_total").increment(1);
        }
        self.hub.notify_job(&event.job_id);
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
        let dispatcher = Arc::clone(&self.dispatcher);
        let drained_node = node_id.clone();
        tokio::spawn(async move { dispatcher.drain(&drained_node).await });

        let idle = self.heartbeat_interval * 3;
        let reason = loop {
            let received = tokio::select! {
                () = handle.closed.cancelled() => break "closed by the server",
                received = tokio::time::timeout(idle, inbound.message()) => received,
            };
            let frame = match received {
                Err(_) => break "heartbeat timeout",
                Ok(Err(_)) => break "stream error",
                Ok(Ok(None)) => break "closed by the agent",
                Ok(Ok(Some(frame))) => frame.frame,
            };
            match frame {
                Some(AgentFrameKind::Heartbeat(_)) => {
                    match self.store.touch_session(&node_id, &handle.session_id).await {
                        Ok(true) => {}
                        // The node reconnected elsewhere, or we were declared dead.
                        Ok(false) => break "session replaced",
                        Err(err) => warn!(node = %node_id, error = %err, "heartbeat not recorded"),
                    }
                }
                Some(AgentFrameKind::Event(event)) => {
                    if let Err(err) = self.record_event(&node_id, &event).await {
                        // Not acknowledged: closing makes the agent resend it.
                        warn!(node = %node_id, job = %event.job_id, error = %err, "event not committed");
                        break "store failure";
                    }
                    let ack = ServerFrame {
                        frame: Some(Frame::Ack(EventAck {
                            job_id: event.job_id,
                            seq: event.seq,
                        })),
                    };
                    if handle.tx.send(Ok(ack)).await.is_err() {
                        break "closed by the agent";
                    }
                }
                Some(AgentFrameKind::Hello(_)) | None => {}
            }
        };
        info!(node = %node_id, session = %handle.session_id, reason, "agent session ended");
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
    type SessionStream = ReceiverStream<Result<ServerFrame, Status>>;
    type FetchBlobStream = ReceiverStream<Result<BlobChunk, Status>>;

    async fn session(
        &self,
        request: Request<Streaming<AgentFrame>>,
    ) -> Result<Response<Self::SessionStream>, Status> {
        let gateway = &self.0;
        let token = bearer_token(request.metadata());
        let cert_cn = peer_common_name(&request);
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
        gateway
            .authenticate(&hello.node_id, token.as_deref(), cert_cn)
            .await?;

        let (tx, rx) = mpsc::channel(SESSION_BUFFER);
        let handle = SessionHandle {
            session_id: Uuid::now_v7().to_string(),
            tx,
            closed: CancellationToken::new(),
            drain_lock: Arc::default(),
        };
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
                &hello.node_id,
                &handle.session_id,
                &gateway.replica_id,
                &hello.agent_version,
            )
            .await
            .map_err(internal)?;
        gateway.hub.register(&hello.node_id, handle.clone());
        info!(
            node = %hello.node_id,
            session = %handle.session_id,
            agent_version = %hello.agent_version,
            "agent session opened"
        );
        tokio::spawn(Arc::clone(gateway).run_session(hello.node_id, handle, inbound));
        Ok(Response::new(ReceiverStream::new(rx)))
    }

    async fn fetch_blob(
        &self,
        request: Request<FetchBlobRequest>,
    ) -> Result<Response<Self::FetchBlobStream>, Status> {
        let gateway = &self.0;
        let node_id = request
            .metadata()
            .get(NODE_ID_METADATA)
            .and_then(|v| v.to_str().ok())
            .ok_or_else(|| Status::unauthenticated("missing node id metadata"))?
            .to_owned();
        let token = bearer_token(request.metadata());
        let cert_cn = peer_common_name(&request);
        gateway
            .authenticate(&node_id, token.as_deref(), cert_cn)
            .await?;

        let digest = request.into_inner().digest.to_ascii_lowercase();
        if digest.len() != 64 || !digest.bytes().all(|b| b.is_ascii_hexdigit()) {
            return Err(Status::invalid_argument("digest must be a hex SHA-256"));
        }
        if gateway
            .store
            .blob_size(&digest)
            .await
            .map_err(internal)?
            .is_none()
        {
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
