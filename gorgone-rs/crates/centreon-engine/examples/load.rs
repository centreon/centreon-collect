//! Synthetic load: N services checked every I seconds with `/bin/true`, the
//! scheduler on one thread (current-thread runtime), the results sent over
//! BBDO to an in-process fake central broker running on another thread.
//! Prints, every 10 s, the checks done and the CPU time and resident memory
//! of the scheduler thread (plugins and fake broker excluded).
//!
//! ```bash
//! cargo run --release -p centreon-engine --example load -- 10000 300 330
//! ```
//! Arguments: services (10000), interval in seconds (300), duration in
//! seconds (330).

use std::sync::Arc;
use std::sync::atomic::{AtomicU64, Ordering};
use std::time::{Duration, Instant};

use centreon_engine::wire::{Decoder, event, frame};
use centreon_engine::{Options, Scheduler};
use gorgone_proto::bbdo::{Ack, Bbdo, Welcome};
use tokio::io::{AsyncReadExt, AsyncWriteExt};

/// Statuses received by the fake broker.
static STATUSES: AtomicU64 = AtomicU64::new(0);

async fn fake_broker(listener: tokio::net::TcpListener) {
    loop {
        let Ok((mut socket, _)) = listener.accept().await else {
            return;
        };
        let mut decoder = Decoder::default();
        let mut chunk = vec![0u8; 1 << 16];
        let mut unacked = 0;
        loop {
            let mut out = Vec::new();
            while let Some((event_type, _)) = decoder.next_event() {
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
                if event_type == event::SERVICE_STATUS {
                    STATUSES.fetch_add(1, Ordering::Relaxed);
                }
                unacked += 1;
                if unacked == 1000 {
                    out.extend(frame(
                        event::PB_ACK,
                        &Ack {
                            acknowledged_events: unacked,
                        },
                    ));
                    unacked = 0;
                }
            }
            if !out.is_empty() && socket.write_all(&out).await.is_err() {
                break;
            }
            match socket.read(&mut chunk).await {
                Ok(0) | Err(_) => break,
                Ok(n) => decoder.push(&chunk[..n]),
            }
        }
    }
}

/// (user + system CPU seconds of thread `tid`, of the reaped children).
fn cpu(tid: u32) -> (f64, f64) {
    let read = |path: String| std::fs::read_to_string(path).unwrap_or_default();
    let fields = |text: &str| -> Vec<f64> {
        // Fields after the command name, which is in parentheses.
        let rest = text.rsplit_once(')').map_or("", |(_, rest)| rest);
        rest.split_whitespace()
            .map(|field| field.parse().unwrap_or(0.0))
            .collect()
    };
    let thread = fields(&read(format!("/proc/self/task/{tid}/stat")));
    let process = fields(&read("/proc/self/stat".to_owned()));
    // utime, stime at 11 and 12 (from the state field, 0-based); cutime,
    // cstime at 13 and 14. Clock ticks of 10 ms.
    let ticks = |values: &[f64], a: usize, b: usize| {
        (values.get(a).copied().unwrap_or(0.0) + values.get(b).copied().unwrap_or(0.0)) / 100.0
    };
    (ticks(&thread, 11, 12), ticks(&process, 13, 14))
}

fn rss_mib() -> f64 {
    std::fs::read_to_string("/proc/self/status")
        .unwrap_or_default()
        .lines()
        .find_map(|line| line.strip_prefix("VmRSS:"))
        .and_then(|value| {
            value
                .trim()
                .trim_end_matches("kB")
                .trim()
                .parse::<f64>()
                .ok()
        })
        .map_or(0.0, |kib| kib / 1024.0)
}

