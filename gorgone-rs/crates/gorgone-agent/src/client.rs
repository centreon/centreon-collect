//! gRPC client of the server gateway: channel, authentication metadata and
//! blob download.

use std::path::Path;
use std::time::Duration;

use gorgone_proto::NODE_ID_METADATA;
use gorgone_proto::agent::FetchBlobRequest;
use gorgone_proto::agent::agent_gateway_client::AgentGatewayClient;
use tonic::metadata::{Ascii, MetadataValue};
use tonic::service::Interceptor;
use tonic::service::interceptor::InterceptedService;
use tonic::transport::{Certificate, Channel, ClientTlsConfig, Endpoint, Identity};
use tonic::{Request, Status};

use crate::AgentError;
use crate::config::AgentConfig;
use crate::executor::{BlobFetcher, FetchError};

const CONNECT_TIMEOUT: Duration = Duration::from_secs(10);
const KEEP_ALIVE_INTERVAL: Duration = Duration::from_secs(20);
const KEEP_ALIVE_TIMEOUT: Duration = Duration::from_secs(10);
/// Longest silence accepted between two chunks of a blob.
const CHUNK_TIMEOUT: Duration = Duration::from_secs(60);

pub(crate) type GatewayClient = AgentGatewayClient<InterceptedService<Channel, Auth>>;

/// Adds the node id and the bearer token to every call.
#[derive(Clone)]
pub(crate) struct Auth {
    node_id: MetadataValue<Ascii>,
    authorization: Option<MetadataValue<Ascii>>,
}

impl Interceptor for Auth {
    fn call(&mut self, mut request: Request<()>) -> Result<Request<()>, Status> {
        let metadata = request.metadata_mut();
        metadata.insert(NODE_ID_METADATA, self.node_id.clone());
        if let Some(authorization) = &self.authorization {
            metadata.insert("authorization", authorization.clone());
        }
        Ok(request)
    }
}

fn metadata_value(name: &str, value: &str) -> Result<MetadataValue<Ascii>, AgentError> {
    value
        .parse()
        .map_err(|_| AgentError::Metadata(name.to_owned()))
}

fn read(path: &Path) -> Result<Vec<u8>, AgentError> {
    std::fs::read(path).map_err(|source| AgentError::Io {
        path: path.to_path_buf(),
        source,
    })
}

/// Builds the client. The channel connects on first use and reconnects by
/// itself, so this only fails on invalid settings.
pub(crate) fn connect(config: &AgentConfig) -> Result<GatewayClient, AgentError> {
    let mut endpoint = Endpoint::from_shared(config.server_url.clone())?
        .connect_timeout(CONNECT_TIMEOUT)
        .tcp_keepalive(Some(KEEP_ALIVE_INTERVAL))
        // Detects a dead peer behind a load balancer that keeps the socket open.
        .http2_keep_alive_interval(KEEP_ALIVE_INTERVAL)
        .keep_alive_timeout(KEEP_ALIVE_TIMEOUT)
        .keep_alive_while_idle(true);

    if config.uses_tls() {
        let mut tls = ClientTlsConfig::new();
        tls = match &config.tls.ca_file {
            Some(ca_file) => tls.ca_certificate(Certificate::from_pem(read(ca_file)?)),
            #[cfg(feature = "native-roots")]
            None => tls.with_native_roots(),
            // Refused by the configuration check: no authority is trusted.
            #[cfg(not(feature = "native-roots"))]
            None => tls,
        };
        if let (Some(cert_file), Some(key_file)) = (&config.tls.cert_file, &config.tls.key_file) {
            tls = tls.identity(Identity::from_pem(read(cert_file)?, read(key_file)?));
        }
        if let Some(server_name) = &config.tls.server_name {
            tls = tls.domain_name(server_name.clone());
        }
        endpoint = endpoint.tls_config(tls)?;
    } else {
        tracing::warn!("server_url is not https: token and jobs travel in clear text");
    }

    let mut authorization = config
        .token
        .as_ref()
        .map(|token| metadata_value("token", &format!("Bearer {}", token.expose())))
        .transpose()?;
    if let Some(value) = &mut authorization {
        value.set_sensitive(true);
    }
    let auth = Auth {
        node_id: metadata_value("node_id", &config.node_id)?,
        authorization,
    };
    Ok(AgentGatewayClient::with_interceptor(
        endpoint.connect_lazy(),
        auth,
    ))
}

/// Downloads blobs with `FetchBlob`.
pub(crate) struct GrpcBlobFetcher {
    pub client: GatewayClient,
}

impl BlobFetcher for GrpcBlobFetcher {
    async fn fetch(&self, digest: &str, limit: u64) -> Result<Vec<u8>, FetchError> {
        let rpc =
            |status: Status| FetchError::Rpc(format!("{:?}: {}", status.code(), status.message()));
        let stalled = |_| FetchError::Rpc("no data received in time".to_owned());

        let request = FetchBlobRequest {
            digest: digest.to_owned(),
        };
        let mut client = self.client.clone();
        let mut chunks = tokio::time::timeout(CHUNK_TIMEOUT, client.fetch_blob(request))
            .await
            .map_err(stalled)?
            .map_err(rpc)?
            .into_inner();
        let mut blob = Vec::new();
        while let Some(chunk) = tokio::time::timeout(CHUNK_TIMEOUT, chunks.message())
            .await
            .map_err(stalled)?
            .map_err(rpc)?
        {
            if (blob.len() + chunk.data.len()) as u64 > limit {
                return Err(FetchError::TooLarge(limit));
            }
            blob.extend_from_slice(&chunk.data);
        }
        Ok(blob)
    }
}
