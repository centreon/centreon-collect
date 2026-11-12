//! Gorgone central control plane: REST API, agent gateway, job store and
//! dispatcher. Replicas are identical and share one SQL store.

mod api;
pub mod config;
mod dispatcher;
mod error;
mod events;
mod gateway;
mod hub;
mod legacy_intake;
mod peer;
mod store;
mod tls;

use std::future::Future;
use std::net::SocketAddr;
use std::sync::Arc;
use std::sync::atomic::AtomicU64;
use std::time::Duration;

use gorgone_proto::agent::agent_gateway_server::AgentGatewayServer;
use gorgone_proto::peer::peer_server::PeerServer;
use sha2::{Digest, Sha256};
use tokio::net::TcpListener;
use tokio::task::JoinHandle;
use tokio_util::sync::CancellationToken;
use tonic::transport::server::TcpIncoming;
use tracing::{info, warn};
use uuid::Uuid;

pub use crate::config::Config;
pub use crate::error::Error;

use crate::dispatcher::{Dispatcher, DispatcherConfig};
use crate::hub::Hub;
use crate::store::Store;

/// Largest gRPC message accepted from an agent (command output included).
const MAX_AGENT_MESSAGE: usize = 64 << 20;
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

async fn serve_rest(
    listener: TcpListener,
    app: axum::Router,
    tls: Option<Arc<rustls::ServerConfig>>,
    shutdown: CancellationToken,
) -> Result<(), Error> {
    let service = app.into_make_service_with_connect_info::<SocketAddr>();
    let Some(tls) = tls else {
        return axum::serve(listener, service)
            .with_graceful_shutdown(shutdown.cancelled_owned())
            .await
            .map_err(Error::io("rest listener"));
    };
    let listener = listener.into_std().map_err(Error::io("rest listener"))?;
    let handle = axum_server::Handle::new();
    let stopper = handle.clone();
    tokio::spawn(async move {
        shutdown.cancelled().await;
        stopper.graceful_shutdown(Some(SHUTDOWN_GRACE));
    });
    let config = axum_server::tls_rustls::RustlsConfig::from_config(tls);
    axum_server::from_tcp_rustls(listener, config)
        .map_err(Error::io("rest listener"))?
        .handle(handle)
        .serve(service)
        .await
        .map_err(Error::io("rest listener"))
}

impl Server {
    /// Starts a replica: connects to the store, binds the three listeners
    /// (port 0 is allowed) and spawns the background work.
    pub async fn start(config: Config) -> Result<ServerHandle, Error> {
        config.validate()?;
        // Several TLS backends are linked in: pick one explicitly. An error
        // only means it is already installed.
        let _ = rustls::crypto::ring::default_provider().install_default();
        api::init_metrics();

        let store = Store::connect(&config.database_url).await?;
        let replica_id = config
            .replica_id
            .clone()
            .unwrap_or_else(|| Uuid::new_v4().to_string());

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

        let hub = Arc::new(Hub::new());
        let dispatcher = Arc::new(Dispatcher::new(
            store.clone(),
            Arc::clone(&hub),
            peers,
            DispatcherConfig {
                replica_id: replica_id.clone(),
                peer_addr: peer_advertise.clone(),
                default_deadline: Duration::from_secs(config.job_default_deadline_s),
                sweep_interval: config.sweep_interval(),
                retention: Duration::from_secs(config.retention_s),
            },
        ));
        // Known to the peers before the first session can be registered.
        store
            .heartbeat_replica(&replica_id, &peer_advertise)
            .await?;

        let shutdown = CancellationToken::new();
        let mut tasks = Vec::new();

        tasks.push(tokio::spawn(Arc::clone(&dispatcher).run(shutdown.clone())));

        let gateway = gateway::GatewayService(Arc::new(gateway::Gateway {
            store: store.clone(),
            hub: Arc::clone(&hub),
            dispatcher: Arc::clone(&dispatcher),
            replica_id: replica_id.clone(),
            heartbeat_interval: config.heartbeat_interval(),
            auto_enroll: config.auto_enroll,
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
                .serve_with_incoming_shutdown(TcpIncoming::from(agent), signal)
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
                .serve_with_incoming_shutdown(TcpIncoming::from(peer), signal)
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
    fn secrets_comparison() {
        assert!(secrets_match("token", "token"));
        assert!(!secrets_match("token", "token2"));
        assert!(!secrets_match("", "token"));
    }
}
