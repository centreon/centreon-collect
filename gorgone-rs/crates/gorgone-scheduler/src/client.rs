//! BBDO connection to the central broker: connection, welcome exchange,
//! batched writes, acknowledgements, reconnection, and the memory queue of
//! the events not acknowledged yet (`broker/core/bbdo/stream.cc`).

use std::collections::VecDeque;
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::time::Duration;

use gorgone_proto::bbdo::{Ack, Bbdo, PeerType, Stop, Welcome};
use prost::Message;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::TcpStream;
use tokio::net::tcp::{OwnedReadHalf, OwnedWriteHalf};
use tokio::sync::{mpsc, oneshot, watch};
use tokio_util::sync::CancellationToken;

use crate::bbdo::{self, Decoder, event};

/// Version announced in the welcome; centreon-web configures 3.0.1.
const BBDO_VERSION: (u32, u32, u32) = (3, 0, 1);
/// Bound of the connection and of the welcome exchange.
const HANDSHAKE_TIMEOUT: Duration = Duration::from_secs(10);
/// A write that takes longer means the central does not read any more.
const WRITE_TIMEOUT: Duration = Duration::from_secs(30);
/// Time given to the central to acknowledge the last events when stopping.
const STOP_TIMEOUT: Duration = Duration::from_secs(5);
/// Bytes written at once at most.
const BATCH_BYTES: usize = 256 * 1024;
const BACKOFF_MIN: Duration = Duration::from_secs(1);
const BACKOFF_MAX: Duration = Duration::from_secs(30);

/// Where to connect and who we are.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Target {
    /// `host:port` of the central broker input.
    pub address: String,
    /// Poller id (`instance_id`).
    pub poller_id: u64,
    /// Poller name.
    pub poller_name: String,
    /// Name of the broker module of the poller.
    pub broker_name: String,
}

/// What the scheduler gives the connection.
#[derive(Debug)]
pub enum Outgoing {
    /// One encoded event (its packets).
    Event(Vec<u8>),
    /// Send what is queued, then `pb_stop`, wait for its acknowledgement and
    /// close; answers once done.
    Stop(oneshot::Sender<()>),
}

/// What the connection tells the scheduler.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Notice {
    /// The welcome exchange succeeded: the configuration must be sent.
    Connected,
    /// The connection was lost.
    Disconnected,
}

/// Counters readable from outside.
#[derive(Debug, Default)]
pub struct Stats {
    /// Connected and welcomed.
    pub connected: AtomicBool,
    /// Events acknowledged by the central since the start.
    pub acknowledged: AtomicU64,
    /// Events written since the start (resent ones counted again).
    pub written: AtomicU64,
    /// Events dropped because the queue was full.
    pub dropped: AtomicU64,
    /// Events waiting to be written or acknowledged.
    pub queued: AtomicU64,
}

/// Events not acknowledged yet, oldest first, bounded.
struct Queue {
    /// Written on the current connection, waiting for an acknowledgement.
    inflight: VecDeque<Vec<u8>>,
    /// Not written yet.
    pending: VecDeque<Vec<u8>>,
    max: usize,
    stats: Arc<Stats>,
}

impl Queue {
    fn push(&mut self, frame: Vec<u8>) {
        self.pending.push_back(frame);
        while self.inflight.len() + self.pending.len() > self.max {
            // The oldest not written yet goes first: a lost metric point
            // rather than a blocked scheduler.
            if self.pending.pop_front().is_none() {
                break;
            }
            let dropped = self.stats.dropped.fetch_add(1, Ordering::Relaxed) + 1;
            if dropped.is_power_of_two() {
                tracing::warn!(dropped, "BBDO queue full: oldest events dropped");
            }
        }
        self.update();
    }

    fn acknowledge(&mut self, count: u32) {
        let count = usize::try_from(count).unwrap_or(usize::MAX);
        let done = count.min(self.inflight.len());
        self.inflight.drain(..done);
        self.stats
            .acknowledged
            .fetch_add(done as u64, Ordering::Relaxed);
        self.update();
    }

    /// The connection is gone: what was not acknowledged is sent again first.
    fn requeue(&mut self) {
        while let Some(frame) = self.inflight.pop_back() {
            self.pending.push_front(frame);
        }
        self.update();
    }

    fn update(&self) {
        let queued = (self.inflight.len() + self.pending.len()) as u64;
        self.stats.queued.store(queued, Ordering::Relaxed);
    }
}

