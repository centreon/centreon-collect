//! BBDO sink for load tests (`deploy/snmp-load/`): accepts the engine's BBDO
//! stream like a central broker, acknowledges everything and writes one CSV
//! line per checked service status, for the analysis script. Prints a
//! summary every 10 s.
//!
//! ```bash
//! cargo run --release -p centreon-engine --example bbdo_sink -- 0.0.0.0:5669 statuses.csv
//! ```
//! CSV columns: received (unix s), host_id, service_id, last_check,
//! latency (s), execution_time (s), state, output (first 60 bytes).

use std::io::Write;
use std::sync::{Arc, Mutex};
use std::time::{Duration, SystemTime, UNIX_EPOCH};

use centreon_engine::wire::{Decoder, event, frame};
use gorgone_proto::bbdo::{Ack, Bbdo, ServiceStatus, Welcome};
use prost::Message;
use tokio::io::{AsyncReadExt, AsyncWriteExt};

#[derive(Default)]
struct Window {
    statuses: u64,
    states: [u64; 5],
    latency: Vec<f64>,
    execution: Vec<f64>,
}

fn percentile(values: &mut [f64], p: f64) -> f64 {
    if values.is_empty() {
        return 0.0;
    }
    values.sort_by(f64::total_cmp);
    let rank = ((values.len() - 1) as f64 * p).round() as usize;
    values[rank]
}

fn now() -> f64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map_or(0.0, |d| d.as_secs_f64())
}

async fn serve(
    mut socket: tokio::net::TcpStream,
    csv: Arc<Mutex<std::io::BufWriter<std::fs::File>>>,
    window: Arc<Mutex<Window>>,
) {
    let mut decoder = Decoder::default();
    let mut chunk = vec![0u8; 1 << 16];
    loop {
        let mut out = Vec::new();
        let mut unacked = 0;
        let received = now();
        while let Some((event_type, payload)) = decoder.next_event() {
            if event_type == event::WELCOME {
                out.extend(frame(
                    event::WELCOME,
                    &Welcome {
                        version: Some(Bbdo {
                            major: 3,
                            minor: 0,
                            patch: 1,
                        }),
                        ..Welcome::default()
                    },
                ));
                continue;
            }
            unacked += 1;
            if event_type != event::SERVICE_STATUS {
                continue;
            }
            let Ok(status) = ServiceStatus::decode(payload.as_slice()) else {
                continue;
            };
            // The configuration dump sends a status for every service.
            if status.last_check == 0 {
                continue;
            }
            let output: String = status
                .output
                .chars()
                .take(60)
                .map(|c| if c == ',' || c == '\n' { ' ' } else { c })
                .collect();
            if let Ok(mut csv) = csv.lock() {
                let _ = writeln!(
                    csv,
                    "{received:.3},{},{},{},{:.4},{:.4},{},{output}",
                    status.host_id,
                    status.service_id,
                    status.last_check,
                    status.latency,
                    status.execution_time,
                    status.state
                );
            }
            if let Ok(mut window) = window.lock() {
                window.statuses += 1;
                window.states[status.state.clamp(0, 4) as usize] += 1;
                window.latency.push(status.latency);
                window.execution.push(status.execution_time);
            }
        }
        if unacked > 0 {
            out.extend(frame(
                event::PB_ACK,
                &Ack {
                    acknowledged_events: unacked,
                },
            ));
        }
        if !out.is_empty() && socket.write_all(&out).await.is_err() {
            return;
        }
        match socket.read(&mut chunk).await {
            Ok(0) | Err(_) => return,
            Ok(n) => decoder.push(&chunk[..n]),
        }
    }
}

#[tokio::main(flavor = "current_thread")]
async fn main() {
    let mut args = std::env::args().skip(1);
    let listen = args.next().unwrap_or_else(|| "0.0.0.0:5669".to_owned());
    let path = args.next().unwrap_or_else(|| "statuses.csv".to_owned());
    let file = std::fs::File::create(&path).expect("CSV file");
    let csv = Arc::new(Mutex::new(std::io::BufWriter::new(file)));
    let window = Arc::new(Mutex::new(Window::default()));
    let listener = tokio::net::TcpListener::bind(&listen)
        .await
        .expect("listen");
    println!("listening on {listen}, statuses to {path}");

    let (report_csv, report_window) = (Arc::clone(&csv), Arc::clone(&window));
    tokio::spawn(async move {
        let mut tick = tokio::time::interval(Duration::from_secs(10));
        tick.tick().await;
        loop {
            tick.tick().await;
            if let Ok(mut csv) = report_csv.lock() {
                let _ = csv.flush();
            }
            let mut w = std::mem::take(&mut *report_window.lock().unwrap());
            println!(
                "{:.0} statuses={} rate={:.1}/s ok={} warn={} crit={} unknown={} \
                 latency p50={:.2} p95={:.2} max={:.2} exec p50={:.3} p95={:.3} max={:.3}",
                now(),
                w.statuses,
                w.statuses as f64 / 10.0,
                w.states[0],
                w.states[1],
                w.states[2],
                w.states[3],
                percentile(&mut w.latency, 0.5),
                percentile(&mut w.latency, 0.95),
                percentile(&mut w.latency, 1.0),
                percentile(&mut w.execution, 0.5),
                percentile(&mut w.execution, 0.95),
                percentile(&mut w.execution, 1.0),
            );
        }
    });

    loop {
        let Ok((socket, peer)) = listener.accept().await else {
            continue;
        };
        println!("connection from {peer}");
        tokio::spawn(serve(socket, Arc::clone(&csv), Arc::clone(&window)));
    }
}
