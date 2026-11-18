//! The scheduler against a real cbd, run by `tests/real-cbd/run.sh`: cbd
//! decodes what the scheduler sends and its Lua output dumps every event as
//! JSON; this test reads the dump. Ignored without that script.

use std::os::unix::fs::PermissionsExt;
use std::path::{Path, PathBuf};
use std::time::Duration;

use centreon_engine::wire::{Decoder, HEADER_SIZE, event, frame};
use centreon_engine::{Options, Scheduler};
use gorgone_proto::bbdo::{Bbdo, PeerType, Welcome};
use prost::Message;
use serde_json::Value;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::TcpStream;

/// `module.json` of the poller: its identity and, with `tls` (the
/// directory of the certificates `run.sh` made), the TLS keys of its
/// output, as centreon-web writes them.
fn module(address: &str, tls: Option<&str>) -> String {
    let (host, port) = address.rsplit_once(':').unwrap();
    let mut output = serde_json::json!({"name": "central-module-master-output", "type": "ipv4",
        "host": host, "port": port, "protocol": "bbdo", "negotiation": "yes"});
    if let Some(dir) = tls {
        output["tls"] = "yes".into();
        output["public_cert"] = format!("{dir}/poller.pem").into();
        output["private_key"] = format!("{dir}/poller.key").into();
        output["ca_certificate"] = format!("{dir}/ca.pem").into();
        output["tls_hostname"] = "central".into();
    }
    serde_json::json!({"centreonBroker": {"poller_id": 42, "poller_name": "rust-poller",
        "broker_name": "rust-poller-module", "output": [output]}})
    .to_string()
}

fn write_config(dir: &Path) -> PathBuf {
    let plugin = dir.join("plugin.sh");
    std::fs::write(&plugin, "#!/bin/sh\necho \"$2 | value=$1;1;2\"\nexit $1\n").unwrap();
    std::fs::set_permissions(&plugin, std::fs::Permissions::from_mode(0o755)).unwrap();
    std::fs::write(
        dir.join("centengine.cfg"),
        "cfg_file=objects.cfg\ninterval_length=1\nstatus_update_interval=2\n\
         broker_module_cfg_file=module.json\n",
    )
    .unwrap();
    std::fs::write(
        dir.join("objects.cfg"),
        format!(
            "define command {{\n command_name dummy\n command_line {} $ARG1$ \"$ARG2$\"\n}}\n\
             define host {{\n host_name rust-h1\n alias Rust host\n address 127.0.0.1\n \
             check_command dummy!0!up\n _HOST_ID 9001\n}}\n\
             define service {{\n host_name rust-h1\n service_description ok\n \
             check_command dummy!0!fine\n check_interval 2\n _SERVICE_ID 1\n}}\n\
             define service {{\n host_name rust-h1\n service_description crit\n \
             check_command dummy!2!broken\n check_interval 2\n retry_interval 1\n \
             max_check_attempts 3\n _SERVICE_ID 2\n}}\n",
            plugin.display()
        ),
    )
    .unwrap();
    dir.join("centengine.cfg")
}

fn dump(path: &Path) -> Vec<Value> {
    std::fs::read_to_string(path)
        .unwrap_or_default()
        .lines()
        .filter_map(|line| serde_json::from_str(line).ok())
        .collect()
}

fn is(event: &Value, element: u64) -> bool {
    event["category"] == 1 && event["element"] == element
}

async fn wait_for(path: &Path, what: &str, found: impl Fn(&[Value]) -> bool) -> Vec<Value> {
    for _ in 0..80 {
        let events = dump(path);
        if found(&events) {
            return events;
        }
        tokio::time::sleep(Duration::from_millis(500)).await;
    }
    panic!("{what}: not in the cbd dump {}", path.display());
}

