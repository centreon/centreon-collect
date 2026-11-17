//! Notifications, event handlers, flapping and freshness of the core,
//! with a paused clock (the commands run are not waited for).

use std::path::Path;
use std::time::Duration;

use gorgone_proto::bbdo::{AdaptiveService, Comment, LogEntry, ServiceStatus};
use prost::Message;
use tokio::sync::mpsc;

use super::super::tests::core;
use super::*;
use crate::bbdo::{Decoder, event};
use crate::client::Outgoing;

fn write(dir: &Path) -> std::path::PathBuf {
    let w = |name: &str, text: &str| std::fs::write(dir.join(name), text).unwrap();
    w(
        "centengine.cfg",
        "cfg_file=objects.cfg\ninterval_length=60\nlog_level_notifications=info\n\
         enable_flap_detection=1\nlog_level_runtime=warning\n\
         service_freshness_check_interval=10\n",
    );
    w(
        "objects.cfg",
        "define command {\n command_name ok\n command_line /bin/true\n}\n\
         define command {\n command_name notify\n command_line /bin/true $CONTACTNAME$\n}\n\
         define command {\n command_name handler\n command_line /bin/true $SERVICESTATE$\n}\n\
         define contact {\n contact_name admin\n email admin@localhost\n \
         service_notification_options w,c,r,f,s\n host_notification_options d,r\n \
         service_notification_commands notify\n host_notification_commands notify\n}\n\
         define contact {\n contact_name other\n service_notification_options c\n \
         service_notification_commands notify\n}\n\
         define contactgroup {\n contactgroup_name ops\n members other\n}\n\
         define host {\n host_name h\n _HOST_ID 1\n contacts admin\n}\n\
         define service {\n host_name h\n service_description s\n check_command ok\n \
         max_check_attempts 2\n notification_interval 1\n contacts admin\n \
         contact_groups ops\n event_handler handler\n _SERVICE_ID 1\n}\n\
         define service {\n host_name h\n service_description e\n check_command ok\n \
         max_check_attempts 1\n notification_interval 1\n contacts admin\n _SERVICE_ID 2\n}\n\
         define serviceescalation {\n host_name h\n service_description e\n \
         first_notification 2\n last_notification 0\n notification_interval 2\n \
         escalation_options c\n contact_groups ops\n}\n\
         define service {\n host_name h\n service_description f\n check_command ok\n \
         max_check_attempts 1\n contacts admin\n _SERVICE_ID 3\n}\n\
         define service {\n host_name h\n service_description p\n check_command ok\n \
         active_checks_enabled 0\n check_freshness 1\n freshness_threshold 30\n \
         _SERVICE_ID 4\n}\n",
    );
    w(
        "module.json",
        r#"{"centreonBroker":{"poller_id":7,"poller_name":"p","broker_name":"p-module",
           "output":[{"type":"ipv4","host":"127.0.0.1","port":"1"}]}}"#,
    );
    dir.join("centengine.cfg")
}

fn sent(out: &mut mpsc::Receiver<Outgoing>) -> Vec<(u32, Vec<u8>)> {
    let mut decoder = Decoder::default();
    while let Ok(outgoing) = out.try_recv() {
        if let Outgoing::Event(frame) = outgoing {
            decoder.push(&frame);
        }
    }
    std::iter::from_fn(|| decoder.next_event()).collect()
}

/// The event log lines written since the last call, but the external
/// commands and passive checks; each one was sent as a log entry.
fn lines(core: &mut Core, events: &[(u32, Vec<u8>)]) -> Vec<String> {
    let entries = events
        .iter()
        .filter(|(kind, _)| *kind == event::LOG_ENTRY)
        .inspect(|(_, payload)| {
            LogEntry::decode(payload.as_slice()).unwrap();
        })
        .count();
    let logged = std::mem::take(&mut core.logged);
    assert_eq!(entries, logged.len());
    logged
        .into_iter()
        .filter(|line| !line.starts_with("EXTERNAL COMMAND") && !line.starts_with("PASSIVE"))
        .collect()
}

fn notifications(core: &mut Core, events: &[(u32, Vec<u8>)]) -> Vec<String> {
    lines(core, events)
        .into_iter()
        .filter(|l| l.contains("NOTIFICATION:"))
        .collect()
}

