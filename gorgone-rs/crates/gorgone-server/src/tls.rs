//! TLS material loading for the gRPC and REST listeners.

use std::io;
use std::net::SocketAddr;
use std::path::Path;
use std::sync::Arc;
use std::time::Duration;

use rustls::pki_types::pem::PemObject;
use rustls::pki_types::{CertificateDer, PrivateKeyDer};
use rustls::server::WebPkiClientVerifier;
use rustls::{RootCertStore, ServerConfig};
use tokio::net::{TcpListener, TcpStream};
use tokio::sync::mpsc;
use tokio_rustls::TlsAcceptor;
use tokio_rustls::server::TlsStream;
use tonic::transport::{Certificate, ClientTlsConfig, Identity, ServerTlsConfig};
use tracing::{debug, warn};

use crate::config::TlsConfig;
use crate::error::Error;

fn read(path: &Path) -> Result<Vec<u8>, Error> {
    std::fs::read(path).map_err(Error::io(path.display().to_string()))
}

/// TLS settings of a gRPC listener; a client CA turns mTLS on.
pub(crate) fn grpc(config: &TlsConfig) -> Result<ServerTlsConfig, Error> {
    let identity = Identity::from_pem(read(&config.cert)?, read(&config.key)?);
    let mut tls = ServerTlsConfig::new().identity(identity);
    if let Some(ca) = &config.client_ca {
        tls = tls.client_ca_root(Certificate::from_pem(read(ca)?));
    }
    Ok(tls)
}

/// TLS settings of the clients calling the other replicas: they are verified
/// against `ca`, and get our peer certificate when they ask for mTLS.
pub(crate) fn peer_client(ca: &Path, own: Option<&TlsConfig>) -> Result<ClientTlsConfig, Error> {
    let mut tls = ClientTlsConfig::new().ca_certificate(Certificate::from_pem(read(ca)?));
    if let Some(own) = own.filter(|own| own.client_ca.is_some()) {
        tls = tls.identity(Identity::from_pem(read(&own.cert)?, read(&own.key)?));
    }
    Ok(tls)
}

fn certificates(path: &Path) -> Result<Vec<CertificateDer<'static>>, Error> {
    let invalid = |err| Error::Tls(format!("{}: {err}", path.display()));
    CertificateDer::pem_file_iter(path)
        .map_err(invalid)?
        .collect::<Result<Vec<_>, _>>()
        .map_err(invalid)
}

/// TLS settings of the REST listener.
pub(crate) fn rest(config: &TlsConfig) -> Result<Arc<ServerConfig>, Error> {
    let certs = certificates(&config.cert)?;
    let key = PrivateKeyDer::from_pem_file(&config.key)
        .map_err(|err| Error::Tls(format!("{}: {err}", config.key.display())))?;
    let builder = ServerConfig::builder();
    let builder = match &config.client_ca {
        Some(ca) => {
            let mut roots = RootCertStore::empty();
            for cert in certificates(ca)? {
                roots
                    .add(cert)
                    .map_err(|err| Error::Tls(format!("{}: {err}", ca.display())))?;
            }
            let verifier = WebPkiClientVerifier::builder(Arc::new(roots))
                .build()
                .map_err(|err| Error::Tls(err.to_string()))?;
            builder.with_client_cert_verifier(verifier)
        }
        None => builder.with_no_client_auth(),
    };
    let mut server = builder
        .with_single_cert(certs, key)
        .map_err(|err| Error::Tls(err.to_string()))?;
    server.alpn_protocols = vec![b"h2".to_vec(), b"http/1.1".to_vec()];
    Ok(Arc::new(server))
}

/// Time left to a client to complete its TLS handshake.
const HANDSHAKE_TIMEOUT: Duration = Duration::from_secs(10);
/// Established connections waiting to be served.
const READY_BACKLOG: usize = 64;

/// TCP listener handing out connections once their TLS handshake is done.
/// Each handshake runs in its own task: a slow client delays nobody.
pub(crate) struct TlsListener {
    local: SocketAddr,
    ready: mpsc::Receiver<(TlsStream<TcpStream>, SocketAddr)>,
}

impl TlsListener {
    /// Starts accepting on `listener`; stops when the value is dropped.
    pub(crate) fn new(listener: TcpListener, config: Arc<ServerConfig>) -> io::Result<Self> {
        let local = listener.local_addr()?;
        let (tx, ready) = mpsc::channel(READY_BACKLOG);
        tokio::spawn(accept_loop(listener, TlsAcceptor::from(config), tx));
        Ok(Self { local, ready })
    }
}

async fn accept_loop(
    listener: TcpListener,
    acceptor: TlsAcceptor,
    ready: mpsc::Sender<(TlsStream<TcpStream>, SocketAddr)>,
) {
    loop {
        let accepted = tokio::select! {
            () = ready.closed() => return,
            accepted = listener.accept() => accepted,
        };
        let (stream, client) = match accepted {
            Ok(accepted) => accepted,
            // The client gave up before being accepted.
            Err(err)
                if matches!(
                    err.kind(),
                    io::ErrorKind::ConnectionRefused
                        | io::ErrorKind::ConnectionAborted
                        | io::ErrorKind::ConnectionReset
                ) =>
            {
                continue;
            }
            Err(err) => {
                // Most likely out of descriptors: leave time to free some.
                warn!(error = %err, "REST accept failed");
                tokio::time::sleep(Duration::from_secs(1)).await;
                continue;
            }
        };
        let acceptor = acceptor.clone();
        let ready = ready.clone();
        tokio::spawn(async move {
            match tokio::time::timeout(HANDSHAKE_TIMEOUT, acceptor.accept(stream)).await {
                Ok(Ok(stream)) => {
                    // An error means the listener is gone: the stream is dropped.
                    let _ = ready.send((stream, client)).await;
                }
                Ok(Err(err)) => debug!(%client, error = %err, "TLS handshake failed"),
                Err(_) => debug!(%client, "TLS handshake timed out"),
            }
        });
    }
}

impl axum::serve::Listener for TlsListener {
    type Io = TlsStream<TcpStream>;
    type Addr = SocketAddr;

    async fn accept(&mut self) -> (Self::Io, Self::Addr) {
        match self.ready.recv().await {
            Some(connection) => connection,
            // The accept loop only ends once this listener is dropped.
            None => std::future::pending().await,
        }
    }

    fn local_addr(&self) -> io::Result<Self::Addr> {
        Ok(self.local)
    }
}