#[tokio::test(flavor = "multi_thread", worker_threads = 2)]
#[ignore = "needs a cbd: run tests/real-cbd/run.sh"]
async fn real_cbd_decodes_and_stores_the_events() {
    let address = std::env::var("GORGONE_TEST_CBD_ADDRESS").expect("GORGONE_TEST_CBD_ADDRESS");
    let events_file =
        PathBuf::from(std::env::var("GORGONE_TEST_CBD_EVENTS").expect("GORGONE_TEST_CBD_EVENTS"));

    // Bytes written by cbd itself: its welcome decodes with our framing.
    let mut raw = TcpStream::connect(&address).await.unwrap();
    let hello = frame(
        event::WELCOME,
        &Welcome {
            version: Some(Bbdo {
                major: 3,
                minor: 0,
                patch: 1,
            }),
            poller_id: 4242,
            poller_name: "framing-probe".into(),
            broker_name: "framing-probe".into(),
            peer_type: PeerType::Engine.into(),
            ..Welcome::default()
        },
    );
    raw.write_all(&hello).await.unwrap();
    let mut bytes = vec![0u8; 4096];
    let n = tokio::time::timeout(Duration::from_secs(10), raw.read(&mut bytes))
        .await
        .unwrap()
        .unwrap();
    let received = &bytes[..n];
    println!(
        "cbd welcome header: {}",
        received[..HEADER_SIZE.min(n)]
            .iter()
            .map(|byte| format!("{byte:02x}"))
            .collect::<String>()
    );
    let mut decoder = Decoder::default();
    decoder.push(received);
    let (event_type, payload) = decoder.next_event().expect("a full event from cbd");
    assert_eq!(decoder.skipped, 0, "bad checksum in cbd's bytes");
    assert_eq!(event_type, event::WELCOME);
    let welcome = Welcome::decode(payload.as_slice()).unwrap();
    assert_eq!(welcome.version.unwrap().major, 3);
    assert_eq!(welcome.broker_name, "central-broker-master");
    assert_eq!(welcome.peer_type, PeerType::Broker as i32);
    drop(raw);

    let dir = tempfile::tempdir().unwrap();
    let main = write_config(dir.path());
    let tls = std::env::var("GORGONE_TEST_CBD_TLS").ok();
    std::fs::write(
        dir.path().join("module.json"),
        module(&address, tls.as_deref()),
    )
    .unwrap();
    let mut options = Options::new(main);
    options.broker_address = Some(address);
    let scheduler = Scheduler::start(options);

    let events = wait_for(&events_file, "instance, host, services", |events| {
        events
            .iter()
            .any(|e| is(e, 44) && e["instance_id"] == 42 && e["running"] == true)
            && events.iter().any(|e| is(e, 30) && e["host_id"] == 9001)
            && events
                .iter()
                .filter(|e| is(e, 27) && e["host_id"] == 9001)
                .count()
                >= 2
            && events.iter().any(|e| is(e, 54) && e["poller_id"] == 42)
    })
    .await;
    let host = events
        .iter()
        .find(|e| is(e, 30) && e["host_id"] == 9001)
        .unwrap();
    assert_eq!(host["name"], "rust-h1");
    assert_eq!(host["alias"], "Rust host");
    assert_eq!(host["instance_id"], 42);

    // CRITICAL with 3 attempts and a 1 s retry: SOFT 1, SOFT 2, HARD 3.
    let events = wait_for(&events_file, "HARD CRITICAL status", |events| {
        events
            .iter()
            .any(|e| is(e, 29) && e["service_id"] == 2 && e["state"] == 2 && e["state_type"] == 1)
    })
    .await;
    let attempts: Vec<i64> = events
        .iter()
        .filter(|e| is(e, 29) && e["service_id"] == 2 && e["checked"] == true)
        .map(|e| e["check_attempt"].as_i64().unwrap())
        .collect();
    assert_eq!(&attempts[..3], [1, 2, 3], "{attempts:?}");
    let hard = events
        .iter()
        .find(|e| is(e, 29) && e["service_id"] == 2 && e["state"] == 2 && e["state_type"] == 1)
        .unwrap();
    assert_eq!(hard["output"], "broken");
    assert_eq!(hard["perfdata"], "value=2;1;2");
    assert!(
        events
            .iter()
            .any(|e| is(e, 32) && e["host_id"] == 9001 && e["checked"] == true),
        "no host status"
    );
    wait_for(&events_file, "instance status", |events| {
        events.iter().any(|e| is(e, 42) && e["instance_id"] == 42)
    })
    .await;

    scheduler.shutdown().await;
    wait_for(&events_file, "instance end", |events| {
        events
            .iter()
            .any(|e| is(e, 44) && e["instance_id"] == 42 && e["running"] == false)
    })
    .await;

    let log = std::fs::read_to_string(events_file.with_file_name("cbd.log")).unwrap_or_default();
    assert!(!log.contains("corrupted"), "cbd saw corrupted data");
    assert!(
        !log.contains("cannot be decoded"),
        "cbd could not decode an event"
    );
    assert!(
        log.contains("received stop from peer with ID 42"),
        "no pb_stop seen by cbd"
    );
    if tls.is_some() {
        assert!(
            log.contains("and peer has 'TLS'") && log.contains("TLS: successful handshake"),
            "cbd did not negotiate TLS"
        );
    }
}

/// The bench key of `deploy/centreon/compose.yaml` (public on purpose).
const BENCH_KEY: &str = r#"{"app_secret":"YmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmI=","salt":"c3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nz"}"#;

fn read(dir: &str, name: &str) -> String {
    std::fs::read_to_string(Path::new(dir).join(name)).unwrap()
}