fn last_status(events: &[(u32, Vec<u8>)], service_id: u64) -> ServiceStatus {
    events
        .iter()
        .filter(|(kind, _)| *kind == event::SERVICE_STATUS)
        .map(|(_, p)| ServiceStatus::decode(p.as_slice()).unwrap())
        .rfind(|s| s.service_id == service_id)
        .unwrap()
}

fn loaded(dir: &Path) -> (Core, mpsc::Receiver<Outgoing>) {
    let main = write(dir);
    let (mut core, mut out, _target) = core(dir, main);
    core.load().unwrap();
    core.ever_connected = true;
    sent(&mut out);
    core.logged.clear();
    (core, out)
}

#[tokio::test(start_paused = true)]
async fn problems_renotifications_acknowledgement_and_recovery() {
    let dir = tempfile::tempdir().unwrap();
    let (mut core, mut out) = loaded(dir.path());
    // SOFT: the event handler runs, nobody is notified.
    core.command("PROCESS_SERVICE_CHECK_RESULT;h;s;2;down")
        .unwrap();
    assert_eq!(
        lines(&mut core, &sent(&mut out)),
        ["SERVICE EVENT HANDLER: h;s;CRITICAL;SOFT;1;handler"]
    );
    // HARD: the contact and the member of the contact group.
    core.command("PROCESS_SERVICE_CHECK_RESULT;h;s;2;down")
        .unwrap();
    let events = sent(&mut out);
    assert_eq!(
        notifications(&mut core, &events),
        [
            "SERVICE NOTIFICATION: admin;h;s;CRITICAL;notify;down;",
            "SERVICE NOTIFICATION: other;h;s;CRITICAL;notify;down;"
        ]
    );
    let status = last_status(&events, 1);
    assert_eq!(status.notification_number, 1);
    assert!(status.last_notification > 0);
    // Not again before notification_interval (1 minute).
    core.command("PROCESS_SERVICE_CHECK_RESULT;h;s;2;down")
        .unwrap();
    assert!(notifications(&mut core, &sent(&mut out)).is_empty());
    tokio::time::advance(Duration::from_secs(61)).await;
    core.command("PROCESS_SERVICE_CHECK_RESULT;h;s;2;down")
        .unwrap();
    let events = sent(&mut out);
    assert_eq!(notifications(&mut core, &events).len(), 2);
    assert_eq!(last_status(&events, 1).notification_number, 2);
    // Acknowledged with notify: its own notification, then no more.
    core.command("ACKNOWLEDGE_SVC_PROBLEM;h;s;2;1;0;admin;on it")
        .unwrap();
    assert_eq!(
        notifications(&mut core, &sent(&mut out))[0],
        "SERVICE NOTIFICATION: admin;h;s;ACKNOWLEDGEMENT (CRITICAL);notify;down;;admin;on it"
    );
    tokio::time::advance(Duration::from_secs(61)).await;
    core.command("PROCESS_SERVICE_CHECK_RESULT;h;s;2;down")
        .unwrap();
    assert!(notifications(&mut core, &sent(&mut out)).is_empty());
    // Recovery: to those notified of the problem who take recoveries.
    core.command("PROCESS_SERVICE_CHECK_RESULT;h;s;0;fine")
        .unwrap();
    let events = sent(&mut out);
    let logged = lines(&mut core, &events);
    assert_eq!(
        logged,
        [
            "SERVICE ALERT: h;s;OK;HARD;1;fine",
            "SERVICE NOTIFICATION: admin;h;s;RECOVERY (OK);notify;fine;",
            "SERVICE EVENT HANDLER: h;s;OK;HARD;1;handler"
        ]
    );
    assert_eq!(last_status(&events, 1).notification_number, 0);
}

