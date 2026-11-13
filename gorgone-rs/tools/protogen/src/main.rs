//! Regenerates the gRPC code checked in under `crates/gorgone-proto`:
//!
//! ```bash
//! cargo run --manifest-path tools/protogen/Cargo.toml
//! ```
//!
//! Run it from `gorgone-rs/` after changing a file under `proto/`.

use std::path::Path;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("../..");
    let proto = root.join("proto");
    let out = root.join("crates/gorgone-proto/src/generated");
    if std::env::var_os("PROTOC").is_none() {
        let protoc = protoc_bin_vendored::protoc_bin_path()?;
        // SAFETY: nothing else runs in this process yet.
        unsafe { std::env::set_var("PROTOC", protoc) };
    }
    std::fs::create_dir_all(&out)?;
    tonic_prost_build::configure()
        .out_dir(&out)
        .emit_rerun_if_changed(false)
        .compile_protos(
            &[
                proto.join("gorgone/agent/v1/agent.proto"),
                proto.join("gorgone/peer/v1/peer.proto"),
            ],
            &[proto],
        )?;
    Ok(())
}
