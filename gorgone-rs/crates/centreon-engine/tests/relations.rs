//! Groups, severities, tags, parents, dependencies and the event log
//! against a fake central broker: the events at the dump and on a reload,
//! a child host UNREACHABLE behind its DOWN parent, a dependent service not
//! checked while its master fails, and the log entries Engine sends.

mod common;

use std::os::unix::fs::PermissionsExt;
use std::path::Path;
use std::sync::Arc;
use std::time::Duration;

use centreon_engine::wire::event;
use centreon_engine::{Options, Scheduler};
use common::{fake_broker, wait_for};
use gorgone_proto::bbdo::{
    Host, HostGroupMember, HostParent, HostStatus, LogEntry, ServiceGroup, ServiceStatus, Severity,
    Tag,
};
use prost::Message;
use serde_json::json;
use tokio::net::TcpListener;
use tokio::sync::{Notify, mpsc};

fn document(dir: &Path, port: u16, members: &[&str]) -> Vec<u8> {
    serde_json::to_vec(&json!({
        "version": 3,
        "poller": {"id": 5, "name": "rust"},
        "broker": {"address": format!("127.0.0.1:{port}"), "name": "rust-module"},
        "settings": {"interval_length": 1, "status_update_interval": 1, "log_service_retries": true,
                     "log_host_retries": true, "log_level_external_command": "info"},
        "resource_macros": {"USER1": dir.display().to_string()},
        "commands": [{"command_name": "dummy", "command_line": "$USER1$/plugin.sh $ARG1$ \"$ARG2$\""}],
        "hosts": [
            {"host_name": "parent", "host_id": 10, "address": "127.0.0.1", "check_command": "dummy!2!gone",
             "check_interval": 1, "max_check_attempts": 1},
            {"host_name": "child", "host_id": 11, "address": "127.0.0.1", "check_command": "dummy!2!no route",
             "check_interval": 1, "max_check_attempts": 1, "parents": ["parent"], "severity_id": 3,
             "category_tags": [8], "group_tags": [20]},
            {"host_name": "other", "host_id": 12, "address": "127.0.0.1"}
        ],
        "services": [
            {"host_name": "other", "service_description": "master", "service_id": 1,
             "check_command": "dummy!2!master down", "check_interval": 1, "max_check_attempts": 1},
            {"host_name": "other", "service_description": "dependent", "service_id": 2,
             "check_command": "dummy!0!fine", "check_interval": 1, "max_check_attempts": 1,
             "servicegroups": ["sg"]}
        ],
        "hostgroups": [{"hostgroup_id": 20, "hostgroup_name": "hg", "alias": "Group", "members": members}],
        "servicegroups": [{"servicegroup_id": 30, "servicegroup_name": "sg", "alias": "Services",
                           "members": [["other", "master"]]}],
        "severities": [{"id": 3, "severity_name": "high", "level": 1, "icon_id": 2, "type": "host"}],
        "tags": [{"id": 8, "tag_name": "cat", "type": "hostcategory"},
                 {"id": 20, "tag_name": "hg", "type": "hostgroup"}],
        "service_dependencies": [{"host_name": ["other"], "service_description": ["master"],
                                  "dependent_host_name": ["other"], "dependent_service_description": ["dependent"],
                                  "execution_failure_criteria": "c,u", "notification_failure_criteria": "c"}]
    }))
    .unwrap()
}

