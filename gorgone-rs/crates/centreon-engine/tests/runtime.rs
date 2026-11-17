//! Downtimes, acknowledgements, comments, recurring downtimes, check
//! periods, `encrypt::` macros and retention, against a fake central
//! broker, with a version 2 structured configuration.

mod common;

use std::os::unix::fs::PermissionsExt;
use std::path::Path;
use std::sync::Arc;
use std::time::Duration;

use centreon_engine::wire::event;
use centreon_engine::{Options, Scheduler};
use common::fake_broker;
use gorgone_proto::bbdo::{
    Acknowledgement, Comment, Downtime, HostStatus, Instance, Service, ServiceStatus,
};
use prost::Message;
use serde_json::json;
use tokio::net::TcpListener;
use tokio::sync::{Notify, mpsc};

/// The key of the bench (public on purpose) and "s3cr3t-value" encrypted
/// with it by centreon-web.
const KEY: &str = r#"{"app_secret":"YmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmI=","salt":"c3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nz"}"#;
const ENCRYPTED: &str = include_str!("data/encrypted-by-web.txt");

fn now() -> i64 {
    std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs() as i64
}

fn write_plugins(dir: &Path) {
    for (name, body) in [
        ("plugin.sh", "#!/bin/sh\necho \"$2 | value=$1\"\nexit $1\n"),
        // Proves the secret was decrypted without printing it.
        ("secret.sh", "#!/bin/sh\necho \"lengths ${#1} ${#2}\"\n"),
    ] {
        let path = dir.join(name);
        std::fs::write(&path, body).unwrap();
        std::fs::set_permissions(&path, std::fs::Permissions::from_mode(0o755)).unwrap();
    }
}

/// A recurring period starting in two minutes (UTC), or none near midnight.
fn soon_period() -> Option<String> {
    let minute = now() / 60 % 1440 + 2;
    let end = minute + 30;
    (end < 1440).then(|| {
        format!(
            "weekly:1,2,3,4,5,6,7 {:02}:{:02}-{:02}:{:02} fixed",
            minute / 60,
            minute % 60,
            end / 60,
            end % 60
        )
    })
}

fn document(dir: &Path, port: u16, period: Option<&str>) -> Vec<u8> {
    let secret = format!("encrypt::{}", ENCRYPTED.trim());
    // One hour a day, twelve hours from now (UTC): an empty timeperiod
    // would be valid all the time, as in Engine.
    let hour = (now() / 3600 + 12) % 24;
    let later = format!("{hour:02}:00-{:02}:00", hour + 1);
    let mut doc = json!({
        "version": 2,
        "poller": {"id": 5, "name": "rust"},
        "broker": {"address": format!("127.0.0.1:{port}"), "name": "rust-module"},
        "settings": {"interval_length": 1, "status_update_interval": 1,
                     "retention_update_interval": 1, "use_timezone": "UTC0"},
        "resource_macros": {"USER1": dir.display().to_string(), "USER2": secret},
        "commands": [
            {"command_name": "dummy", "command_line": "$USER1$/plugin.sh $ARG1$ \"$ARG2$\""},
            {"command_name": "secret", "command_line": "$USER1$/secret.sh $_HOSTSECRET$ $USER2$"}
        ],
        "timeperiods": [
            {"timeperiod_name": "later", "alias": "Later", "sunday": later.clone(),
             "monday": later.clone(), "tuesday": later.clone(), "wednesday": later.clone(),
             "thursday": later.clone(), "friday": later.clone(), "saturday": later},
            {"timeperiod_name": "always", "alias": "Always", "sunday": "00:00-24:00",
             "monday": "00:00-24:00", "tuesday": "00:00-24:00", "wednesday": "00:00-24:00",
             "thursday": "00:00-24:00", "friday": "00:00-24:00", "saturday": "00:00-24:00"}
        ],
        "hosts": [{"host_name": "h1", "address": "127.0.0.1", "host_id": 10,
                   "check_command": "dummy!0!up", "check_interval": 1,
                   "check_period": "always", "macros": {"SECRET": secret}}],
        "service_templates": [{"name": "stpl", "check_interval": 1, "retry_interval": 1,
                               "max_check_attempts": 2}],
        "services": [
            {"host_name": "h1", "service_description": "ok", "use": ["stpl"],
             "check_command": "dummy!0!fine", "service_id": 1},
            {"host_name": "h1", "service_description": "crit", "use": ["stpl"],
             "check_command": "dummy!2!broken", "service_id": 2},
            {"host_name": "h1", "service_description": "secret", "use": ["stpl"],
             "check_command": "secret", "service_id": 3},
            {"host_name": "h1", "service_description": "off", "use": ["stpl"],
             "check_command": "dummy!0!never", "check_period": "later", "service_id": 4}
        ]
    });
    if let Some(period) = period {
        doc["recurring_downtimes"] = json!([{"downtime_id": 9, "service_ids": [[10, 1]],
                                             "timezone": "UTC0", "periods": [period]}]);
    }
    serde_json::to_vec(&doc).unwrap()
}

