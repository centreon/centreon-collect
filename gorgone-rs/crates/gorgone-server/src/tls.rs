//! TLS material loading for the gRPC and REST listeners.

use std::path::Path;
use std::sync::Arc;

use rustls::pki_types::pem::PemObject;
use rustls::pki_types::{CertificateDer, PrivateKeyDer};
use rustls::server::WebPkiClientVerifier;
use rustls::{RootCertStore, ServerConfig};
use tonic::transport::{Certificate, ClientTlsConfig, Identity, ServerTlsConfig};

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
