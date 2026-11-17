//! The structured configuration in force, kept on disk so that the engine
//! restarts with it: one file, the digest of the applied job on the first
//! line and the document after it, replaced atomically.

use std::fs::{self, DirBuilder, File, OpenOptions};
use std::io::{self, Write};
use std::os::unix::fs::{DirBuilderExt, OpenOptionsExt};
use std::path::{Path, PathBuf};

const FILE: &str = "configuration";

/// A kept document and the digest it was applied under.
#[derive(Debug, Clone, PartialEq, Eq)]
pub(crate) struct Snapshot {
    pub digest: String,
    pub document: Vec<u8>,
}

fn path(dir: &Path) -> PathBuf {
    dir.join(FILE)
}

/// Reads the kept document; `Ok(None)` when there is none.
pub(crate) fn read(dir: &Path) -> io::Result<Option<Snapshot>> {
    let bytes = match fs::read(path(dir)) {
        Ok(bytes) => bytes,
        Err(error) if error.kind() == io::ErrorKind::NotFound => return Ok(None),
        Err(error) => return Err(error),
    };
    let Some(end) = bytes.iter().position(|b| *b == b'\n') else {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "kept configuration without digest line",
        ));
    };
    let digest = String::from_utf8_lossy(&bytes[..end]).trim().to_owned();
    Ok(Some(Snapshot {
        digest,
        document: bytes[end + 1..].to_vec(),
    }))
}

/// Replaces the kept document, durably: written aside, synced, renamed.
pub(crate) fn write(dir: &Path, digest: &str, document: &[u8]) -> io::Result<()> {
    DirBuilder::new().recursive(true).mode(0o700).create(dir)?;
    let target = path(dir);
    let tmp = target.with_extension("tmp");
    let mut file = OpenOptions::new()
        .write(true)
        .create(true)
        .truncate(true)
        .mode(0o600)
        .open(&tmp)?;
    file.write_all(digest.as_bytes())?;
    file.write_all(b"\n")?;
    file.write_all(document)?;
    file.sync_all()?;
    fs::rename(&tmp, &target)?;
    File::open(dir)?.sync_all()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn round_trip() {
        let dir = tempfile::tempdir().unwrap();
        let kept = dir.path().join("engine");
        assert_eq!(read(&kept).unwrap(), None);
        write(&kept, "abc", b"{\"version\":1}\n").unwrap();
        write(&kept, "def", b"{}").unwrap();
        assert_eq!(
            read(&kept).unwrap(),
            Some(Snapshot {
                digest: "def".into(),
                document: b"{}".to_vec()
            })
        );
        std::fs::write(kept.join(FILE), "no line").unwrap();
        assert!(read(&kept).is_err());
    }
}
