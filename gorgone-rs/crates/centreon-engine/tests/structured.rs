//! The engine with a structured configuration against a fake central
//! broker: full dump at connection, incremental reload as Engine sends it,
//! refused document, restart from the kept document.

mod common;

use std::os::unix::fs::PermissionsExt;
use std::path::Path;
use std::sync::Arc;
use std::time::Duration;

use centreon_engine::wire::event;
use centreon_engine::{Options, Scheduler};
use common::{fake_broker, wait_for};
use gorgone_proto::bbdo::{Host, Service};
use prost::Message;
use serde_json::json;
use tokio::net::TcpListener;
use tokio::sync::{Notify, mpsc};

/// A document with services 1 (`ok`), 2 (`crit`) and the extra ones.
fn document(dir: &Path, port: u16, crit_output: &str, extra: &[(u64, &str)]) -> Vec<u8> {
    let mut services = vec![
        json!({"host_name": "h1", "service_description": "ok", "use": ["stpl"],
               "check_command": "dummy!0!fine", "service_id": 1}),
        json!({"host_name": "h1", "service_description": "crit", "use": ["stpl"],
               "check_command": format!("dummy!2!{crit_output}"), "service_id": 2}),
    ];
    for (id, name) in extra {
        services.push(
            json!({"host_name": "h1", "service_description": name, "use": ["stpl"],
                             "check_command": "dummy!1!warn", "service_id": id}),
        );
    }
    serde_json::to_vec(&json!({
        "version": 1,
        "poller": {"id": 5, "name": "rust"},
        "broker": {"address": format!("127.0.0.1:{port}"), "name": "rust-module"},
        "settings": {"interval_length": 1, "status_update_interval": 1},
        "resource_macros": {"USER1": dir.display().to_string()},
        "commands": [{"command_name": "dummy", "command_line": "$USER1$/plugin.sh $ARG1$ \"$ARG2$\""}],
        "host_templates": [{"name": "tpl", "check_command": "dummy!0!host up", "check_interval": 1}],
        "hosts": [{"host_name": "h1", "use": ["tpl"], "address": "127.0.0.1", "host_id": 10}],
        "service_templates": [{"name": "stpl", "check_interval": 30, "retry_interval": 1,
                               "max_check_attempts": 2}],
        "services": services,
    }))
    .unwrap()
}

