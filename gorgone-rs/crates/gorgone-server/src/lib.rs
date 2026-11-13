//! Gorgone central control plane: REST API, agent gateway, job store and
//! dispatcher. Replicas are identical and share one SQL store.

#[cfg(not(any(feature = "sqlite", feature = "mysql")))]
compile_error!("enable at least one store backend: feature `sqlite` or `mysql`");

mod api;
mod cidr;
pub mod config;
mod dispatcher;
mod error;
mod events;
mod gateway;
mod hub;
mod legacy_intake;
mod metrics;
mod peer;
mod store;
mod tls;

use std::future::Future;
use std::net::SocketAddr;
use std::sync::Arc;
use std::sync::atomic::AtomicU64;
use std::time::Duration;

use axum::serve::ListenerExt;
use gorgone_proto::agent::agent_gateway_server::AgentGatewayServer;
use gorgone_proto::peer::peer_server::PeerServer;
use sha2::{Digest, Sha256};
use tokio::net::TcpListener;
use tokio::task::JoinHandle;
use tokio_util::sync::CancellationToken;
use tonic::transport::server::TcpIncoming;
use tracing::{info, warn};

pub use crate::config::Config;
pub use crate::error::Error;

use crate::dispatcher::{Dispatcher, DispatcherConfig};
use crate::hub::Hub;
use crate::store::Store;

/// Largest gRPC message accepted from an agent.
const MAX_AGENT_MESSAGE: usize = 4 << 20;
/// Time left to the listeners to finish in-flight work on shutdown.
const SHUTDOWN_GRACE: Duration = Duration::from_secs(5);

/// Compares two secrets through their SHA-256, so that neither length nor
/// content shows in timing.
pub(crate) fn secrets_match(given: &str, expected: &str) -> bool {
    let given = Sha256::digest(given.as_bytes());
    let expected = Sha256::digest(expected.as_bytes());
    given
        .iter()
        .zip(expected.iter())
        .fold(0u8, |acc, (a, b)| acc | (a ^ b))
        == 0
}

/// Lowercase hexadecimal form of `bytes`.
pub(crate) fn hex(bytes: &[u8]) -> String {
    use std::fmt::Write;
    bytes.iter().fold(String::new(), |mut out, byte| {
        let _ = write!(out, "{byte:02x}");
        out
    })
}

/// `N` bytes from the operating system generator, in hexadecimal.
pub(crate) fn random_hex<const N: usize>() -> Result<String, Error> {
    let mut bytes = [0u8; N];
    getrandom::getrandom(&mut bytes)
        .map_err(|err| std::io::Error::other(err.to_string()))
        .map_err(Error::io("system random generator"))?;
    Ok(hex(&bytes))
}

/// Random identifier (128 bits) of a job, a session or a replica.
pub(crate) fn random_id() -> Result<String, Error> {
    random_hex::<16>()
}

/// Bearer token of a gRPC request, if any.
pub(crate) fn bearer_token(metadata: &tonic::metadata::MetadataMap) -> Option<String> {
    let value = metadata.get("authorization")?.to_str().ok()?;
    let (scheme, token) = value.split_once(' ')?;
    scheme
        .eq_ignore_ascii_case("bearer")
        .then(|| token.trim().to_owned())
}

/// Entry point to run a replica inside a process.
#[derive(Debug)]
pub struct Server;

/// A running replica.
#[derive(Debug)]
pub struct ServerHandle {
    /// Bound address of the REST listener.
    pub rest_addr: SocketAddr,
    /// Bound address of the agent gateway.
    pub agent_addr: SocketAddr,
    /// Bound address of the peer service.
    pub peer_addr: SocketAddr,
    /// Identifier of this replica in the store.
    pub replica_id: String,
    shutdown: CancellationToken,
    tasks: Vec<JoinHandle<()>>,
    store: Store,
    hub: Arc<Hub>,
}

async fn bind(addr: SocketAddr) -> Result<TcpListener, Error> {
    TcpListener::bind(addr)
        .await
        .map_err(Error::io(format!("bind {addr}")))
}

fn local_addr(listener: &TcpListener) -> Result<SocketAddr, Error> {
    listener.local_addr().map_err(Error::io("local address"))
}