#[tokio::test(start_paused = true)]
async fn downtimes_escalations_and_switches() {
    let dir = tempfile::tempdir().unwrap();
    let (mut core, mut out) = loaded(dir.path());
    // A downtime: its start is notified, the problems during it are not.
    core.command("SCHEDULE_SVC_DOWNTIME;h;s;0;4000000000;1;0;0;admin;maintenance")
        .unwrap();
    core.run_timers();
    assert_eq!(
        notifications(&mut core, &sent(&mut out)),
        ["SERVICE NOTIFICATION: admin;h;s;DOWNTIMESTART (OK);notify;;"]
    );
    for _ in 0..2 {
        core.command("PROCESS_SERVICE_CHECK_RESULT;h;s;2;down")
            .unwrap();
    }
    assert!(notifications(&mut core, &sent(&mut out)).is_empty());
    // Escalated from the second notification: the group only.
    core.command("PROCESS_SERVICE_CHECK_RESULT;h;e;2;down")
        .unwrap();
    assert_eq!(
        notifications(&mut core, &sent(&mut out)),
        ["SERVICE NOTIFICATION: admin;h;e;CRITICAL;notify;down;"]
    );
    tokio::time::advance(Duration::from_secs(61)).await;
    core.command("PROCESS_SERVICE_CHECK_RESULT;h;e;2;down")
        .unwrap();
    assert_eq!(
        notifications(&mut core, &sent(&mut out)),
        ["SERVICE NOTIFICATION: other;h;e;CRITICAL;notify;down;"]
    );
    let e = core.by_key[&(1, 2)];
    let normal = &core.objs[e].state.notification.sent[&0];
    assert!(normal.escalated && normal.interval == 2.0);
    // Its interval (2 minutes) now plans the next one.
    tokio::time::advance(Duration::from_secs(61)).await;
    core.command("PROCESS_SERVICE_CHECK_RESULT;h;e;2;down")
        .unwrap();
    assert!(notifications(&mut core, &sent(&mut out)).is_empty());
    // Disabled on the service: an adaptive event, no notification.
    core.command("[1] DISABLE_SVC_NOTIFICATIONS;h;e;1").unwrap();
    let events = sent(&mut out);
    let adaptive = events
        .iter()
        .find(|(kind, _)| *kind == event::ADAPTIVE_SERVICE)
        .map(|(_, p)| AdaptiveService::decode(p.as_slice()).unwrap())
        .unwrap();
    assert_eq!((adaptive.service_id, adaptive.notify), (2, Some(false)));
    tokio::time::advance(Duration::from_secs(200)).await;
    core.command("PROCESS_SERVICE_CHECK_RESULT;h;e;2;down")
        .unwrap();
    assert!(notifications(&mut core, &sent(&mut out)).is_empty());
    // Globally disabled: reported by the instance status.
    core.command("DISABLE_NOTIFICATIONS").unwrap();
    assert!(!core.program_status().notifications);
    core.command("ENABLE_SVC_NOTIFICATIONS;h;e").unwrap();
    tokio::time::advance(Duration::from_secs(200)).await;
    core.command("PROCESS_SERVICE_CHECK_RESULT;h;e;2;down")
        .unwrap();
    assert!(notifications(&mut core, &sent(&mut out)).is_empty());
    core.command("ENABLE_NOTIFICATIONS").unwrap();
    tokio::time::advance(Duration::from_secs(200)).await;
    core.command("PROCESS_SERVICE_CHECK_RESULT;h;e;2;down")
        .unwrap();
    assert_eq!(notifications(&mut core, &sent(&mut out)).len(), 1);
}

