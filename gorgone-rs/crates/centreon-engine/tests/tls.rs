//! The BBDO `TLS` extension against a fake central broker: the welcomes
//! say both sides have it, then the engine is the TLS client of the same
//! connection, with its certificate, and every event goes inside TLS. The
//! certificates are files, or inline in the document (version 5) with an
//! `encrypt::` key, as centreon-web's managed PKI sends them.

use std::sync::Arc;
use std::time::Duration;

use centreon_engine::wire::{Decoder, event, frame};
use centreon_engine::{Options, Scheduler};
use gorgone_proto::bbdo::{Ack, Bbdo, Instance, PeerType, Welcome};
use prost::Message;
use rustls::pki_types::pem::PemObject;
use rustls::pki_types::{CertificateDer, PrivateKeyDer};
use rustls::server::WebPkiClientVerifier;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::TcpListener;
use tokio::sync::mpsc;

struct Pki {
    ca: rcgen::CertifiedIssuer<'static, rcgen::KeyPair>,
}

impl Pki {
    fn new() -> Self {
        let mut params = rcgen::CertificateParams::new(Vec::<String>::new()).unwrap();
        params.is_ca = rcgen::IsCa::Ca(rcgen::BasicConstraints::Unconstrained);
        params
            .distinguished_name
            .push(rcgen::DnType::CommonName, "bbdo test ca");
        params.key_usages = vec![
            rcgen::KeyUsagePurpose::KeyCertSign,
            rcgen::KeyUsagePurpose::DigitalSignature,
        ];
        let key = rcgen::KeyPair::generate().unwrap();
        Self {
            ca: rcgen::CertifiedIssuer::self_signed(params, key).unwrap(),
        }
    }

    fn issue(&self, name: &str) -> (String, String) {
        let mut params = rcgen::CertificateParams::new(vec![name.to_owned()]).unwrap();
        params
            .distinguished_name
            .push(rcgen::DnType::CommonName, name);
        let key = rcgen::KeyPair::generate().unwrap();
        let cert = params.signed_by(&key, &self.ca).unwrap();
        (cert.pem(), key.serialize_pem())
    }
}

/// Answers the welcome with `TLS`, then reads the events inside TLS,
/// acknowledging them, and forwards their types.
async fn tls_broker(
    listener: TcpListener,
    config: Arc<rustls::ServerConfig>,
    events: mpsc::UnboundedSender<(u32, Vec<u8>)>,
    welcomes: mpsc::UnboundedSender<Welcome>,
) {
    let (mut socket, _) = listener.accept().await.unwrap();
    let mut decoder = Decoder::default();
    let mut chunk = vec![0u8; 65536];
    loop {
        if let Some((kind, payload)) = decoder.next_event() {
            assert_eq!(kind, event::WELCOME);
            welcomes
                .send(Welcome::decode(payload.as_slice()).unwrap())
                .unwrap();
            break;
        }
        let n = socket.read(&mut chunk).await.unwrap();
        decoder.push(&chunk[..n]);
    }
    let answer = frame(
        event::WELCOME,
        &Welcome {
            version: Some(Bbdo {
                major: 3,
                minor: 0,
                patch: 1,
            }),
            extensions: "TLS COMPRESSION".into(),
            broker_name: "central-broker-master".into(),
            peer_type: PeerType::Broker.into(),
            ..Welcome::default()
        },
    );
    socket.write_all(&answer).await.unwrap();
    let mut tls = tokio_rustls::TlsAcceptor::from(config)
        .accept(socket)
        .await
        .unwrap();
    let mut decoder = Decoder::default();
    loop {
        let mut unacked = 0;
        while let Some((kind, payload)) = decoder.next_event() {
            unacked += 1;
            let _ = events.send((kind, payload));
        }
        if unacked > 0 {
            let ack = frame(
                event::PB_ACK,
                &Ack {
                    acknowledged_events: unacked,
                },
            );
            if tls.write_all(&ack).await.is_err() {
                return;
            }
        }
        match tls.read(&mut chunk).await {
            Ok(0) | Err(_) => return,
            Ok(n) => decoder.push(&chunk[..n]),
        }
    }
}

/// The central broker's TLS server: its certificate from `pki`, a client
/// certificate of that CA required.
fn central(pki: &Pki) -> Arc<rustls::ServerConfig> {
    let (server_cert, server_key) = pki.issue("central");

    let mut roots = rustls::RootCertStore::empty();
    roots
        .add(CertificateDer::from_pem_slice(pki.ca.pem().as_bytes()).unwrap())
        .unwrap();
    let provider = Arc::new(rustls::crypto::ring::default_provider());
    let verifier = WebPkiClientVerifier::builder_with_provider(Arc::new(roots), provider.clone())
        .build()
        .unwrap();
    let config = rustls::ServerConfig::builder_with_provider(provider)
        .with_safe_default_protocol_versions()
        .unwrap()
        .with_client_cert_verifier(verifier)
        .with_single_cert(
            vec![CertificateDer::from_pem_slice(server_cert.as_bytes()).unwrap()],
            PrivateKeyDer::from_pem_slice(server_key.as_bytes()).unwrap(),
        )
        .unwrap();
    Arc::new(config)
}