/// Why a connection ended.
enum End {
    /// Lost or refused: try again.
    Lost(String),
    /// The target changed: reconnect at once.
    Retarget,
    /// Stopped on purpose.
    Stopped,
}

/// The connection task.
pub struct Client {
    /// Events from the scheduler.
    pub events: mpsc::Receiver<Outgoing>,
    /// Current target; `None` while no configuration is loaded.
    pub target: watch::Receiver<Option<Target>>,
    /// Connection changes, to the scheduler.
    pub notices: mpsc::UnboundedSender<Notice>,
    /// Queue bound.
    pub max_pending: usize,
    /// Shared counters.
    pub stats: Arc<Stats>,
    /// Ends the task (after a `Stop`, or at once).
    pub cancel: CancellationToken,
}

/// Encodes the welcome of `target`.
#[must_use]
pub fn welcome(target: &Target) -> Vec<u8> {
    let (major, minor, patch) = BBDO_VERSION;
    bbdo::frame(
        event::WELCOME,
        &Welcome {
            version: Some(Bbdo {
                major,
                minor,
                patch,
            }),
            // No compression, no TLS.
            extensions: String::new(),
            poller_id: target.poller_id,
            poller_name: target.poller_name.clone(),
            broker_name: target.broker_name.clone(),
            peer_type: PeerType::Engine.into(),
            extended_negotiation: false,
        },
    )
}

/// Number of events an acknowledgement event carries, if it is one.
fn ack_count(event_type: u32, payload: &[u8]) -> Option<u32> {
    match event_type {
        event::PB_ACK => Ack::decode(payload).ok().map(|ack| ack.acknowledged_events),
        // The legacy mapping: one unsigned integer, network byte order.
        event::ACK => payload
            .get(..4)
            .map(|bytes| u32::from_be_bytes([bytes[0], bytes[1], bytes[2], bytes[3]])),
        _ => None,
    }
}

/// Reads the socket for acknowledgements until it closes.
async fn read_acks(
    mut reader: OwnedReadHalf,
    mut decoder: Decoder,
    acks: mpsc::UnboundedSender<u32>,
) {
    let mut chunk = vec![0u8; 4096];
    loop {
        while let Some((event_type, payload)) = decoder.next_event() {
            if let Some(count) = ack_count(event_type, &payload) {
                tracing::debug!(count, "BBDO acknowledgement received");
                if acks.send(count).is_err() {
                    return;
                }
            }
        }
        match reader.read(&mut chunk).await {
            Ok(0) | Err(_) => return,
            Ok(n) => decoder.push(&chunk[..n]),
        }
    }
}

impl Client {
    /// Runs until cancelled.
    pub async fn run(mut self) {
        let mut queue = Queue {
            inflight: VecDeque::new(),
            pending: VecDeque::new(),
            max: self.max_pending.max(1),
            stats: self.stats.clone(),
        };
        let mut backoff = BACKOFF_MIN;
        let mut stop_reply: Option<oneshot::Sender<()>> = None;
        loop {
            // A target first; events keep queueing meanwhile.
            let target = loop {
                if let Some(target) = self.target.borrow_and_update().clone() {
                    break target;
                }
                tokio::select! {
                    () = self.cancel.cancelled() => return,
                    changed = self.target.changed() => if changed.is_err() { return },
                    message = self.events.recv() => match message {
                        Some(Outgoing::Event(frame)) => queue.push(frame),
                        Some(Outgoing::Stop(reply)) => {
                            let _ = reply.send(());
                            return;
                        }
                        None => return,
                    },
                }
            };
            // Events keep queueing while connecting; a stop ends it.
            let cancel = self.cancel.clone();
            let connecting = connect(&target, &cancel);
            tokio::pin!(connecting);
            let connected = loop {
                tokio::select! {
                    result = &mut connecting => break result,
                    message = self.events.recv() => match message {
                        Some(Outgoing::Event(frame)) => queue.push(frame),
                        Some(Outgoing::Stop(reply)) => {
                            let _ = reply.send(());
                            return;
                        }
                        None => return,
                    },
                }
            };
            let end = match connected {
                Ok((reader, writer, decoder)) => {
                    backoff = BACKOFF_MIN;
                    self.stats.connected.store(true, Ordering::Relaxed);
                    tracing::info!(address = %target.address, poller_id = target.poller_id, "connected to the central broker");
                    let _ = self.notices.send(Notice::Connected);
                    let end = self
                        .session(
                            &target,
                            reader,
                            writer,
                            decoder,
                            &mut queue,
                            &mut stop_reply,
                        )
                        .await;
                    self.stats.connected.store(false, Ordering::Relaxed);
                    queue.requeue();
                    let _ = self.notices.send(Notice::Disconnected);
                    end
                }
                Err(error) => End::Lost(error),
            };
            match end {
                End::Stopped => {
                    if let Some(reply) = stop_reply.take() {
                        let _ = reply.send(());
                    }
                    return;
                }
                End::Retarget => continue,
                End::Lost(error) => {
                    tracing::warn!(address = %target.address, %error, retry_in = ?backoff, "central broker connection lost");
                }
            }
            // Wait before the next attempt, still queueing.
            let sleep = tokio::time::sleep(backoff);
            tokio::pin!(sleep);
            backoff = (backoff * 2).min(BACKOFF_MAX);
            loop {
                tokio::select! {
                    () = &mut sleep => break,
                    () = self.cancel.cancelled() => return,
                    changed = self.target.changed() => {
                        if changed.is_err() { return }
                        break;
                    }
                    message = self.events.recv() => match message {
                        Some(Outgoing::Event(frame)) => queue.push(frame),
                        Some(Outgoing::Stop(reply)) => {
                            let _ = reply.send(());
                            return;
                        }
                        None => return,
                    },
                }
            }
        }
    }
}

