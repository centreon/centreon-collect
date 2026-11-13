//! Control stream with the server: handshake, heartbeat, dispatch intake,
//! event delivery, reconnection.

use std::collections::hash_map::RandomState;
use std::hash::{BuildHasher, Hasher};
use std::sync::Arc;
use std::time::Duration;

use gorgone_core::kind;
use gorgone_proto::agent::{AgentFrame, Heartbeat, Hello, ServerFrame, agent_frame, server_frame};
use tokio::sync::mpsc;
use tokio_stream::wrappers::ReceiverStream;
use tokio_util::sync::CancellationToken;

use crate::client::GatewayClient;
use crate::journal::Journal;
use crate::scheduler::Scheduler;

const HANDSHAKE_TIMEOUT: Duration = Duration::from_secs(30);
const DEFAULT_HEARTBEAT: Duration = Duration::from_secs(15);
const OUTBOUND_BUFFER: usize = 32;

/// Why a session ended.
#[derive(Debug, thiserror::Error)]
enum SessionError {
    #[error("{0:?}: {1}")]
    Rpc(tonic::Code, String),
    #[error("no answer from the server in {HANDSHAKE_TIMEOUT:?}")]
    HandshakeTimeout,
    #[error("the server did not answer with a welcome frame")]
    NoWelcome,
    #[error("stream closed by the server")]
    Closed,
}

impl From<tonic::Status> for SessionError {
    fn from(status: tonic::Status) -> Self {
        Self::Rpc(status.code(), status.message().to_owned())
    }
}

/// Number in `[0, 1)`, good enough to spread reconnections: every
/// `RandomState` hashes with its own keys, seeded by the system.
fn jitter() -> f64 {
    let bits = RandomState::new().build_hasher().finish();
    (bits >> 11) as f64 / (1u64 << 53) as f64
}

/// Jittered exponential backoff.
pub(crate) struct Backoff {
    min: Duration,
    max: Duration,
    current: Duration,
}

impl Backoff {
    pub(crate) fn new(min: Duration, max: Duration) -> Self {
        Self {
            min,
            max,
            current: min,
        }
    }

    pub(crate) fn reset(&mut self) {
        self.current = self.min;
    }

    /// Next delay: uniform in `[current / 2, current]`, then `current` doubles.
    pub(crate) fn next_delay(&mut self) -> Duration {
        let ceiling = self.current;
        self.current = ceiling.saturating_mul(2).min(self.max);
        ceiling.mul_f64(0.5 + jitter() / 2.0)
    }
}

/// What the session loop needs, shared with the rest of the agent.
pub(crate) struct Session {
    pub client: GatewayClient,
    pub node_id: String,
    pub journal: Arc<Journal>,
    pub scheduler: Arc<Scheduler>,
    pub backoff: Backoff,
}

impl Session {
    /// Keeps a session open until `stop` is cancelled.
    pub(crate) async fn run(mut self, stop: CancellationToken) {
        loop {
            let mut established = false;
            let ended = tokio::select! {
                ended = self.serve(&mut established) => ended,
                () = stop.cancelled() => return,
            };
            if established {
                self.backoff.reset();
            }
            let delay = self.backoff.next_delay();
            tracing::warn!(error = %ended, retry_in = ?delay, "session ended");
            tokio::select! {
                () = tokio::time::sleep(delay) => {}
                () = stop.cancelled() => return,
            }
        }
    }

    /// Runs one session; only returns when it ends.
    async fn serve(&mut self, established: &mut bool) -> SessionError {
        match self.serve_inner(established).await {
            Ok(never) => match never {},
            Err(error) => error,
        }
    }