#[tokio::test(flavor = "multi_thread", worker_threads = 2)]
async fn relations_unreachable_dependencies_and_event_log() {
    let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
    let port = listener.local_addr().unwrap().port();
    let dir = tempfile::tempdir().unwrap();
    let plugin = dir.path().join("plugin.sh");
    std::fs::write(&plugin, "#!/bin/sh\necho \"$2 | value=$1\"\nexit $1\n").unwrap();
    std::fs::set_permissions(&plugin, std::fs::Permissions::from_mode(0o755)).unwrap();
    let (events_tx, mut events) = mpsc::unbounded_channel();
    let (welcomes_tx, _welcomes) = mpsc::unbounded_channel();
    tokio::spawn(fake_broker(
        listener,
        events_tx,
        welcomes_tx,
        Arc::new(Notify::new()),
    ));
    let scheduler = Scheduler::start(Options::structured(dir.path().join("state")));
    let summary = scheduler
        .apply(document(dir.path(), port, &["child"]), "d1".into())
        .await
        .unwrap();
    assert!(summary.contains("Total Errors:   0"), "{summary}");

    // The dump: severities and tags, hosts and services, parents and
    // groups, the end of the configuration, then the initial states.
    let mut dump = Vec::new();
    wait_for(&mut events, Duration::from_secs(10), |kind, payload| {
        dump.push((kind, payload.to_vec()));
        (kind == event::INSTANCE_CONFIGURATION).then_some(())
    })
    .await;
    let kinds: Vec<u32> = dump.iter().map(|(kind, _)| *kind).collect();
    assert_eq!(
        kinds,
        [
            event::INSTANCE,
            event::SEVERITY,
            event::TAG,
            event::TAG,
            event::HOST,
            event::HOST,
            event::HOST,
            event::SERVICE,
            event::SERVICE,
            event::HOST_PARENT,
            event::HOST_GROUP,
            event::HOST_GROUP_MEMBER,
            event::SERVICE_GROUP,
            event::SERVICE_GROUP_MEMBER,
            event::SERVICE_GROUP_MEMBER,
            event::INSTANCE_CONFIGURATION
        ]
    );
    let severity = Severity::decode(dump[1].1.as_slice()).unwrap();
    assert_eq!(
        (severity.id, severity.r#type, severity.poller_id),
        (3, 1, 5)
    );
    let tag = Tag::decode(dump[2].1.as_slice()).unwrap();
    assert_eq!((tag.id, tag.name.as_str()), (8, "cat"));
    let child = Host::decode(dump[5].1.as_slice()).unwrap();
    assert_eq!(
        (child.host_id, child.severity_id, child.tags.len()),
        (11, 3, 2)
    );
    let parent = HostParent::decode(dump[9].1.as_slice()).unwrap();
    assert_eq!(
        (parent.child_id, parent.parent_id, parent.enabled),
        (11, 10, true)
    );
    let group = ServiceGroup::decode(dump[12].1.as_slice()).unwrap();
    assert_eq!((group.servicegroup_id, group.name.as_str()), (30, "sg"));

    // The initial states, in Engine's format, then the alerts.
    let mut initial = Vec::new();
    let mut unreachable = false;
    let mut alerts = Vec::new();
    let mut master_failed_at = None;
    let mut dependent_checks = Vec::new();
    let deadline = tokio::time::Instant::now() + Duration::from_secs(15);
    while tokio::time::Instant::now() < deadline {
        let Ok(Some((kind, payload))) = tokio::time::timeout_at(deadline, events.recv()).await
        else {
            break;
        };
        match kind {
            event::LOG_ENTRY => {
                let entry = LogEntry::decode(payload.as_slice()).unwrap();
                assert_eq!(entry.instance_name, "rust");
                if entry.msg_type == 8 || entry.msg_type == 9 {
                    initial.push((entry.msg_type, entry.host_id, entry.service_id));
                } else if entry.msg_type <= 1 {
                    alerts.push((
                        entry.msg_type,
                        entry.host_id,
                        entry.service_id,
                        entry.status,
                    ));
                }
            }
            event::HOST_STATUS => {
                let status = HostStatus::decode(payload.as_slice()).unwrap();
                if status.host_id == 11 && status.state == 2 && status.state_type == 1 {
                    unreachable = true;
                }
            }
            event::SERVICE_STATUS => {
                let status = ServiceStatus::decode(payload.as_slice()).unwrap();
                match status.service_id {
                    1 if status.state == 2 && master_failed_at.is_none() => {
                        master_failed_at = Some(status.last_check);
                    }
                    2 => dependent_checks.push(status.last_check),
                    _ => {}
                }
            }
            _ => {}
        }
        let settled = master_failed_at.is_some() && dependent_checks.len() >= 4;
        if unreachable && settled {
            break;
        }
    }
    initial.sort_unstable();
    assert_eq!(
        initial,
        [(8, 12, 1), (8, 12, 2), (9, 10, 0), (9, 11, 0), (9, 12, 0)]
    );
    assert!(unreachable, "the child never became UNREACHABLE");
    assert!(alerts.contains(&(1, 11, 0, 2)), "{alerts:?}");
    assert!(alerts.contains(&(0, 12, 1, 2)), "{alerts:?}");
    // Once its master failed, the dependent is not checked again: its
    // statuses keep the same last check (0 if it never ran).
    let failed_at = master_failed_at.expect("the master never failed");
    let after: Vec<i64> = dependent_checks
        .iter()
        .copied()
        .filter(|checked| *checked > failed_at + 1)
        .collect();
    assert!(
        after.is_empty(),
        "checked after its master failed: {dependent_checks:?}"
    );

    // Reload: a member joins the host group.
    scheduler
        .apply(
            document(dir.path(), port, &["child", "parent"]),
            "d2".into(),
        )
        .await
        .unwrap();
    let mut members = Vec::new();
    wait_for(&mut events, Duration::from_secs(10), |kind, payload| {
        if kind == event::HOST_GROUP_MEMBER {
            let member = HostGroupMember::decode(payload).unwrap();
            members.push((member.host_id, member.enabled));
        }
        assert_ne!(kind, event::INSTANCE, "a reload sends no pb_instance");
        (kind == event::INSTANCE_CONFIGURATION).then_some(())
    })
    .await;
    assert_eq!(members, [(11, false), (10, true), (11, true)]);
    scheduler.shutdown().await;
}