/// Runs a listener to completion and logs why it stopped.
fn spawn_listener<F>(name: &'static str, serve: F) -> JoinHandle<()>
where
    F: Future<Output = Result<(), Error>> + Send + 'static,
{
    tokio::spawn(async move {
        if let Err(err) = serve.await {
            warn!(listener = name, error = %err, "listener stopped");
        }
    })
}

/// Small frames (acknowledgements, dispatches) must leave at once: with
/// Nagle's algorithm they wait for the peer's delayed ACK, tens of
/// milliseconds per exchange.
fn no_delay(listener: TcpListener) -> TcpIncoming {
    TcpIncoming::from(listener).with_nodelay(Some(true))
}

fn rest_no_delay(stream: &mut tokio::net::TcpStream) {
    if let Err(err) = stream.set_nodelay(true) {
        warn!(error = %err, "cannot disable Nagle's algorithm on a REST connection");
    }
}

async fn serve_rest(
    listener: TcpListener,
    app: axum::Router,
    tls: Option<Arc<rustls::ServerConfig>>,
    shutdown: CancellationToken,
) -> Result<(), Error> {
    let service = app.into_make_service_with_connect_info::<SocketAddr>();
    let stopped = shutdown.cancelled_owned();
    // Both stop accepting on shutdown and let the open connections finish;
    // `ServerHandle::shutdown` bounds that wait.
    let served = match tls {
        None => {
            axum::serve(listener.tap_io(rest_no_delay), service)
                .with_graceful_shutdown(stopped)
                .await
        }
        Some(tls) => {
            let listener = tls::TlsListener::new(listener, tls)
                .map_err(Error::io("rest listener"))?
                .tap_io(|stream| rest_no_delay(stream.get_mut().0));
            axum::serve(listener, service)
                .with_graceful_shutdown(stopped)
                .await
        }
    };
    served.map_err(Error::io("rest listener"))
}

impl Server {
    /// Starts a replica: connects to the store, binds the three listeners
    /// (port 0 is allowed) and spawns the background work.
    pub async fn start(config: Config) -> Result<ServerHandle, Error> {
        config.validate()?;
        if !config.rest.requires_credentials() {
            // `Config::load` refuses this unless `insecure_no_auth` is set;
            // an embedder building the configuration itself is warned.
            warn!("the REST API runs without credentials");
        }
        // Several TLS backends are linked in: pick one explicitly. An error
        // only means it is already installed.
        let _ = rustls::crypto::ring::default_provider().install_default();

        let store = Store::connect_with(
            &config.database_url,
            store::StoreOptions {
                max_connections: config.db_max_connections,
                sqlite_relaxed_sync: config.sqlite_synchronous == config::SqliteSynchronous::Normal,
            },
        )
        .await?;
        let replica_id = match &config.replica_id {
            Some(replica_id) => replica_id.clone(),
            None => random_id()?,
        };

        let rest = bind(config.rest_listen).await?;
        let agent = bind(config.agent_listen).await?;
        let peer = bind(config.peer_listen).await?;
        let (rest_addr, agent_addr, peer_addr) =
            (local_addr(&rest)?, local_addr(&agent)?, local_addr(&peer)?);

        let peer_advertise = config.peer_advertise_addr.clone().unwrap_or_else(|| {
            // A wildcard bind is only reachable locally without more input.
            let ip = if peer_addr.ip().is_unspecified() {
                std::net::IpAddr::from([127, 0, 0, 1])
            } else {
                peer_addr.ip()
            };
            let scheme = if config.peer_tls.is_some() {
                "https"
            } else {
                "http"
            };
            format!("{scheme}://{}", SocketAddr::new(ip, peer_addr.port()))
        });
        let peer_client_tls = config
            .peer_ca
            .as_deref()
            .map(|ca| tls::peer_client(ca, config.peer_tls.as_ref()))
            .transpose()?;
        let peers = peer::PeerClients::new(config.peer_token.as_deref(), peer_client_tls)?;

        let hub = Arc::new(Hub::new(Duration::from_millis(
            config.session_send_timeout_ms,
        )));
        let dispatcher = Arc::new(Dispatcher::new(
            store.clone(),
            Arc::clone(&hub),
            peers,
            DispatcherConfig {
                replica_id: replica_id.clone(),
                peer_addr: peer_advertise.clone(),
                default_deadline: Duration::from_secs(config.job_default_deadline_s),
                validation_deadline: Duration::from_secs(config.validation_deadline_s),
                sweep_interval: config.sweep_interval(),
                retention: Duration::from_secs(config.retention_s),
                auto_enroll: config.auto_enroll,
            },
        ));
        // Sessions recorded for this replica id belong to a former
        // incarnation: this process holds none yet.
        let stale = store.remove_replica_sessions(&replica_id).await?;
        if stale > 0 {
            info!(
                stale,
                "removed sessions left by a former run of this replica"
            );
        }
        // Known to the peers before the first session can be registered.
        store
            .heartbeat_replica(&replica_id, &peer_advertise)
            .await?;

        let shutdown = CancellationToken::new();
        let mut tasks = Vec::new();

        tasks.extend(dispatcher.spawn_background(&shutdown));

        let gateway = gateway::GatewayService(Arc::new(gateway::Gateway {
            store: store.clone(),
            hub: Arc::clone(&hub),
            dispatcher: Arc::clone(&dispatcher),
            replica_id: replica_id.clone(),
            heartbeat_interval: config.heartbeat_interval(),
            auto_enroll: config.auto_enroll,
            opening: Arc::new(tokio::sync::Semaphore::new(gateway::MAX_OPENING_SESSIONS)),
        }));
        let mut agent_server = tonic::transport::Server::builder()
            .http2_keepalive_interval(Some(Duration::from_secs(30)));
        if let Some(tls) = &config.agent_tls {
            agent_server = agent_server.tls_config(tls::grpc(tls)?)?;
        }
        let agent_router = agent_server.add_service(
            AgentGatewayServer::new(gateway).max_decoding_message_size(MAX_AGENT_MESSAGE),
        );
        let signal = shutdown.clone().cancelled_owned();
        tasks.push(spawn_listener("agent", async move {
            agent_router
                .serve_with_incoming_shutdown(no_delay(agent), signal)
                .await
                .map_err(Error::from)
        }));

        let mut peer_server = tonic::transport::Server::builder();
        if let Some(tls) = &config.peer_tls {
            peer_server = peer_server.tls_config(tls::grpc(tls)?)?;
        }
        let peer_router = peer_server.add_service(PeerServer::new(peer::PeerService {
            dispatcher: Arc::clone(&dispatcher),
            token: config.peer_token.clone(),
        }));
        let signal = shutdown.clone().cancelled_owned();
        tasks.push(spawn_listener("peer", async move {
            peer_router
                .serve_with_incoming_shutdown(no_delay(peer), signal)
                .await
                .map_err(Error::from)
        }));

        // An explicit setting wins; otherwise the id persisted in the store.
        let thumbprint = match &config.thumbprint {
            Some(thumbprint) => thumbprint.clone(),
            None => store.server_id().await?,
        };
        if config.centcore.enabled {
            let intake = legacy_intake::Intake::new(
                config.centcore.clone(),
                Arc::clone(&dispatcher),
                store.clone(),
                replica_id.clone(),
            );
            tasks.push(tokio::spawn(intake.run(shutdown.clone())));
        }
        let app = api::router(Arc::new(api::ApiState {
            store: store.clone(),
            hub: Arc::clone(&hub),
            dispatcher,
            auth: config.rest.clone(),
            thumbprint,
            body_limit: config.body_limit_bytes,
            metrics_public: config.metrics_public,
            legacy_requests: AtomicU64::new(0),
        }));
        let rest_tls = config.rest_tls.as_ref().map(tls::rest).transpose()?;
        tasks.push(spawn_listener(
            "rest",
            serve_rest(rest, app, rest_tls, shutdown.clone()),
        ));

        info!(
            replica = %replica_id,
            rest = %rest_addr,
            agent = %agent_addr,
            peer = %peer_addr,
            peer_advertise = %peer_advertise,
            "gorgone-server started"
        );
        Ok(ServerHandle {
            rest_addr,
            agent_addr,
            peer_addr,
            replica_id,
            shutdown,
            tasks,
            store,
            hub,
        })
    }
}

impl ServerHandle {
    /// Stops the listeners, closes the agent sessions and removes this
    /// replica and its sessions from the store.
    pub async fn shutdown(self) {
        self.shutdown.cancel();
        self.hub.close_all();
        for mut task in self.tasks {
            // Long polls and event streams may outlive the grace period.
            if tokio::time::timeout(SHUTDOWN_GRACE, &mut task)
                .await
                .is_err()
            {
                task.abort();
            }
        }
        if let Err(err) = self.store.remove_replica(&self.replica_id).await {
            warn!(error = %err, "replica not removed from the store");
        }
        self.store.close().await;
        info!(replica = %self.replica_id, "gorgone-server stopped");
    }
}

/// Runs a replica until `shutdown` completes, then stops it gracefully.
pub async fn run(config: Config, shutdown: impl Future<Output = ()>) -> Result<(), Error> {
    let handle = Server::start(config).await?;
    shutdown.await;
    handle.shutdown().await;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn random_identifiers() {
        assert_eq!(hex(&[0x00, 0x0f, 0xa5, 0xff]), "000fa5ff");
        let id = random_id().unwrap();
        assert_eq!(id.len(), 32);
        assert!(id.bytes().all(|b| b.is_ascii_hexdigit()));
        assert_ne!(id, random_id().unwrap());
        assert_eq!(random_hex::<32>().unwrap().len(), 64);
    }

    #[test]
    fn secrets_comparison() {
        assert!(secrets_match("token", "token"));
        assert!(!secrets_match("token", "token2"));
        assert!(!secrets_match("", "token"));
    }
}