    async fn serve_inner(
        &mut self,
        established: &mut bool,
    ) -> Result<std::convert::Infallible, SessionError> {
        let (outbound, frames) = mpsc::channel::<AgentFrame>(OUTBOUND_BUFFER);
        let hello = Hello {
            node_id: self.node_id.clone(),
            agent_version: env!("CARGO_PKG_VERSION").to_owned(),
            capabilities: [
                kind::RUN_COMMANDS,
                kind::ENGINE_COMMANDS,
                kind::APPLY_CONFIG,
                kind::SERVICE_CONTROL,
            ]
            .map(str::to_owned)
            .to_vec(),
        };
        // Queued before the call: the server reads it before answering.
        outbound
            .send(frame(agent_frame::Frame::Hello(hello)))
            .await
            .map_err(|_| SessionError::Closed)?;

        let handshake = async {
            let mut inbound = self
                .client
                .session(ReceiverStream::new(frames))
                .await?
                .into_inner();
            match inbound.message().await? {
                Some(ServerFrame {
                    frame: Some(server_frame::Frame::Welcome(welcome)),
                }) => Ok((inbound, welcome)),
                Some(_) => Err(SessionError::NoWelcome),
                None => Err(SessionError::Closed),
            }
        };
        let (mut inbound, welcome) = tokio::time::timeout(HANDSHAKE_TIMEOUT, handshake)
            .await
            .map_err(|_| SessionError::HandshakeTimeout)??;
        *established = true;
        tracing::info!(
            session_id = %welcome.session_id,
            server_version = %welcome.server_version,
            "session established"
        );

        // Everything the previous session did not get acknowledged goes again.
        self.journal.begin_session();
        let period = match welcome.heartbeat_interval_s {
            0 => DEFAULT_HEARTBEAT,
            seconds => Duration::from_secs(u64::from(seconds)),
        };
        let mut heartbeat = tokio::time::interval(period);
        heartbeat.set_missed_tick_behavior(tokio::time::MissedTickBehavior::Delay);

        loop {
            let pending = self.journal.has_unsent();
            tokio::select! {
                message = inbound.message() => match message?.and_then(|m| m.frame) {
                    Some(server_frame::Frame::Dispatch(dispatch)) => {
                        if let Some(job) = dispatch.job {
                            self.scheduler.submit(job);
                        }
                    }
                    Some(server_frame::Frame::Ack(ack)) => self.journal.ack(&ack.job_id, ack.seq),
                    Some(server_frame::Frame::Cancel(cancel)) => {
                        tracing::info!(job_id = %cancel.job_id, "cancel requested");
                        self.journal.cancel(&cancel.job_id);
                    }
                    Some(server_frame::Frame::Welcome(_)) => {}
                    None => return Err(SessionError::Closed),
                },
                _ = heartbeat.tick() => {
                    let running_jobs = u32::try_from(self.journal.unfinished()).unwrap_or(u32::MAX);
                    let beat = frame(agent_frame::Frame::Heartbeat(Heartbeat { running_jobs }));
                    // A full buffer means events are flowing: the beat can be skipped.
                    if let Err(mpsc::error::TrySendError::Closed(_)) = outbound.try_send(beat) {
                        return Err(SessionError::Closed);
                    }
                }
                // Reserving first keeps the loop reading acks under back-pressure.
                permit = outbound.reserve(), if pending => {
                    let permit = permit.map_err(|_| SessionError::Closed)?;
                    if let Some(event) = self.journal.pop_unsent() {
                        permit.send(frame(agent_frame::Frame::Event(event)));
                    }
                }
                () = self.journal.outbound_ready(), if !pending => {}
            }
        }
    }
}

fn frame(frame: agent_frame::Frame) -> AgentFrame {
    AgentFrame { frame: Some(frame) }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn jitter_varies_within_its_range() {
        let draws: Vec<f64> = (0..64).map(|_| jitter()).collect();
        assert!(draws.iter().all(|draw| (0.0..1.0).contains(draw)));
        assert!(draws.iter().any(|draw| *draw < 0.5) && draws.iter().any(|draw| *draw >= 0.5));
    }

    #[test]
    fn backoff_grows_to_max_and_resets() {
        let (min, max) = (Duration::from_millis(100), Duration::from_millis(450));
        let mut backoff = Backoff::new(min, max);
        for ceiling in [100, 200, 400, 450, 450] {
            let delay = backoff.next_delay();
            let ceiling = Duration::from_millis(ceiling);
            assert!(delay >= ceiling / 2 && delay <= ceiling, "{delay:?}");
        }
        backoff.reset();
        assert!(backoff.next_delay() <= min);
    }
}