/// A structured document (version 5) with the certificates inline and the
/// key encrypted, as centreon-web's managed PKI sends them: cbd checks the
/// poller certificate against its CA, the engine the certificate of cbd.
#[tokio::test(flavor = "multi_thread", worker_threads = 2)]
#[ignore = "needs a cbd: run tests/real-cbd/run.sh"]
async fn real_cbd_takes_inline_certificates_of_its_ca() {
    let Ok(tls) = std::env::var("GORGONE_TEST_CBD_TLS") else {
        println!("TLS=1 not set: skipped");
        return;
    };
    let address = std::env::var("GORGONE_TEST_CBD_ADDRESS").expect("GORGONE_TEST_CBD_ADDRESS");
    let events_file =
        PathBuf::from(std::env::var("GORGONE_TEST_CBD_EVENTS").expect("GORGONE_TEST_CBD_EVENTS"));
    let dir = tempfile::tempdir().unwrap();
    let credentials = dir.path().join("engine-context.json");
    std::fs::write(&credentials, BENCH_KEY).unwrap();
    let keys = centreon_engine::Credentials::from_json(BENCH_KEY.as_bytes()).unwrap();
    let key = format!(
        "encrypt::{}",
        keys.encrypt_with_iv(&read(&tls, "inline.key"), [3; 16])
    );
    let plugin = dir.path().join("plugin.sh");
    std::fs::write(&plugin, "#!/bin/sh\necho \"$2\"\nexit $1\n").unwrap();
    std::fs::set_permissions(&plugin, std::fs::Permissions::from_mode(0o755)).unwrap();
    let document = serde_json::json!({
        "version": 5,
        "poller": {"id": 43, "name": "rust-inline"},
        "broker": {"address": address, "name": "rust-inline-module",
                   "tls": {"tls": "yes", "ca_certificate_pem": read(&tls, "ca.pem"),
                           "public_cert_pem": read(&tls, "inline.pem"),
                           "private_key_pem": key, "tls_hostname": "central"}},
        "commands": [{"command_name": "dummy",
                      "command_line": format!("{} $ARG1$ \"$ARG2$\"", plugin.display())}],
        "hosts": [{"host_name": "rust-inline-h", "address": "127.0.0.1", "host_id": 9101,
                   "check_command": "dummy!0!up"}]
    });
    let mut options = Options::structured(dir.path().join("state"));
    options.credentials_file = Some(credentials);
    let scheduler = Scheduler::start(options);
    let report = scheduler
        .apply(serde_json::to_vec(&document).unwrap(), "inline".into())
        .await
        .unwrap();
    assert!(report.contains("Total Errors:   0"), "{report}");
    wait_for(
        &events_file,
        "instance and host of the inline poller",
        |events| {
            events
                .iter()
                .any(|e| is(e, 44) && e["instance_id"] == 43 && e["running"] == true)
                && events.iter().any(|e| is(e, 30) && e["host_id"] == 9101)
        },
    )
    .await;
    assert!(scheduler.connected());
    scheduler.shutdown().await;
}

/// A poller certificate of another CA: cbd refuses it (mutual
/// authentication), nothing of that poller is stored.
#[tokio::test(flavor = "multi_thread", worker_threads = 2)]
#[ignore = "needs a cbd: run tests/real-cbd/run.sh"]
async fn real_cbd_refuses_a_poller_of_another_ca() {
    let Ok(tls) = std::env::var("GORGONE_TEST_CBD_TLS") else {
        println!("TLS=1 not set: skipped");
        return;
    };
    let address = std::env::var("GORGONE_TEST_CBD_ADDRESS").expect("GORGONE_TEST_CBD_ADDRESS");
    let events_file =
        PathBuf::from(std::env::var("GORGONE_TEST_CBD_EVENTS").expect("GORGONE_TEST_CBD_EVENTS"));
    let dir = tempfile::tempdir().unwrap();
    let main = write_config(dir.path());
    let (host, port) = address.rsplit_once(':').unwrap();
    // The rogue poller trusts the real CA: only cbd's check can refuse it.
    let module = serde_json::json!({"centreonBroker": {"poller_id": 44,
        "poller_name": "rust-rogue", "broker_name": "rust-rogue-module",
        "output": [{"name": "rogue-output", "type": "ipv4", "host": host, "port": port,
                    "protocol": "bbdo", "negotiation": "yes", "tls": "yes",
                    "public_cert": format!("{tls}/rogue.pem"),
                    "private_key": format!("{tls}/rogue.key"),
                    "ca_certificate": format!("{tls}/ca.pem"), "tls_hostname": "central"}]}});
    std::fs::write(dir.path().join("module.json"), module.to_string()).unwrap();
    let mut options = Options::new(main);
    options.broker_address = Some(address);
    let scheduler = Scheduler::start(options);
    let log_file = events_file.with_file_name("cbd.log");
    let mut refused = false;
    for _ in 0..60 {
        let log = std::fs::read_to_string(&log_file).unwrap_or_default();
        // GnuTLS reports GNUTLS_CERT_INVALID before SIGNER_NOT_FOUND
        // (`params::validate_cert`); the dump then shows nothing of it.
        if log.contains("peer certificate is invalid")
            || log.contains("peer certificate was not issued by a trusted authority")
        {
            refused = true;
            break;
        }
        tokio::time::sleep(Duration::from_millis(500)).await;
    }
    assert!(refused, "cbd did not refuse the certificate of another CA");
    tokio::time::sleep(Duration::from_secs(3)).await;
    assert!(
        !dump(&events_file)
            .iter()
            .any(|e| e["instance_id"] == 44 || e["poller_id"] == 44),
        "cbd stored events of the rogue poller"
    );
    scheduler.shutdown().await;
}
