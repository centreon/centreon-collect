//! gRPC contracts of Gorgone: agent gateway and replica peer service.

#![allow(clippy::all, missing_docs)]

/// `gorgone.agent.v1`: control stream and blob transfer between server and agents.
pub mod agent {
    tonic::include_proto!("gorgone.agent.v1");
}

/// `gorgone.peer.v1`: hints exchanged between server replicas.
pub mod peer {
    tonic::include_proto!("gorgone.peer.v1");
}

/// Metadata key carrying the node id on `FetchBlob` calls.
pub const NODE_ID_METADATA: &str = "x-gorgone-node-id";
