//! Blob of a structured engine configuration
//! (`gorgone-rs/schemas/engine-configuration.v1.json`): the JSON document
//! compressed with zstd. gorgone-server packs what centreon-web sends, the
//! agent unpacks it for its engine; the digest of the blob names it.

use std::io::{self, Read, Write};

/// zstd level: the one of the bundles.
const LEVEL: i32 = 3;

/// Compresses a document.
///
/// # Errors
/// An I/O error of the encoder.
pub fn pack(document: &[u8]) -> io::Result<Vec<u8>> {
    let mut encoder = zstd::Encoder::new(Vec::new(), LEVEL)?;
    encoder.write_all(document)?;
    encoder.finish()
}

/// Decompresses a blob, refusing more than `max_unpacked` bytes.
///
/// # Errors
/// An invalid blob, or one that unpacks above `max_unpacked`.
pub fn unpack(blob: &[u8], max_unpacked: u64) -> io::Result<Vec<u8>> {
    let mut document = Vec::new();
    zstd::Decoder::new(blob)?
        .take(max_unpacked.saturating_add(1))
        .read_to_end(&mut document)?;
    if document.len() as u64 > max_unpacked {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            format!("configuration document above the {max_unpacked} bytes allowed"),
        ));
    }
    Ok(document)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn round_trip_and_limit() {
        let document = br#"{"version":1}"#.repeat(100);
        let blob = pack(&document).unwrap();
        assert!(blob.len() < document.len());
        assert_eq!(pack(&document).unwrap(), blob, "same document, same digest");
        assert_eq!(unpack(&blob, 10_000).unwrap(), document);
        assert!(unpack(&blob, 100).is_err());
        assert!(unpack(b"not zstd", 100).is_err());
    }
}