#[tokio::test(start_paused = true)]
async fn flapping_starts_suppresses_and_stops() {
    let dir = tempfile::tempdir().unwrap();
    let (mut core, mut out) = loaded(dir.path());
    let mut all = Vec::new();
    let mut logged = Vec::new();
    for i in 0..12 {
        let code = if i % 2 == 0 { 2 } else { 0 };
        core.command(&format!("PROCESS_SERVICE_CHECK_RESULT;h;f;{code};r{i}"))
            .unwrap();
        let events = sent(&mut out);
        logged.extend(lines(&mut core, &events));
        all.extend(events);
    }
    let f = core.by_key[&(1, 3)];
    assert!(core.objs[f].state.flap.is_flapping);
    let started: Vec<String> = logged
        .iter()
        .filter(|&l| l.contains("FLAPPING ALERT"))
        .cloned()
        .collect();
    assert_eq!(started.len(), 1, "{started:?}");
    assert!(
        started[0].starts_with(
            "SERVICE FLAPPING ALERT: h;f;STARTED; Service appears to have started flapping ("
        ),
        "{started:?}"
    );
    let comment = all
        .iter()
        .filter(|(kind, _)| *kind == event::COMMENT)
        .map(|(_, p)| Comment::decode(p.as_slice()).unwrap())
        .find(|c| c.entry_type == 3)
        .expect("a flapping comment");
    assert_eq!(comment.author, "(Centreon Engine Process)");
    assert!(last_status(&all, 3).flapping);
    // The start is notified, the problems while it flaps are not.
    let notified: Vec<&String> = logged
        .iter()
        .filter(|l| l.contains("NOTIFICATION:"))
        .collect();
    let start = notified
        .iter()
        .position(|l| l.contains(";FLAPPINGSTART ("))
        .unwrap_or_else(|| panic!("{notified:?}"));
    assert_eq!(notified.len(), start + 1, "{notified:?}");
    // Stable: it stops, the comment goes.
    let mut all = Vec::new();
    let mut logged = Vec::new();
    for _ in 0..21 {
        core.command("PROCESS_SERVICE_CHECK_RESULT;h;f;0;ok")
            .unwrap();
        let events = sent(&mut out);
        logged.extend(lines(&mut core, &events));
        all.extend(events);
    }
    assert!(!core.objs[f].state.flap.is_flapping);
    assert!(logged.iter().any(|l| l.starts_with(
        "SERVICE FLAPPING ALERT: h;f;STOPPED; Service appears to have stopped flapping"
    )));
    assert!(logged.iter().any(|l| l.contains(";FLAPPINGSTOP (OK);")));
    assert!(
        all.iter()
            .filter(|(kind, _)| *kind == event::COMMENT)
            .map(|(_, p)| Comment::decode(p.as_slice()).unwrap())
            .any(|c| c.internal_id == comment.internal_id && c.deletion_time > 0)
    );
}

#[tokio::test(start_paused = true)]
async fn stale_passive_results_force_a_check() {
    let dir = tempfile::tempdir().unwrap();
    let (mut core, mut out) = loaded(dir.path());
    let p = core.by_key[&(1, 4)];
    // Passive only: never planned.
    assert!(core.objs[p].due.is_none());
    core.command("PROCESS_SERVICE_CHECK_RESULT;h;p;0;fresh")
        .unwrap();
    tokio::time::advance(Duration::from_secs(20)).await;
    core.run_timers();
    assert!(!core.objs[p].state.being_freshened);
    // Threshold 30 s: stale after it, and a check is forced.
    tokio::time::advance(Duration::from_secs(20)).await;
    core.run_timers();
    assert!(core.objs[p].state.being_freshened);
    let stale: Vec<String> = lines(&mut core, &sent(&mut out))
        .into_iter()
        .filter(|l| l.starts_with("Warning: The results of service 'p' on host 'h' are stale by"))
        .collect();
    assert_eq!(stale.len(), 1, "{stale:?}");
    assert!(
        stale[0]
            .ends_with("(threshold=0d 0h 0m 30s).  I'm forcing an immediate check of the service."),
        "{stale:?}"
    );
    // Not twice while it runs.
    tokio::time::advance(Duration::from_secs(10)).await;
    core.run_timers();
    assert!(lines(&mut core, &sent(&mut out)).is_empty());
}

#[test]
fn date_macros_as_engine() {
    let tz = crate::tz::Tz::posix("CET-1CEST,M3.5.0,M10.5.0/3").unwrap();
    let mut m = HashMap::new();
    // 2026-11-17 14:03:05 CET.
    let t = 1_794_920_585;
    date_macros(&tz, &tz.abbreviation(t), DateFormat::Us, t, &mut m);
    assert_eq!(m["LONGDATETIME"], "Tue Nov 17 14:03:05 CET 2026");
    assert_eq!(m["SHORTDATETIME"], "11-17-2026 14:03:05");
    assert_eq!(m["DATE"], "11-17-2026");
    date_macros(&tz, "CET", DateFormat::StrictIso8601, t, &mut m);
    assert_eq!(m["SHORTDATETIME"], "2026-11-17T14:03:05");
}
