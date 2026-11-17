//! Notifications, event handlers and freshness against a fake central
//! broker, with a version 4 structured configuration and real processes:
//! the notification and event handler commands write what they got to
//! files.

mod common;

use std::os::unix::fs::PermissionsExt;
use std::path::Path;
use std::sync::Arc;
use std::time::Duration;

use centreon_engine::wire::event;
use centreon_engine::{Options, Scheduler};
use common::fake_broker;
use gorgone_proto::bbdo::{AdaptiveService, InstanceStatus, LogEntry, ServiceStatus};
use prost::Message;
use serde_json::json;
use tokio::net::TcpListener;
use tokio::sync::{Notify, mpsc};

fn write_plugins(dir: &Path) {
    for (name, body) in [
        ("plugin.sh", "#!/bin/sh\necho \"$2\"\nexit $1\n"),
        // One line per call, in the file named first.
        (
            "record.sh",
            "#!/bin/sh\nfile=$1\nshift\necho \"$*\" >> \"$file\"\n",
        ),
    ] {
        let path = dir.join(name);
        std::fs::write(&path, body).unwrap();
        std::fs::set_permissions(&path, std::fs::Permissions::from_mode(0o755)).unwrap();
    }
}

fn document(dir: &Path, port: u16) -> Vec<u8> {
    let d = dir.display().to_string();
    serde_json::to_vec(&json!({
        "version": 4,
        "poller": {"id": 6, "name": "rust"},
        "broker": {"address": format!("127.0.0.1:{port}"), "name": "rust-module"},
        "settings": {"interval_length": 1, "status_update_interval": 1,
                     "log_level_notifications": "info", "service_freshness_check_interval": 1,
                     "notification_timeout": 5},
        "resource_macros": {"USER1": d},
        "commands": [
            {"command_name": "dummy", "command_line": "$USER1$/plugin.sh $ARG1$ \"$ARG2$\""},
            {"command_name": "notify-mail", "command_line":
             "$USER1$/record.sh $USER1$/notified $NOTIFICATIONTYPE$ $NOTIFICATIONNUMBER$ $CONTACTNAME$ $CONTACTEMAIL$ $SERVICEDESC$ $SERVICESTATE$ $SERVICEOUTPUT$"},
            {"command_name": "handler", "command_line":
             "$USER1$/record.sh $USER1$/handled $ARG1$ $SERVICESTATE$ $SERVICESTATETYPE$ $SERVICEATTEMPT$"}
        ],
        "contacts": [{"contact_name": "admin", "email": "admin@example.test",
                      "service_notification_options": "w,c,r",
                      "service_notification_commands": ["notify-mail"]}],
        "hosts": [{"host_name": "h1", "address": "127.0.0.1", "host_id": 10, "contacts": ["admin"]}],
        "services": [
            {"host_name": "h1", "service_description": "crit", "check_command": "dummy!2!broken",
             "check_interval": 1, "retry_interval": 1, "max_check_attempts": 2,
             "notification_interval": 2, "event_handler": "handler!on-crit", "service_id": 1},
            {"host_name": "h1", "service_description": "passive", "check_command": "dummy!0!forced",
             "active_checks_enabled": false, "check_freshness": true, "freshness_threshold": 2,
             "service_id": 2}
        ]
    }))
    .unwrap()
}

/// Lines of `path`, waiting until `found` holds for them.
async fn wait_lines(path: &Path, found: impl Fn(&[String]) -> bool) -> Vec<String> {
    for _ in 0..100 {
        let lines: Vec<String> = std::fs::read_to_string(path)
            .unwrap_or_default()
            .lines()
            .map(str::to_owned)
            .collect();
        if found(&lines) {
            return lines;
        }
        tokio::time::sleep(Duration::from_millis(200)).await;
    }
    panic!("{}: not found", path.display());
}