/// The events received, searchable more than once.
struct Seen {
    events: mpsc::UnboundedReceiver<(u32, Vec<u8>)>,
    all: Vec<(u32, Vec<u8>)>,
}

impl Seen {
    /// The first event of type `kind`, received since `from`, matching.
    async fn find<M: Message + Default>(
        &mut self,
        kind: u32,
        from: usize,
        mut matches: impl FnMut(&M) -> bool,
    ) -> (usize, M) {
        let deadline = tokio::time::Instant::now() + Duration::from_secs(20);
        let mut at = from;
        loop {
            while at < self.all.len() {
                let (event_type, payload) = &self.all[at];
                at += 1;
                if *event_type == kind {
                    let message = M::decode(payload.as_slice()).unwrap();
                    if matches(&message) {
                        return (at, message);
                    }
                }
            }
            let next = tokio::time::timeout_at(deadline, self.events.recv())
                .await
                .unwrap_or_else(|_| panic!("event {kind:#x} not received in time"))
                .unwrap();
            self.all.push(next);
        }
    }
}

fn options(dir: &Path) -> Options {
    let mut options = Options::structured(dir.join("state"));
    options.retention_file = Some(dir.join("retention.json"));
    options.credentials_file = Some(dir.join("engine-context.json"));
    options
}

