//! The BBDO `TLS` extension, poller side (`broker/tls/src/connector.cc`,
//! `stream.cc`, `params.cc`): once the welcomes say both sides have it,
//! the connector is the TLS client on the same TCP connection and every
//! BBDO frame goes inside TLS (`broker/core/bbdo/stream.cc:947-1010`).
//!
//! Broker's GnuTLS uses certificates when both `public_cert` and
//! `private_key` are set, anonymous Diffie-Hellman (`+ANON-DH`) otherwise.
//! rustls has no anonymous suite: without a certificate and a key the
//! engine does not offer the extension (`auto`) or refuses to connect
//! (`yes`). The peer is verified against `ca_certificate` when set, its
//! name against `tls_hostname` only when set (`verify_peers3` /
//! `verify_peers2`); without a CA, Broker verifies nothing, nor does the
//! engine.
//!
//! The certificates are files on the poller, or inline PEM (version 5 of
//! the document, the PKI centreon-web manages): an inline value wins over
//! its path.

use std::sync::Arc;

use rustls::client::WebPkiServerVerifier;
use rustls::client::danger::{HandshakeSignatureValid, ServerCertVerified, ServerCertVerifier};
use rustls::crypto::{CryptoProvider, WebPkiSupportedAlgorithms};
use rustls::pki_types::pem::PemObject;
use rustls::pki_types::{CertificateDer, PrivateKeyDer, ServerName, UnixTime};
use rustls::{ClientConfig, DigitallySignedStruct, RootCertStore, SignatureScheme};

#[cfg(test)]
use crate::broker_conf::PrivateKeyPem;
use crate::broker_conf::{TlsConf, TlsMode};

/// Name of the extension in the welcome (`tls/src/main.cc:69`).
pub const EXTENSION: &str = "TLS";

/// What the engine announces for `conf`: the extension when it can do it.
/// `Err` when TLS is mandatory and cannot be done.
pub fn offered(conf: &TlsConf) -> Result<bool, String> {
    let certificate = conf.has_certificate();
    match conf.mode {
        TlsMode::No => Ok(false),
        _ if certificate => Ok(true),
        TlsMode::Auto => Ok(false),
        TlsMode::Yes => Err(
            "TLS to the central broker needs public_cert and private_key: anonymous TLS \
             (Broker's ANON-DH) is not supported"
                .to_owned(),
        ),
    }
}

/// Whether the extension is in force once the peer's welcome is read
/// (`stream.cc:947-982`): both sides list it. A mandatory one the peer
/// lacks refuses the connection.
pub fn negotiated(conf: &TlsConf, offered: bool, peer_extensions: &str) -> Result<bool, String> {
    let peer = peer_extensions.split(' ').any(|name| name == EXTENSION);
    if offered && peer {
        return Ok(true);
    }
    if conf.mode == TlsMode::Yes {
        return Err(format!(
            "connection refused: extension '{EXTENSION}' is set to 'yes' but the central \
             broker does not offer it"
        ));
    }
    Ok(false)
}

fn certificates(path: &str) -> Result<Vec<CertificateDer<'static>>, String> {
    let certs = CertificateDer::pem_file_iter(path)
        .map_err(|error| format!("{path}: {error}"))?
        .collect::<Result<Vec<_>, _>>()
        .map_err(|error| format!("{path}: {error}"))?;
    if certs.is_empty() {
        return Err(format!("{path}: no certificate"));
    }
    Ok(certs)
}

/// Certificates of an inline PEM (`label` names it in the errors).
fn inline_certificates(label: &str, pem: &str) -> Result<Vec<CertificateDer<'static>>, String> {
    let certs = CertificateDer::pem_slice_iter(pem.as_bytes())
        .collect::<Result<Vec<_>, _>>()
        .map_err(|error| format!("{label}: {error}"))?;
    if certs.is_empty() {
        return Err(format!("{label}: no certificate"));
    }
    Ok(certs)
}

/// The certificates of `pem` when set, else of the file `path`, with the
/// name the errors give them.
fn certificates_of(
    pem: Option<&str>,
    pem_label: &str,
    path: Option<&str>,
) -> Option<Result<(Vec<CertificateDer<'static>>, String), String>> {
    match (pem, path) {
        (Some(pem), _) => {
            Some(inline_certificates(pem_label, pem).map(|certs| (certs, pem_label.to_owned())))
        }
        (None, Some(path)) => Some(certificates(path).map(|certs| (certs, path.to_owned()))),
        (None, None) => None,
    }
}

/// The private key of `conf`: inline (decrypted) when set, else its file.
/// An error never quotes the key.
fn private_key(conf: &TlsConf) -> Result<PrivateKeyDer<'static>, String> {
    const LABEL: &str = "broker.tls.private_key_pem";
    if let Some(inline) = &conf.private_key_pem {
        let pem = inline.pem().map_err(|error| format!("{LABEL}: {error}"))?;
        return PrivateKeyDer::from_pem_slice(pem.as_bytes())
            .map_err(|_| format!("{LABEL}: not a PEM private key"));
    }
    let Some(path) = &conf.private_key else {
        return Err("TLS needs a private key".to_owned());
    };
    PrivateKeyDer::from_pem_file(path).map_err(|error| format!("{path}: {error}"))
}