#[tokio::test(flavor = "multi_thread", worker_threads = 2)]
async fn notifications_event_handlers_and_freshness() {
    let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
    let port = listener.local_addr().unwrap().port();
    let dir = tempfile::tempdir().unwrap();
    write_plugins(dir.path());
    let (events_tx, mut events) = mpsc::unbounded_channel();
    let (welcomes_tx, _welcomes) = mpsc::unbounded_channel();
    tokio::spawn(fake_broker(
        listener,
        events_tx,
        welcomes_tx,
        Arc::new(Notify::new()),
    ));
    let mut options = Options::structured(dir.path().join("state"));
    options.credentials_file = None;
    let scheduler = Scheduler::start(options);
    let report = scheduler
        .apply(document(dir.path(), port), "d1".into())
        .await
        .unwrap();
    assert!(report.contains("Total Errors:   0"), "{report}");

    // The HARD problem is notified, then again after notification_interval.
    let notified = wait_lines(&dir.path().join("notified"), |lines| lines.len() >= 2).await;
    assert_eq!(
        notified[0],
        "PROBLEM 1 admin admin@example.test crit CRITICAL broken"
    );
    assert!(notified[1].starts_with("PROBLEM 2 admin"), "{notified:?}");
    // The event handler ran at the SOFT and at the HARD state.
    let handled = wait_lines(&dir.path().join("handled"), |lines| lines.len() >= 2).await;
    assert_eq!(
        handled[..2],
        ["on-crit CRITICAL SOFT 1", "on-crit CRITICAL HARD 2"]
    );

    let mut log_lines = Vec::new();
    let mut statuses = Vec::new();
    let mut forced = false;
    let deadline = tokio::time::Instant::now() + Duration::from_secs(20);
    while (log_lines.is_empty() || !forced) && tokio::time::Instant::now() < deadline {
        let Ok(Some((kind, payload))) =
            tokio::time::timeout(Duration::from_secs(1), events.recv()).await
        else {
            continue;
        };
        match kind {
            event::LOG_ENTRY => {
                let entry = LogEntry::decode(payload.as_slice()).unwrap();
                // SERVICE_NOTIFICATION, contact and command parsed by cbmod's rules.
                if entry.msg_type == 2 {
                    log_lines.push(entry);
                }
            }
            event::SERVICE_STATUS => {
                let status = ServiceStatus::decode(payload.as_slice()).unwrap();
                // A stale passive service gets an active check.
                if status.service_id == 2 && status.output == "forced" && status.check_type == 0 {
                    forced = true;
                }
                statuses.push(status);
            }
            _ => {}
        }
    }
    let entry = &log_lines[0];
    assert_eq!(
        (
            entry.notification_contact.as_str(),
            entry.notification_cmd.as_str(),
            entry.status,
            entry.host_id,
            entry.service_id
        ),
        ("admin", "notify-mail", 2, 10, 1)
    );
    assert!(forced, "the stale passive service was checked");
    assert!(
        statuses
            .iter()
            .any(|s| s.service_id == 1 && s.notification_number >= 1 && s.last_notification > 0)
    );

    // Switched off by the web: the adaptive event, no more notification.
    scheduler
        .external_command("DISABLE_SVC_NOTIFICATIONS;h1;crit")
        .await
        .unwrap();
    let deadline = tokio::time::Instant::now() + Duration::from_secs(10);
    let mut adaptive = None;
    let mut notifications_flag = None;
    while (adaptive.is_none() || notifications_flag.is_none())
        && tokio::time::Instant::now() < deadline
    {
        let Ok(Some((kind, payload))) =
            tokio::time::timeout(Duration::from_secs(1), events.recv()).await
        else {
            continue;
        };
        if kind == event::ADAPTIVE_SERVICE {
            adaptive = Some(AdaptiveService::decode(payload.as_slice()).unwrap());
        }
        if kind == event::INSTANCE_STATUS {
            notifications_flag = Some(
                InstanceStatus::decode(payload.as_slice())
                    .unwrap()
                    .notifications,
            );
        }
    }
    let adaptive = adaptive.expect("pb_adaptive_service");
    assert_eq!((adaptive.service_id, adaptive.notify), (1, Some(false)));
    assert_eq!(notifications_flag, Some(true));
    let before = std::fs::read_to_string(dir.path().join("notified")).unwrap();
    tokio::time::sleep(Duration::from_secs(4)).await;
    let after = std::fs::read_to_string(dir.path().join("notified")).unwrap();
    assert_eq!(before, after, "no notification while disabled");
    scheduler.shutdown().await;
}
