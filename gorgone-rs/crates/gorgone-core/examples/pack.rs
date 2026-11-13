//! Packs a directory (or a file) into a configuration bundle:
//!
//! ```bash
//! cargo run -q -p gorgone-core --example pack -- /var/cache/centreon/config/engine/1 > bundle.tar.zst
//! ```
//!
//! The digest is printed on stderr.

use std::io::Write;
use std::path::PathBuf;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let source: PathBuf = std::env::args_os()
        .nth(1)
        .ok_or("usage: pack <directory or file>")?
        .into();
    let bundle = gorgone_core::bundle::pack(&source)?;
    eprintln!("{}", gorgone_core::bundle::digest(&bundle));
    std::io::stdout().write_all(&bundle)?;
    Ok(())
}
