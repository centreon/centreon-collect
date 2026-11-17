//! The scheduler against a fake central broker speaking BBDO over TCP:
//! welcome exchange, configuration then statuses, SOFT then HARD, forced
//! check, reconnection with resync, reload, stop.

use std::os::unix::fs::PermissionsExt;
use std::path::Path;
use std::sync::Arc;
use std::time::Duration;

use gorgone_proto::bbdo::{Ack, Bbdo, Instance, PeerType, Service, ServiceStatus, Stop, Welcome};
use gorgone_scheduler::wire::{Decoder, event, frame};
use gorgone_scheduler::{Options, Scheduler};
use prost::Message;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::TcpListener;
use tokio::sync::{Notify, mpsc};

/// Accepts connections one after the other, answers the welcome, forwards
/// every other event and acknowledges them by two. `drop_connection` closes
/// the current one.
async fn fake_broker(
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

fn write_config(dir: &Path, port: u16, extra_service: bool) {
    let plugin = dir.join("plugin.sh");
    std::fs::write(&plugin, "#!/bin/sh\necho \"$2 | value=$1\"\nexit $1\n").unwrap();
    std::fs::set_permissions(&plugin, std::fs::Permissions::from_mode(0o755)).unwrap();
    std::fs::write(
        dir.join("centengine.cfg"),
        format!(
            "cfg_file={0}/objects.cfg\nresource_file={0}/resource.cfg\ninterval_length=1\n\
             status_update_interval=1\nbroker_module_cfg_file={0}/module.json\n",
            dir.display()
        ),
    )
    .unwrap();
    std::fs::write(
        dir.join("resource.cfg"),
        format!("$USER1$={}\n", dir.display()),
    )
    .unwrap();
    let mut objects = String::from(
        "define command {\n command_name dummy\n command_line $USER1$/plugin.sh $ARG1$ \"$ARG2$\"\n}\n\
         define host {\n name tpl\n check_command dummy!0!host up\n check_interval 1\n register 0\n}\n\
         define host {\n host_name h1\n use tpl\n address 127.0.0.1\n _HOST_ID 10\n}\n\
         define service {\n name stpl\n check_interval 1\n retry_interval 1\n max_check_attempts 2\n register 0\n}\n\
         define service {\n host_name h1\n service_description ok\n use stpl\n check_command dummy!0!fine\n _SERVICE_ID 1\n}\n\
         define service {\n host_name h1\n service_description crit\n use stpl\n check_command dummy!2!broken\n _SERVICE_ID 2\n}\n",
    );
    if extra_service {
        objects.push_str(
            "define service {\n host_name h1\n service_description added\n use stpl\n \
             check_command dummy!1!warn\n _SERVICE_ID 3\n}\n",
        );
    }
    std::fs::write(dir.join("objects.cfg"), objects).unwrap();
    std::fs::write(
        dir.join("module.json"),
        format!(
            r#"{{"centreonBroker":{{"poller_id":5,"poller_name":"rust","broker_name":"rust-module",
               "output":[{{"type":"ipv4","host":"127.0.0.1","port":"{port}"}}]}}}}"#
        ),
    )
    .unwrap();
}