#[tokio::test(flavor = "multi_thread", worker_threads = 2)]
async fn structured_configuration_and_incremental_reload() {
    let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
    let port = listener.local_addr().unwrap().port();
    let dir = tempfile::tempdir().unwrap();
    let plugin = dir.path().join("plugin.sh");
    std::fs::write(&plugin, "#!/bin/sh\necho \"$2 | value=$1\"\nexit $1\n").unwrap();
    std::fs::set_permissions(&plugin, std::fs::Permissions::from_mode(0o755)).unwrap();
    let kept = dir.path().join("state");
    let (events_tx, mut events) = mpsc::unbounded_channel();
    let (welcomes_tx, mut welcomes) = mpsc::unbounded_channel();
    tokio::spawn(fake_broker(
        listener,
        events_tx,
        welcomes_tx,
        Arc::new(Notify::new()),
    ));

    // Nothing kept: the engine waits, and refuses files-only requests.
    let scheduler = Scheduler::start(Options::structured(&kept));
    let error = scheduler.reload().await.unwrap_err();
    assert!(error.contains("no configuration applied yet"), "{error}");
    assert!(!scheduler.status().loaded);

    let summary = scheduler
        .apply(document(dir.path(), port, "broken", &[]), "d1".into())
        .await
        .unwrap();
    assert!(summary.contains("2 services"), "{summary}");
    assert!(summary.contains("Total Errors:   0"), "{summary}");
    assert_eq!(scheduler.status().digest.as_deref(), Some("d1"));
    let welcome = tokio::time::timeout(Duration::from_secs(10), welcomes.recv())
        .await
        .unwrap()
        .unwrap();
    assert_eq!(
        (welcome.poller_id, welcome.poller_name.as_str()),
        (5, "rust")
    );
    // The full dump once connected.
    let mut order = Vec::new();
    while order.len() < 5 {
        order.push(events.recv().await.unwrap().0);
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
    // Wait for the first statuses, so that the reload starts on a quiet line.
    wait_for(&mut events, Duration::from_secs(10), |kind, _| {
        (kind == event::SERVICE_STATUS).then_some(())
    })
    .await;

    // Reload: `crit` changes, `added` appears; `ok` and the host do not
    // change. The central gets only those, then the configuration end,
    // and no `pb_instance` (which would disable every object).
    let summary = scheduler
        .apply(
            document(dir.path(), port, "still broken", &[(3, "added")]),
            "d2".into(),
        )
        .await
        .unwrap();
    assert!(
        summary.contains("1 added, 1 changed, 0 removed"),
        "{summary}"
    );
    let mut configuration = Vec::new();
    wait_for(&mut events, Duration::from_secs(10), |kind, payload| {
        match kind {
            event::INSTANCE | event::HOST => configuration.push((kind, 0, true)),
            event::SERVICE => {
                let service = Service::decode(payload).unwrap();
                configuration.push((kind, service.service_id, service.enabled));
            }
            _ => {}
        }
        (kind == event::INSTANCE_CONFIGURATION).then_some(())
    })
    .await;
    configuration.sort_unstable();
    assert_eq!(
        configuration,
        [(event::SERVICE, 2, true), (event::SERVICE, 3, true)]
    );

    // Removal: `pb_service` with `enabled` false, nothing else.
    let summary = scheduler
        .apply(document(dir.path(), port, "still broken", &[]), "d3".into())
        .await
        .unwrap();
    assert!(
        summary.contains("0 added, 0 changed, 1 removed"),
        "{summary}"
    );
    let mut configuration = Vec::new();
    wait_for(&mut events, Duration::from_secs(10), |kind, payload| {
        match kind {
            event::INSTANCE => configuration.push((kind, 0, true)),
            event::HOST => configuration.push((kind, 0, Host::decode(payload).unwrap().enabled)),
            event::SERVICE => {
                let service = Service::decode(payload).unwrap();
                configuration.push((kind, service.service_id, service.enabled));
            }
            _ => {}
        }
        (kind == event::INSTANCE_CONFIGURATION).then_some(())
    })
    .await;
    assert_eq!(configuration, [(event::SERVICE, 3, false)]);

    // An invalid document is refused; the kept one and the one in force stay.
    let mut invalid: serde_json::Value =
        serde_json::from_slice(&document(dir.path(), port, "x", &[])).unwrap();
    invalid["hosts"][0]["notes_url"] = json!("x");
    let report = scheduler
        .apply(serde_json::to_vec(&invalid).unwrap(), "bad".into())
        .await
        .unwrap_err();
    assert!(report.contains("unknown field `notes_url`"), "{report}");
    assert_eq!(scheduler.status().digest.as_deref(), Some("d3"));
    assert_eq!(centreon_engine::kept_digest(&kept).as_deref(), Some("d3"));
    // A reload puts the kept document in force again: nothing changes.
    let summary = scheduler.reload().await.unwrap();
    assert!(
        summary.contains("0 added, 0 changed, 0 removed"),
        "{summary}"
    );
    scheduler.shutdown().await;

    // A new engine starts with the kept document.
    let restarted = Scheduler::start(Options::structured(&kept));
    let mut watch = restarted.clone();
    while !watch.status().loaded {
        tokio::time::timeout(Duration::from_secs(5), watch.status_changed())
            .await
            .unwrap();
    }
    let status = restarted.status();
    assert_eq!((status.services, status.digest.as_deref()), (2, Some("d3")));
    restarted.shutdown().await;

    // A files engine refuses documents.
    let files = Scheduler::start(Options::new(dir.path().join("absent.cfg")));
    let error = files.apply(b"{}".to_vec(), "x".into()).await.unwrap_err();
    assert!(error.contains("from files"), "{error}");
    files.shutdown().await;
}