#[tokio::test(flavor = "multi_thread", worker_threads = 2)]
async fn downtimes_acknowledgements_comments_secrets_and_retention() {
    let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
    let port = listener.local_addr().unwrap().port();
    let dir = tempfile::tempdir().unwrap();
    write_plugins(dir.path());
    std::fs::write(dir.path().join("engine-context.json"), KEY).unwrap();
    let (events_tx, events) = mpsc::unbounded_channel();
    let (welcomes_tx, mut welcomes) = mpsc::unbounded_channel();
    tokio::spawn(fake_broker(
        listener,
        events_tx,
        welcomes_tx,
        Arc::new(Notify::new()),
    ));
    let mut seen = Seen {
        events,
        all: Vec::new(),
    };
    let period = soon_period();

    let scheduler = Scheduler::start(options(dir.path()));
    let report = scheduler
        .apply(document(dir.path(), port, period.as_deref()), "d1".into())
        .await
        .unwrap();
    assert!(report.contains("Total Errors:   0"), "{report}");
    welcomes.recv().await.unwrap();
    let (_, instance) = seen
        .find::<Instance>(event::INSTANCE, 0, |i| i.running)
        .await;
    assert!(instance.is_encryption_ready, "the engine has the key");

    // The secret reached the plugin decrypted: 12 characters, not the
    // base64 text.
    seen.find::<ServiceStatus>(event::SERVICE_STATUS, 0, |s| {
        s.service_id == 3 && s.checked && s.output == "lengths 12 12"
    })
    .await;
    let (_, hard) = seen
        .find::<ServiceStatus>(event::SERVICE_STATUS, 0, |s| {
            s.service_id == 2 && s.checked && s.state_type == 1
        })
        .await;
    assert_eq!(hard.check_attempt, 2);

    // Sticky acknowledgement: the event, the status, the comment.
    let mark = seen.all.len();
    scheduler
        .external_command("[1] ACKNOWLEDGE_SVC_PROBLEM;h1;crit;2;0;1;admin;on it")
        .await
        .unwrap();
    let (_, ack) = seen
        .find::<Acknowledgement>(event::ACKNOWLEDGEMENT, mark, |_| true)
        .await;
    assert_eq!(
        (ack.service_id, ack.sticky, ack.state, ack.deletion_time),
        (2, true, 2, 0)
    );
    seen.find::<ServiceStatus>(event::SERVICE_STATUS, mark, |s| {
        s.service_id == 2 && s.acknowledgement_type == 2
    })
    .await;
    seen.find::<Comment>(event::COMMENT, mark, |c| {
        c.entry_type == 4 && c.data == "on it"
    })
    .await;
    // It survives the next results: sticky, same problem.
    let mark = seen.all.len();
    seen.find::<ServiceStatus>(event::SERVICE_STATUS, mark, |s| {
        s.service_id == 2 && s.acknowledgement_type == 2
    })
    .await;

    // A comment, then its deletion.
    let mark = seen.all.len();
    scheduler
        .external_command(&format!(
            "[{}] ADD_HOST_COMMENT;h1;1;admin;hello; world",
            now()
        ))
        .await
        .unwrap();
    let (_, comment) = seen
        .find::<Comment>(event::COMMENT, mark, |c| c.data == "hello; world")
        .await;
    assert!(comment.persistent && comment.service_id == 0 && comment.deletion_time == 0);
    scheduler
        .external_command(&format!("DEL_HOST_COMMENT;{}", comment.internal_id))
        .await
        .unwrap();
    seen.find::<Comment>(event::COMMENT, mark, |c| {
        c.internal_id == comment.internal_id && c.deletion_time > 0
    })
    .await;

    // A fixed host downtime from now: added, started, the depth is 1.
    let mark = seen.all.len();
    let start = now();
    scheduler
        .external_command(&format!(
            "[{start}] SCHEDULE_HOST_DOWNTIME;h1;{start};{};1;0;0;admin;maintenance",
            start + 3600
        ))
        .await
        .unwrap();
    let (_, added) = seen
        .find::<Downtime>(event::DOWNTIME, mark, |d| d.comment_data == "maintenance")
        .await;
    assert!(!added.started && added.fixed && added.r#type == 2);
    assert_eq!(added.actual_start_time, u64::MAX);
    seen.find::<Downtime>(event::DOWNTIME, mark, |d| d.id == added.id && d.started)
        .await;
    seen.find::<HostStatus>(event::HOST_STATUS, mark, |h| {
        h.scheduled_downtime_depth == 1
    })
    .await;
    seen.find::<Comment>(event::COMMENT, mark, |c| {
        c.entry_type == 2 && c.data.contains("fixed downtime")
    })
    .await;

    // The recurring downtime of the service, scheduled by the engine.
    if period.is_some() {
        let (_, cycle) = seen
            .find::<Downtime>(event::DOWNTIME, 0, |d| {
                d.comment_data == "[Downtime cycle #9]"
            })
            .await;
        assert_eq!((cycle.host_id, cycle.service_id, cycle.r#type), (10, 1, 1));
        assert_eq!(cycle.author, "Downtime cycle");
    }

    // Outside its check period: not checked, planned in it, hours later.
    let hours_later = now() + 6 * 3600;
    let (_, off) = seen
        .find::<ServiceStatus>(event::SERVICE_STATUS, 0, |s| {
            s.service_id == 4 && s.next_check > hours_later
        })
        .await;
    assert!(!off.checked);

    // Clean stop: the retention file is written, with no clear secret.
    scheduler.shutdown().await;
    let retention = std::fs::read_to_string(dir.path().join("retention.json")).unwrap();
    assert!(!retention.contains("s3cr3t"));
    assert!(retention.contains("maintenance"));

    // Restart: states, acknowledgement and downtime come back.
    let mark = seen.all.len();
    let scheduler = Scheduler::start(options(dir.path()));
    welcomes.recv().await.unwrap();
    let (_, service) = seen
        .find::<Service>(event::SERVICE, mark, |s| s.service_id == 2)
        .await;
    assert!(service.checked && service.acknowledged);
    assert_eq!(
        (service.state, service.state_type, service.check_attempt),
        (2, 1, 2)
    );
    let (_, kept) = seen
        .find::<Downtime>(event::DOWNTIME, mark, |d| d.id == added.id)
        .await;
    assert_eq!(kept.comment_data, "maintenance");
    seen.find::<HostStatus>(event::HOST_STATUS, mark, |h| {
        h.scheduled_downtime_depth == 1
    })
    .await;

    // Cancelled: stopped and deleted, depth back to 0.
    let mark = seen.all.len();
    scheduler
        .external_command(&format!("DEL_HOST_DOWNTIME;{}", added.id))
        .await
        .unwrap();
    seen.find::<Downtime>(event::DOWNTIME, mark, |d| {
        d.id == added.id && d.cancelled && d.deletion_time != u64::MAX
    })
    .await;
    seen.find::<HostStatus>(event::HOST_STATUS, mark, |h| {
        h.scheduled_downtime_depth == 0
    })
    .await;
    // Acknowledgement removed: its deletion is sent.
    scheduler
        .external_command("REMOVE_SVC_ACKNOWLEDGEMENT;h1;crit")
        .await
        .unwrap();
    seen.find::<Acknowledgement>(event::ACKNOWLEDGEMENT, mark, |a| {
        a.service_id == 2 && a.deletion_time > 0
    })
    .await;
    scheduler.shutdown().await;
}
