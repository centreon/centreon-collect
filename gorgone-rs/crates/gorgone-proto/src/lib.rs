//! gRPC contracts of Gorgone: agent gateway and replica peer service, and the
//! part of the Centreon Engine API the agent calls.
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

/// `com.centreon.engine`: the calls of the Centreon Engine API used by the
/// agent, from the mirror `proto/com/centreon/engine/engine.proto`.
pub mod engine {
    include!("generated/com.centreon.engine.rs");
}

/// Metadata key carrying the node id on agent calls.
pub const NODE_ID_METADATA: &str = "x-gorgone-node-id";
