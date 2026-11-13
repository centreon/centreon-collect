//! gRPC contracts of Gorgone: agent gateway and replica peer service.
//!
//! The code under `generated/` is checked in so that building needs neither
//! `protoc` nor a code generator. Regenerate it with `tools/protogen` after
//! changing a file under `proto/`.

#![allow(clippy::all, missing_docs)]

/// `gorgone.agent.v1`: control stream and blob transfer between server and agents.
pub mod agent {
    include!("generated/gorgone.agent.v1.rs");
}

/// `gorgone.peer.v1`: hints exchanged between server replicas.
pub mod peer {
    include!("generated/gorgone.peer.v1.rs");
}

/// Metadata key carrying the node id on agent calls.
pub const NODE_ID_METADATA: &str = "x-gorgone-node-id";