/// Waits for the first event matching `found`, at most `limit`.
async fn wait_for<T>(
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

#[tokio::test(flavor = "multi_thread", worker_threads = 2)]
async fn runs_checks_and_feeds_a_bbdo_peer() {
    let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
    let port = listener.local_addr().unwrap().port();
    let dir = tempfile::tempdir().unwrap();
    write_config(dir.path(), port, false);
    let (events_tx, mut events) = mpsc::unbounded_channel();
    let (welcomes_tx, mut welcomes) = mpsc::unbounded_channel();
    let drop_connection = Arc::new(Notify::new());
    tokio::spawn(fake_broker(
        listener,
        events_tx,
        welcomes_tx,
        drop_connection.clone(),
    ));

    let scheduler = Scheduler::start(Options::new(dir.path().join("centengine.cfg")));
    let welcome = tokio::time::timeout(Duration::from_secs(10), welcomes.recv())
        .await
        .unwrap()
        .unwrap();
    assert_eq!(welcome.poller_id, 5);
    assert_eq!(welcome.poller_name, "rust");
    assert_eq!(welcome.peer_type, PeerType::Engine as i32);
    assert_eq!(welcome.version.unwrap().major, 3);
    assert!(welcome.extensions.is_empty());

    // The configuration first, in Engine's order.
    let mut order = Vec::new();
    while order.len() < 5 {
        let (event_type, _) = events.recv().await.unwrap();
        order.push(event_type);
    }
    assert_eq!(
        order,
        [
            event::INSTANCE,
            event::HOST,
            event::SERVICE,
            event::SERVICE,
            event::INSTANCE_CONFIGURATION
        ]
    );
    let status = scheduler.status();
    assert!(status.loaded);
    assert_eq!((status.poller_id, status.hosts, status.services), (5, 1, 2));

    // max_check_attempts 2, retry 1 s: CRITICAL goes SOFT then HARD.
    let mut seen_soft = false;
    let hard = wait_for(
        &mut events,
        Duration::from_secs(15),
        |event_type, payload| {
            if event_type != event::SERVICE_STATUS {
                return None;
            }
            let status = ServiceStatus::decode(payload).unwrap();
            if status.service_id != 2 || !status.checked {
                return None;
            }
            if status.state_type == 0 {
                seen_soft = true;
                assert_eq!(status.check_attempt, 1);
            }
            (status.state_type == 1).then_some(status)
        },
    )
    .await;
    assert!(seen_soft, "the problem was hard at once");
    assert_eq!((hard.state, hard.check_attempt), (2, 2));
    assert_eq!(hard.output, "broken");
    assert_eq!(hard.perfdata, "value=2");
    assert_eq!(hard.host_id, 10);

    // A forced check runs at once.
    let now = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let answer = scheduler
        .external_command(&format!("[{now}] SCHEDULE_FORCED_SVC_CHECK;h1;ok;{now}"))
        .await
        .unwrap();
    assert!(
        answer.contains("scheduled") || answer.contains("running"),
        "{answer}"
    );
    let refused = scheduler
        .external_command("ACKNOWLEDGE_SVC_PROBLEM;h1;crit;1;1;1;me;x")
        .await
        .unwrap_err();
    assert!(refused.contains("not supported"), "{refused}");

    // A lost connection: the scheduler reconnects and sends everything again.
    drop_connection.notify_one();
    tokio::time::timeout(Duration::from_secs(10), welcomes.recv())
        .await
        .expect("no reconnection")
        .unwrap();
    wait_for(
        &mut events,
        Duration::from_secs(10),
        |event_type, payload| {
            (event_type == event::INSTANCE && Instance::decode(payload).unwrap().running)
                .then_some(())
        },
    )
    .await;

    // Reload with one more service: it is declared.
    write_config(dir.path(), port, true);
    let summary = scheduler.reload().await.unwrap();
    assert!(summary.contains("3 services"), "{summary}");
    wait_for(
        &mut events,
        Duration::from_secs(10),
        |event_type, payload| {
            (event_type == event::SERVICE && Service::decode(payload).unwrap().service_id == 3)
                .then_some(())
        },
    )
    .await;
    // An invalid one is refused and changes nothing.
    std::fs::write(dir.path().join("objects.cfg"), "define service {\n").unwrap();
    let report = scheduler.reload().await.unwrap_err();
    assert!(report.contains("Total Errors:"), "{report}");
    assert_eq!(scheduler.status().services, 3);

    // Stop: instance not running, then pb_stop, acknowledged.
    scheduler.shutdown().await;
    let mut stopped = false;
    wait_for(
        &mut events,
        Duration::from_secs(10),
        |event_type, payload| {
            if event_type == event::INSTANCE && !Instance::decode(payload).unwrap().running {
                stopped = true;
            }
            (event_type == event::PB_STOP)
                .then(|| Stop::decode(payload).unwrap().poller_id)
                .map(|poller_id| assert_eq!(poller_id, 5))
        },
    )
    .await;
    assert!(stopped, "no instance end before pb_stop");
    assert!(
        scheduler
            .stats()
            .acknowledged
            .load(std::sync::atomic::Ordering::Relaxed)
            > 0
    );
}