fn provider() -> Arc<CryptoProvider> {
    Arc::new(rustls::crypto::ring::default_provider())
}

/// Accepts any certificate: Broker without `ca_certificate`. The
/// signatures of the handshake are still checked.
#[derive(Debug)]
struct Unverified(WebPkiSupportedAlgorithms);

impl ServerCertVerifier for Unverified {
    fn verify_server_cert(
        &self,
        _end_entity: &CertificateDer<'_>,
        _intermediates: &[CertificateDer<'_>],
        _server_name: &ServerName<'_>,
        _ocsp: &[u8],
        _now: UnixTime,
    ) -> Result<ServerCertVerified, rustls::Error> {
        Ok(ServerCertVerified::assertion())
    }

    fn verify_tls12_signature(
        &self,
        message: &[u8],
        cert: &CertificateDer<'_>,
        dss: &DigitallySignedStruct,
    ) -> Result<HandshakeSignatureValid, rustls::Error> {
        rustls::crypto::verify_tls12_signature(message, cert, dss, &self.0)
    }

    fn verify_tls13_signature(
        &self,
        message: &[u8],
        cert: &CertificateDer<'_>,
        dss: &DigitallySignedStruct,
    ) -> Result<HandshakeSignatureValid, rustls::Error> {
        rustls::crypto::verify_tls13_signature(message, cert, dss, &self.0)
    }

    fn supported_verify_schemes(&self) -> Vec<SignatureScheme> {
        self.0.supported_schemes()
    }
}

/// The chain against the CA, the name ignored: `verify_peers2`, used by
/// Broker when `tls_hostname` is not set.
#[derive(Debug)]
struct ChainOnly(Arc<WebPkiServerVerifier>);

impl ServerCertVerifier for ChainOnly {
    fn verify_server_cert(
        &self,
        end_entity: &CertificateDer<'_>,
        intermediates: &[CertificateDer<'_>],
        server_name: &ServerName<'_>,
        ocsp: &[u8],
        now: UnixTime,
    ) -> Result<ServerCertVerified, rustls::Error> {
        match self
            .0
            .verify_server_cert(end_entity, intermediates, server_name, ocsp, now)
        {
            Err(rustls::Error::InvalidCertificate(
                rustls::CertificateError::NotValidForName
                | rustls::CertificateError::NotValidForNameContext { .. },
            )) => Ok(ServerCertVerified::assertion()),
            other => other,
        }
    }

    fn verify_tls12_signature(
        &self,
        message: &[u8],
        cert: &CertificateDer<'_>,
        dss: &DigitallySignedStruct,
    ) -> Result<HandshakeSignatureValid, rustls::Error> {
        self.0.verify_tls12_signature(message, cert, dss)
    }

    fn verify_tls13_signature(
        &self,
        message: &[u8],
        cert: &CertificateDer<'_>,
        dss: &DigitallySignedStruct,
    ) -> Result<HandshakeSignatureValid, rustls::Error> {
        self.0.verify_tls13_signature(message, cert, dss)
    }

    fn supported_verify_schemes(&self) -> Vec<SignatureScheme> {
        self.0.supported_verify_schemes()
    }
}

/// The client configuration of `conf` (certificate mode) and the name to
/// present: `tls_hostname`, else the host of `address`.
pub fn client(
    conf: &TlsConf,
    address: &str,
) -> Result<(ClientConfig, ServerName<'static>), String> {
    if !conf.has_certificate() {
        return Err("TLS needs public_cert and private_key".to_owned());
    }
    let provider = provider();
    let builder = ClientConfig::builder_with_provider(provider.clone())
        .with_safe_default_protocol_versions()
        .map_err(|error| error.to_string())?;
    let ca = certificates_of(
        conf.ca_certificate_pem.as_deref(),
        "broker.tls.ca_certificate_pem",
        conf.ca_certificate.as_deref(),
    )
    .transpose()?;
    let verifier: Arc<dyn ServerCertVerifier> = match ca {
        Some((authorities, ca)) => {
            let mut roots = RootCertStore::empty();
            for cert in authorities {
                roots.add(cert).map_err(|error| format!("{ca}: {error}"))?;
            }
            let webpki =
                WebPkiServerVerifier::builder_with_provider(Arc::new(roots), provider.clone())
                    .build()
                    .map_err(|error| format!("{ca}: {error}"))?;
            if conf.tls_hostname.is_some() {
                webpki
            } else {
                Arc::new(ChainOnly(webpki))
            }
        }
        None => Arc::new(Unverified(provider.signature_verification_algorithms)),
    };
    let (chain, cert) = certificates_of(
        conf.public_cert_pem.as_deref(),
        "broker.tls.public_cert_pem",
        conf.public_cert.as_deref(),
    )
    .ok_or("TLS needs public_cert")??;
    let key = private_key(conf)?;
    let config = builder
        .dangerous()
        .with_custom_certificate_verifier(verifier)
        .with_client_auth_cert(chain, key)
        .map_err(|error| format!("{cert}: {error}"))?;
    let host = match &conf.tls_hostname {
        Some(name) => name.clone(),
        None => address
            .rsplit_once(':')
            .map_or(address, |(host, _)| host)
            .trim_start_matches('[')
            .trim_end_matches(']')
            .to_owned(),
    };
    let name = ServerName::try_from(host.clone())
        .map_err(|error| format!("invalid TLS server name '{host}': {error}"))?;
    Ok((config, name))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn conf(mode: TlsMode, certificate: bool) -> TlsConf {
        TlsConf {
            mode,
            public_cert: certificate.then(|| "c.pem".to_owned()),
            private_key: certificate.then(|| "k.pem".to_owned()),
            ..TlsConf::default()
        }
    }

    #[test]
    fn negotiates_as_broker() {
        // auto or yes on both sides: TLS.
        let auto = conf(TlsMode::Auto, true);
        assert!(offered(&auto).unwrap());
        assert!(negotiated(&auto, true, "TLS COMPRESSION").unwrap());
        // auto against a peer without it: clear text.
        assert!(!negotiated(&auto, true, "COMPRESSION").unwrap());
        assert!(!negotiated(&auto, true, "").unwrap());
        // yes against a peer without it: refused.
        let yes = conf(TlsMode::Yes, true);
        assert!(negotiated(&yes, true, "").is_err());
        // no: never offered, whatever the peer says.
        assert!(!offered(&conf(TlsMode::No, true)).unwrap());
        assert!(!negotiated(&conf(TlsMode::No, true), false, "TLS").unwrap());
        // No certificate: anonymous TLS, not offered (auto) or refused (yes).
        assert!(!offered(&conf(TlsMode::Auto, false)).unwrap());
        assert!(offered(&conf(TlsMode::Yes, false)).is_err());
        // Inline certificate and key count as a certificate.
        let inline = TlsConf {
            mode: TlsMode::Yes,
            public_cert_pem: Some("pem".into()),
            private_key_pem: Some(PrivateKeyPem::new(Ok("pem".into()))),
            ..TlsConf::default()
        };
        assert!(offered(&inline).unwrap());
    }

    fn pki() -> (
        rcgen::CertifiedIssuer<'static, rcgen::KeyPair>,
        String,
        String,
    ) {
        let mut params = rcgen::CertificateParams::new(Vec::<String>::new()).unwrap();
        params.is_ca = rcgen::IsCa::Ca(rcgen::BasicConstraints::Unconstrained);
        let ca = rcgen::CertifiedIssuer::self_signed(params, rcgen::KeyPair::generate().unwrap())
            .unwrap();
        let key = rcgen::KeyPair::generate().unwrap();
        let cert = rcgen::CertificateParams::new(vec!["poller".to_owned()])
            .unwrap()
            .signed_by(&key, &ca)
            .unwrap();
        (ca, cert.pem(), key.serialize_pem())
    }

    #[test]
    fn builds_a_client_from_inline_pem() {
        let (ca, cert, key) = pki();
        let conf = TlsConf {
            mode: TlsMode::Yes,
            ca_certificate_pem: Some(ca.pem()),
            public_cert_pem: Some(cert.clone()),
            private_key_pem: Some(PrivateKeyPem::new(Ok(key.clone()))),
            tls_hostname: Some("central".into()),
            // Paths that do not exist: the inline values win.
            ca_certificate: Some("/nonexistent/ca.pem".into()),
            public_cert: Some("/nonexistent/c.pem".into()),
            private_key: Some("/nonexistent/k.pem".into()),
        };
        let (_, name) = client(&conf, "broker:5669").unwrap();
        assert_eq!(name.to_str(), "central");
        // The key of a document that could not be decrypted: the reason,
        // never the value.
        let undecrypted = TlsConf {
            private_key_pem: Some(PrivateKeyPem::new(Err("no key to decrypt it".into()))),
            ..conf.clone()
        };
        let error = client(&undecrypted, "broker:5669").unwrap_err();
        assert!(
            error.contains("broker.tls.private_key_pem: no key to decrypt it"),
            "{error}"
        );
        let garbage = TlsConf {
            private_key_pem: Some(PrivateKeyPem::new(Ok("not a key".into()))),
            ..conf.clone()
        };
        let error = client(&garbage, "broker:5669").unwrap_err();
        assert!(error.contains("not a PEM private key"), "{error}");
        let no_cert = TlsConf {
            public_cert_pem: Some("no pem here".into()),
            ..conf.clone()
        };
        assert!(
            client(&no_cert, "broker:5669")
                .unwrap_err()
                .contains("broker.tls.public_cert_pem: no certificate")
        );
        // The key never appears in a debug print.
        assert!(!format!("{conf:?}").contains("PRIVATE KEY"));
        assert!(format!("{conf:?}").contains("PrivateKeyPem(..)"));
        let _ = key;
    }
}
