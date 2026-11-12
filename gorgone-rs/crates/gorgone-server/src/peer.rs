//! Replica-to-replica hints: `Peer` service and its client cache.

use std::collections::HashMap;
use std::sync::{Arc, Mutex, PoisonError};
use std::time::Duration;

use gorgone_proto::peer::peer_client::PeerClient;
use gorgone_proto::peer::peer_server::Peer;
use gorgone_proto::peer::{WakeReply, WakeRequest};
use tonic::metadata::{Ascii, MetadataValue};
use tonic::transport::{Channel, ClientTlsConfig, Endpoint};
use tonic::{Request, Response, Status};

use crate::dispatcher::Dispatcher;
use crate::error::Error;
use crate::{bearer_token, secrets_match};

/// `Peer` service: a wake-up drains the queued jobs of a node held here.
pub(crate) struct PeerService {
    pub(crate) dispatcher: Arc<Dispatcher>,
    /// Shared secret the callers must present, when configured.
    pub(crate) token: Option<String>,
}

#[tonic::async_trait]
impl Peer for PeerService {
    async fn wake(&self, request: Request<WakeRequest>) -> Result<Response<WakeReply>, Status> {
        if let Some(expected) = &self.token {
            let given = bearer_token(request.metadata());
            if !given.is_some_and(|given| secrets_match(&given, expected)) {
                return Err(Status::unauthenticated("invalid peer token"));
            }
        }
        let held = self.dispatcher.drain(&request.get_ref().node_id).await;
        Ok(Response::new(WakeReply { held }))
    }
}

/// Lazily connected clients, one per peer address.
#[derive(Debug, Default)]
pub(crate) struct PeerClients {
    clients: Mutex<HashMap<String, PeerClient<Channel>>>,
    authorization: Option<MetadataValue<Ascii>>,
    tls: Option<ClientTlsConfig>,
}

impl PeerClients {
    /// `token` is presented as a bearer; `tls` is used for `https` peers.
    pub(crate) fn new(token: Option<&str>, tls: Option<ClientTlsConfig>) -> Result<Self, Error> {
        let authorization = token
            .map(|token| {
                format!("Bearer {token}")
                    .parse::<MetadataValue<Ascii>>()
                    .map_err(|_| Error::Invalid("peer_token must be printable ASCII".into()))
            })
            .transpose()?;
        Ok(Self {
            clients: Mutex::default(),
            authorization,
            tls,
        })
    }

    fn client(&self, addr: &str) -> Result<PeerClient<Channel>, Error> {
        let mut clients = self.clients.lock().unwrap_or_else(PoisonError::into_inner);
        if let Some(client) = clients.get(addr) {
            return Ok(client.clone());
        }
        let mut endpoint = Endpoint::from_shared(addr.to_owned())?
            .connect_timeout(Duration::from_secs(2))
            .timeout(Duration::from_secs(5));
        if addr.starts_with("https://") {
            let tls = self.tls.clone().ok_or_else(|| {
                Error::Tls(format!("peer {addr} uses TLS but peer_ca is not set"))
            })?;
            endpoint = endpoint.tls_config(tls)?;
        }
        let client = PeerClient::new(endpoint.connect_lazy());
        clients.insert(addr.to_owned(), client.clone());
        Ok(client)
    }

    /// Tells the replica at `addr` that `node_id` has queued jobs. Returns
    /// whether it still holds the node.
    pub(crate) async fn wake(&self, addr: &str, node_id: &str) -> Result<bool, Error> {
        let mut client = self.client(addr)?;
        let mut request = Request::new(WakeRequest {
            node_id: node_id.to_owned(),
        });
        if let Some(authorization) = &self.authorization {
            request
                .metadata_mut()
                .insert("authorization", authorization.clone());
        }
        let reply = client
            .wake(request)
            .await
            .map_err(|status| Error::Invalid(format!("peer {addr}: {status}")))?;
        Ok(reply.into_inner().held)
    }

    /// Drops every cached channel; they are rebuilt lazily.
    pub(crate) fn clear(&self) {
        self.clients
            .lock()
            .unwrap_or_else(PoisonError::into_inner)
            .clear();
    }
}