/// Connects and exchanges the welcomes.
async fn connect(
    target: &Target,
    cancel: &CancellationToken,
) -> Result<(OwnedReadHalf, OwnedWriteHalf, Decoder), String> {
    {
        let exchange = async {
            let mut stream = TcpStream::connect(&target.address)
                .await
                .map_err(|error| format!("{}: {error}", target.address))?;
            let _ = stream.set_nodelay(true);
            stream
                .write_all(&welcome(target))
                .await
                .map_err(|error| format!("sending the welcome: {error}"))?;
            let mut decoder = Decoder::default();
            let mut chunk = vec![0u8; 4096];
            loop {
                if let Some((event_type, payload)) = decoder.next_event() {
                    match event_type {
                        event::WELCOME => {
                            let peer = Welcome::decode(payload.as_slice())
                                .map_err(|error| format!("invalid welcome: {error}"))?;
                            let version = peer.version.unwrap_or_default();
                            if version.major != BBDO_VERSION.0 {
                                return Err(format!(
                                    "the central broker speaks BBDO {}.{}.{}, we speak {}.x",
                                    version.major, version.minor, version.patch, BBDO_VERSION.0
                                ));
                            }
                            tracing::debug!(
                                peer = %peer.broker_name,
                                version = %format!("{}.{}.{}", version.major, version.minor, version.patch),
                                "BBDO welcome received"
                            );
                            break;
                        }
                        // A BBDO 3.0.0 peer answers with the legacy welcome.
                        event::VERSION_RESPONSE => break,
                        other => {
                            return Err(format!(
                                "unexpected event {other:#x} instead of a welcome"
                            ));
                        }
                    }
                }
                let n = stream
                    .read(&mut chunk)
                    .await
                    .map_err(|error| format!("reading the welcome: {error}"))?;
                if n == 0 {
                    return Err("connection closed during the welcome exchange".to_owned());
                }
                decoder.push(&chunk[..n]);
            }
            let (reader, writer) = stream.into_split();
            Ok((reader, writer, decoder))
        };
        tokio::select! {
            result = tokio::time::timeout(HANDSHAKE_TIMEOUT, exchange) => {
                result.map_err(|_| "no welcome from the central broker in time".to_owned())?
            }
            () = cancel.cancelled() => Err("stopped".to_owned()),
        }
    }
}

