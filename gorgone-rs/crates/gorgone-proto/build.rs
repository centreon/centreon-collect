fn main() -> Result<(), Box<dyn std::error::Error>> {
    // No system protoc required: use the vendored binary unless one is given.
    if std::env::var_os("PROTOC").is_none() {
        let protoc = protoc_bin_vendored::protoc_bin_path()?;
        // SAFETY: build scripts are single-threaded at this point.
        unsafe { std::env::set_var("PROTOC", protoc) };
    }
    let root = "../../proto";
    let protos = [
        format!("{root}/gorgone/agent/v1/agent.proto"),
        format!("{root}/gorgone/peer/v1/peer.proto"),
    ];
    for proto in &protos {
        println!("cargo:rerun-if-changed={proto}");
    }
    tonic_prost_build::configure().compile_protos(&protos, &[root.to_string()])?;
    Ok(())
}