/// Applies `document` (its broker address set to a fake central broker
/// with `TLS`) and checks that the instance arrives inside TLS.
async fn reaches_the_central(
    pki: &Pki,
    mut document: serde_json::Value,
    dir: &std::path::Path,
    credentials: Option<std::path::PathBuf>,
) {
    let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
    let port = listener.local_addr().unwrap().port();
    let (events_tx, mut events) = mpsc::unbounded_channel();
    let (welcomes_tx, mut welcomes) = mpsc::unbounded_channel();
    tokio::spawn(tls_broker(listener, central(pki), events_tx, welcomes_tx));
    document["broker"]["address"] = format!("127.0.0.1:{port}").into();

    let mut options = Options::structured(dir.join("state"));
    options.credentials_file = credentials;
    let scheduler = Scheduler::start(options);
    let report = scheduler
        .apply(serde_json::to_vec(&document).unwrap(), "d".into())
        .await
        .unwrap();
    assert!(report.contains("Total Errors:   0"), "{report}");

    let welcome = tokio::time::timeout(Duration::from_secs(10), welcomes.recv())
        .await
        .unwrap()
        .unwrap();
    assert_eq!(welcome.extensions, "TLS");
    // The dump comes inside TLS.
    let instance = loop {
        let (kind, payload) = tokio::time::timeout(Duration::from_secs(10), events.recv())
            .await
            .expect("events inside TLS")
            .unwrap();
        if kind == event::INSTANCE {
            break Instance::decode(payload.as_slice()).unwrap();
        }
    };
    assert_eq!((instance.instance_id, instance.running), (8, true));
    assert!(scheduler.connected());
    scheduler.shutdown().await;
}

#[tokio::test(flavor = "multi_thread", worker_threads = 2)]
async fn the_central_broker_is_reached_inside_tls() {
    let pki = Pki::new();
    let dir = tempfile::tempdir().unwrap();
    let write = |name: &str, text: &str| {
        let path = dir.path().join(name);
        std::fs::write(&path, text).unwrap();
        path.display().to_string()
    };
    let ca = write("ca.pem", &pki.ca.pem());
    let (cert, key) = pki.issue("poller");
    let (cert, key) = (write("poller.pem", &cert), write("poller.key", &key));
    let document = serde_json::json!({
        "version": 4,
        "poller": {"id": 8, "name": "rust-tls"},
        "broker": {"address": "set later:1", "name": "rust-tls-module",
                   "tls": {"tls": "yes", "public_cert": cert, "private_key": key,
                           "ca_certificate": ca, "tls_hostname": "central"}},
        "hosts": [{"host_name": "h1", "address": "127.0.0.1", "host_id": 1}]
    });
    reaches_the_central(&pki, document, dir.path(), None).await;
}

/// The bench key of `deploy/centreon/compose.yaml` (public on purpose).
const BENCH_KEY: &str = r#"{"app_secret":"YmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmI=","salt":"c3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nz"}"#;

#[tokio::test(flavor = "multi_thread", worker_threads = 2)]
async fn inline_certificates_and_an_encrypted_key_reach_the_central() {
    let pki = Pki::new();
    let dir = tempfile::tempdir().unwrap();
    let credentials = dir.path().join("engine-context.json");
    std::fs::write(&credentials, BENCH_KEY).unwrap();
    let keys = centreon_engine::Credentials::from_json(BENCH_KEY.as_bytes()).unwrap();
    let (cert, key) = pki.issue("poller");
    let encrypted = format!("encrypt::{}", keys.encrypt_with_iv(&key, [9; 16]));
    let document = serde_json::json!({
        "version": 5,
        "poller": {"id": 8, "name": "rust-tls"},
        "broker": {"address": "set later:1", "name": "rust-tls-module",
                   "tls": {"tls": "yes", "ca_certificate_pem": pki.ca.pem(),
                           "public_cert_pem": cert, "private_key_pem": encrypted,
                           "tls_hostname": "central"}},
        "hosts": [{"host_name": "h1", "address": "127.0.0.1", "host_id": 1}]
    });
    reaches_the_central(&pki, document, dir.path(), Some(credentials)).await;
    // The configuration kept on disk holds the key encrypted only.
    let kept = std::fs::read_to_string(dir.path().join("state/configuration")).unwrap();
    assert!(kept.contains("encrypt::") && !kept.contains("PRIVATE KEY"));
}