impl Client {
    /// Writes the queue as it fills, until the connection ends.
    async fn session(
        &mut self,
        target: &Target,
        reader: OwnedReadHalf,
        mut writer: OwnedWriteHalf,
        decoder: Decoder,
        queue: &mut Queue,
        stop_reply: &mut Option<oneshot::Sender<()>>,
    ) -> End {
        let (ack_tx, mut acks) = mpsc::unbounded_channel();
        let reader_task = tokio::spawn(read_acks(reader, decoder, ack_tx));
        let _abort = AbortOnDrop(reader_task.abort_handle());
        let mut buffer = Vec::with_capacity(BATCH_BYTES);
        let mut stopping = false;
        loop {
            // Take what is there without waiting.
            while let Ok(message) = self.events.try_recv() {
                match message {
                    Outgoing::Event(frame) => queue.push(frame),
                    Outgoing::Stop(reply) => {
                        *stop_reply = Some(reply);
                        stopping = true;
                    }
                }
            }
            while let Ok(count) = acks.try_recv() {
                queue.acknowledge(count);
            }
            if !queue.pending.is_empty() {
                buffer.clear();
                let mut batch = 0;
                while let Some(frame) = queue.pending.front() {
                    if !buffer.is_empty() && buffer.len() + frame.len() > BATCH_BYTES {
                        break;
                    }
                    buffer.extend_from_slice(frame);
                    if let Some(frame) = queue.pending.pop_front() {
                        queue.inflight.push_back(frame);
                    }
                    batch += 1;
                }
                match tokio::time::timeout(WRITE_TIMEOUT, writer.write_all(&buffer)).await {
                    Ok(Ok(())) => {
                        self.stats.written.fetch_add(batch, Ordering::Relaxed);
                    }
                    Ok(Err(error)) => return End::Lost(format!("write: {error}")),
                    Err(_) => return End::Lost("write timed out".to_owned()),
                }
                continue;
            }
            if stopping {
                return self.stop(target, &mut writer, &mut acks, queue).await;
            }
            tokio::select! {
                message = self.events.recv() => match message {
                    Some(Outgoing::Event(frame)) => queue.push(frame),
                    Some(Outgoing::Stop(reply)) => {
                        *stop_reply = Some(reply);
                        stopping = true;
                    }
                    None => return End::Stopped,
                },
                count = acks.recv() => match count {
                    Some(count) => queue.acknowledge(count),
                    None => return End::Lost("closed by the central broker".to_owned()),
                },
                changed = self.target.changed() => {
                    if changed.is_err() {
                        return End::Stopped;
                    }
                    if self.target.borrow().as_ref() != Some(target) {
                        tracing::info!("central broker target changed: reconnecting");
                        return End::Retarget;
                    }
                }
                () = self.cancel.cancelled() => return End::Stopped,
            }
        }
    }

    /// `pb_stop`, then the acknowledgement of what is in flight.
    async fn stop(
        &self,
        target: &Target,
        writer: &mut OwnedWriteHalf,
        acks: &mut mpsc::UnboundedReceiver<u32>,
        queue: &mut Queue,
    ) -> End {
        let frame = bbdo::frame(
            event::PB_STOP,
            &Stop {
                poller_id: target.poller_id,
            },
        );
        if writer.write_all(&frame).await.is_ok() {
            let wait = async {
                while !queue.inflight.is_empty() {
                    match acks.recv().await {
                        Some(count) => queue.acknowledge(count),
                        None => break,
                    }
                }
            };
            let _ = tokio::time::timeout(STOP_TIMEOUT, wait).await;
        }
        let _ = writer.shutdown().await;
        if !queue.inflight.is_empty() {
            tracing::warn!(
                events = queue.inflight.len(),
                "stopping with events the central broker did not acknowledge"
            );
        }
        End::Stopped
    }
}

struct AbortOnDrop(tokio::task::AbortHandle);

impl Drop for AbortOnDrop {
    fn drop(&mut self) {
        self.0.abort();
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn queue_drops_the_oldest_and_requeues_in_order() {
        let stats = Arc::new(Stats::default());
        let mut queue = Queue {
            inflight: VecDeque::new(),
            pending: VecDeque::new(),
            max: 3,
            stats: stats.clone(),
        };
        for i in 0..5u8 {
            queue.push(vec![i]);
        }
        assert_eq!(queue.pending, [vec![2], vec![3], vec![4]]);
        assert_eq!(stats.dropped.load(Ordering::Relaxed), 2);
        // Two written, one acknowledged, then the connection is lost.
        for _ in 0..2 {
            let frame = queue.pending.pop_front().unwrap();
            queue.inflight.push_back(frame);
        }
        queue.acknowledge(1);
        queue.requeue();
        assert_eq!(queue.pending, [vec![3], vec![4]]);
        assert!(queue.inflight.is_empty());
        assert_eq!(stats.queued.load(Ordering::Relaxed), 2);
    }

    #[test]
    fn reads_both_kinds_of_acknowledgement() {
        let pb = Ack {
            acknowledged_events: 1000,
        }
        .encode_to_vec();
        assert_eq!(ack_count(event::PB_ACK, &pb), Some(1000));
        assert_eq!(ack_count(event::ACK, &[0, 0, 1, 2]), Some(258));
        assert_eq!(ack_count(event::WELCOME, &pb), None);
    }
}
