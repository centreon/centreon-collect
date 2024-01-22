fn main() {
    //let _build = cxx_build::bridge("src/multiplexing.rs");
    let _build = cxx_build::bridge("src/io.rs");

    println!("cargo:rerun-if-changed=src/io.rs");
}