fn main() {
    let args: Vec<u64> = std::env::args()
        .skip(1)
        .map(|arg| arg.parse().expect("numbers"))
        .collect();
    let services = args.first().copied().unwrap_or(10_000);
    let interval = args.get(1).copied().unwrap_or(300);
    let duration = args.get(2).copied().unwrap_or(330);

    // The fake broker on its own thread and runtime.
    let (port_tx, port_rx) = std::sync::mpsc::channel();
    std::thread::spawn(move || {
        let runtime = tokio::runtime::Builder::new_current_thread()
            .enable_all()
            .build()
            .unwrap();
        runtime.block_on(async move {
            let listener = tokio::net::TcpListener::bind("127.0.0.1:0").await.unwrap();
            port_tx.send(listener.local_addr().unwrap().port()).unwrap();
            fake_broker(listener).await;
        });
    });
    let port = port_rx.recv().unwrap();

    let dir = tempfile::tempdir().unwrap();
    let mut objects = String::from(
        "define command {\n command_name true\n command_line /bin/true\n}\n\
         define service {\n name tpl\n check_command true\n check_interval 1\n register 0\n}\n",
    );
    let hosts = services.div_ceil(20);
    for host in 1..=hosts {
        objects.push_str(&format!(
            "define host {{\n host_name h{host}\n address 127.0.0.1\n _HOST_ID {host}\n}}\n"
        ));
    }
    for service in 1..=services {
        let host = (service - 1) / 20 + 1;
        objects.push_str(&format!(
            "define service {{\n host_name h{host}\n service_description s{service}\n use tpl\n \
             _SERVICE_ID {service}\n}}\n"
        ));
    }
    std::fs::write(dir.path().join("objects.cfg"), objects).unwrap();
    std::fs::write(
        dir.path().join("centengine.cfg"),
        // One interval unit of `interval` seconds: check_interval 1.
        format!(
            "cfg_file=objects.cfg\ninterval_length={interval}\nmax_service_check_spread=60\n\
             broker_module_cfg_file=module.json\n"
        ),
    )
    .unwrap();
    std::fs::write(
        dir.path().join("module.json"),
        format!(
            r#"{{"centreonBroker":{{"poller_id":1,"poller_name":"load",
               "output":[{{"type":"ipv4","host":"127.0.0.1","port":"{port}"}}]}}}}"#
        ),
    )
    .unwrap();

    let runtime = tokio::runtime::Builder::new_current_thread()
        .enable_all()
        .build()
        .unwrap();
    // The scheduler runs on this thread.
    let tid = std::process::id();
    runtime.block_on(async move {
        let mut options = Options::new(dir.path().join("centengine.cfg"));
        options.max_concurrent_checks = 256;
        let scheduler = Arc::new(Scheduler::start(options));
        let started = Instant::now();
        let (cpu0, children0) = cpu(tid);
        println!(
            "services={services} hosts={hosts} interval={interval}s expected={:.1} checks/s",
            services as f64 / interval as f64
        );
        let mut last = (0, cpu0);
        while started.elapsed() < Duration::from_secs(duration) {
            tokio::time::sleep(Duration::from_secs(10)).await;
            let done = STATUSES.load(Ordering::Relaxed);
            let (cpu_now, children) = cpu(tid);
            let elapsed = started.elapsed().as_secs_f64();
            println!(
                "t={elapsed:5.0}s statuses={done:7} rate={:6.1}/s scheduler_cpu={:5.1}% \
                 total_cpu={:6.2}s plugins_cpu={:6.2}s rss={:6.1}MiB queued={}",
                (done - last.0) as f64 / 10.0,
                (cpu_now - last.1) * 10.0,
                cpu_now - cpu0,
                children - children0,
                rss_mib(),
                scheduler.stats().queued.load(Ordering::Relaxed),
            );
            last = (done, cpu_now);
        }
        let (cpu_end, _) = cpu(tid);
        let done = STATUSES.load(Ordering::Relaxed);
        println!(
            "end: {done} statuses ({} checks) in {:.0}s, scheduler thread {:.2}s CPU ({:.1}%), {:.1} µs per check, rss {:.1} MiB",
            done.saturating_sub(services),
            started.elapsed().as_secs_f64(),
            cpu_end - cpu0,
            (cpu_end - cpu0) / started.elapsed().as_secs_f64() * 100.0,
            // The first status of each service comes from the configuration dump.
            (cpu_end - cpu0) / done.saturating_sub(services).max(1) as f64 * 1e6,
            rss_mib()
        );
        scheduler.shutdown().await;
    });
}
