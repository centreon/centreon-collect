//! Fake central broker shared by the integration tests.

#![allow(dead_code)]

use std::sync::Arc;
use std::time::Duration;

use centreon_engine::wire::{Decoder, event, frame};
use gorgone_proto::bbdo::{Ack, Bbdo, PeerType, Welcome};
use prost::Message;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::TcpListener;
use tokio::sync::{Notify, mpsc};

/// Accepts connections one after the other, answers the welcome, forwards
/// every other event and acknowledges them by two. `drop_connection` closes
/// the current one.
pub async fn fake_broker(
    listener: TcpListener,
    events: mpsc::UnboundedSender<(u32, Vec<u8>)>,
    welcomes: mpsc::UnboundedSender<Welcome>,
    drop_connection: Arc<Notify>,
) {
    loop {
        let Ok((mut socket, _)) = listener.accept().await else {
            return;
        };
        let mut decoder = Decoder::default();
        let mut chunk = vec![0u8; 65536];
        let mut unacked = 0u32;
        loop {
            let mut outgoing = Vec::new();
            while let Some((event_type, payload)) = decoder.next_event() {
                if event_type == event::WELCOME {
                    let _ = welcomes.send(Welcome::decode(payload.as_slice()).unwrap());
                    outgoing.extend(frame(
                        event::WELCOME,
                        &Welcome {
                            version: Some(Bbdo {
                                major: 3,
                                minor: 0,
                                patch: 1,
                            }),
                            broker_name: "central-broker-master".into(),
                            peer_type: PeerType::Broker.into(),
                            ..Welcome::default()
                        },
                    ));
                    continue;
                }
                if event_type >> 16 == 1 {
                    unacked += 1;
                }
                let stop = event_type == event::PB_STOP;
                if unacked >= 2 || (stop && unacked > 0) {
                    outgoing.extend(frame(
                        event::PB_ACK,
                        &Ack {
                            acknowledged_events: unacked,
                        },
                    ));
                    unacked = 0;
                }
                let _ = events.send((event_type, payload));
            }
            if !outgoing.is_empty() && socket.write_all(&outgoing).await.is_err() {
                break;
            }
            tokio::select! {
                read = socket.read(&mut chunk) => match read {
                    Ok(0) | Err(_) => break,
                    Ok(n) => decoder.push(&chunk[..n]),
                },
                () = drop_connection.notified() => break,
            }
        }
    }
}

/// Waits for the first event matching `found`, at most `limit`.
pub async fn wait_for<T>(
    events: &mut mpsc::UnboundedReceiver<(u32, Vec<u8>)>,
    limit: Duration,
    mut found: impl FnMut(u32, &[u8]) -> Option<T>,
) -> T {
    let deadline = tokio::time::Instant::now() + limit;
    loop {
        let (event_type, payload) = tokio::time::timeout_at(deadline, events.recv())
            .await
            .expect("event not received in time")
            .expect("fake broker gone");
        if let Some(value) = found(event_type, &payload) {
            return value;
        }
    }
}
