//! Configuration bundles: deterministic `tar.zst` archives addressed by their
//! SHA-256, applied file by file with atomic renames.

use std::collections::BTreeSet;
use std::fs;
use std::io::{self, Read, Write};
use std::os::unix::fs::PermissionsExt;
use std::path::{Component, Path, PathBuf};

use sha2::{Digest, Sha256};

/// Default cap on the unpacked size of a bundle.
pub const DEFAULT_MAX_UNPACKED: u64 = 1 << 30;

/// Bundle errors.
#[derive(Debug, thiserror::Error)]
pub enum BundleError {
    /// I/O failure, with the path involved.
    #[error("{path}: {source}")]
    Io {
        /// Path being read or written.
        path: PathBuf,
        /// Underlying error.
        source: io::Error,
    },
    /// Archive could not be decoded.
    #[error("invalid bundle: {0}")]
    Invalid(io::Error),
    /// Entry that must never be unpacked (link, device, absolute or `..` path).
    #[error("unsafe bundle entry: {0}")]
    UnsafeEntry(String),
    /// Bundle bytes do not match the announced digest.
    #[error("digest mismatch: expected {expected}, got {actual}")]
    DigestMismatch {
        /// Digest announced by the server.
        expected: String,
        /// Digest computed on the received bytes.
        actual: String,
    },
    /// Unpacked size above the configured cap.
    #[error("bundle larger than {0} bytes once unpacked")]
    TooLarge(u64),
}

fn io_err(path: &Path) -> impl FnOnce(io::Error) -> BundleError + '_ {
    move |source| BundleError::Io {
        path: path.to_path_buf(),
        source,
    }
}

/// Lowercase hex SHA-256 of `bytes`.
#[must_use]
pub fn digest(bytes: &[u8]) -> String {
    hex::encode(Sha256::digest(bytes))
}

/// Packs the regular files under `src` (a directory, or a single file) into a
/// bundle. The output only depends on relative paths, modes and contents, so
/// an unchanged tree always gives the same digest.
pub fn pack(src: &Path) -> Result<Vec<u8>, BundleError> {
    let mut files = Vec::new();
    // Never follow a link planted in place of the source.
    let meta = fs::symlink_metadata(src).map_err(io_err(src))?;
    if meta.file_type().is_symlink() {
        return Err(BundleError::UnsafeEntry(src.display().to_string()));
    }
    if meta.is_file() {
        let name = src
            .file_name()
            .ok_or_else(|| BundleError::UnsafeEntry(src.display().to_string()))?;
        files.push((PathBuf::from(name), src.to_path_buf()));
    } else {
        collect(src, Path::new(""), &mut files)?;
    }
    files.sort();

    let encoder = zstd::Encoder::new(Vec::new(), 3).map_err(BundleError::Invalid)?;
    let mut tar = tar::Builder::new(encoder);
    for (rel, abs) in &files {
        let data = fs::read(abs).map_err(io_err(abs))?;
        let mode = fs::metadata(abs).map_err(io_err(abs))?.permissions().mode() & 0o777;
        let mut header = tar::Header::new_gnu();
        header.set_entry_type(tar::EntryType::Regular);
        header.set_size(data.len() as u64);
        header.set_mode(mode);
        header.set_mtime(0);
        header.set_uid(0);
        header.set_gid(0);
        tar.append_data(&mut header, rel, data.as_slice())
            .map_err(io_err(abs))?;
    }
    tar.into_inner()
        .and_then(zstd::Encoder::finish)
        .map_err(BundleError::Invalid)
}

fn collect(root: &Path, rel: &Path, out: &mut Vec<(PathBuf, PathBuf)>) -> Result<(), BundleError> {
    let dir = root.join(rel);
    for entry in fs::read_dir(&dir).map_err(io_err(&dir))? {
        let entry = entry.map_err(io_err(&dir))?;
        let kind = entry.file_type().map_err(io_err(&dir))?;
        let child = rel.join(entry.file_name());
        if kind.is_dir() {
            collect(root, &child, out)?;
        } else if kind.is_file() {
            out.push((child, entry.path()));
        }
        // Links and special files are never shipped.
    }
    Ok(())
}

/// Checks that `bundle` is a readable archive holding only safe entries,
/// without writing anything. Returns the number of files and their total size.
pub fn verify(bundle: &[u8], max_unpacked: u64) -> Result<(usize, u64), BundleError> {
    let decoder = zstd::Decoder::new(bundle).map_err(BundleError::Invalid)?;
    let mut archive = tar::Archive::new(decoder);
    let (mut files, mut total) = (0, 0u64);
    for entry in archive.entries().map_err(BundleError::Invalid)? {
        let mut entry = entry.map_err(BundleError::Invalid)?;
        let rel = entry.path().map_err(BundleError::Invalid)?.into_owned();
        let kind = entry.header().entry_type();
        if !is_safe(&rel) || !(kind.is_dir() || kind.is_file()) {
            return Err(BundleError::UnsafeEntry(rel.display().to_string()));
        }
        if kind.is_file() {
            // Count real bytes: the header size is attacker-controlled.
            total += io::copy(
                &mut (&mut entry).take(max_unpacked - total + 1),
                &mut io::sink(),
            )
            .map_err(BundleError::Invalid)?;
            if total > max_unpacked {
                return Err(BundleError::TooLarge(max_unpacked));
            }
            files += 1;
        }
    }
    Ok((files, total))
}

/// How [`apply`] writes a bundle.
#[derive(Debug, Clone)]
pub struct ApplyOptions<'a> {
    /// Name of the marker file recording the applied digest, inside `dest`.
    pub marker: &'a str,
    /// Remove regular files of `dest` that are not in the bundle.
    pub prune: bool,
    /// Cap on the unpacked size.
    pub max_unpacked: u64,
}

/// Result of [`apply`].
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct ApplyReport {
    /// Files written.
    pub written: usize,
    /// Stale files removed.
    pub pruned: usize,
    /// True when the digest was already applied and nothing was touched.
    pub unchanged: bool,
}

/// Digest recorded by the last successful [`apply`] into `dest`, if any.
#[must_use]
pub fn applied_digest(dest: &Path, marker: &str) -> Option<String> {
    fs::read_to_string(dest.join(marker))
        .ok()
        .map(|s| s.trim().to_owned())
}

/// Verifies `bundle` against `expected_digest` and syncs it into `dest`.
///
/// Every file is written next to its destination and renamed over it, so a
/// reader never sees a partial file. Works on mount points and shared
/// volumes, unlike a directory swap.
pub fn apply(
    bundle: &[u8],
    expected_digest: &str,
    dest: &Path,
    options: &ApplyOptions<'_>,
) -> Result<ApplyReport, BundleError> {
    let actual = digest(bundle);
    if !actual.eq_ignore_ascii_case(expected_digest) {
        return Err(BundleError::DigestMismatch {
            expected: expected_digest.to_owned(),
            actual,
        });
    }
    if applied_digest(dest, options.marker).as_deref() == Some(actual.as_str()) {
        return Ok(ApplyReport {
            written: 0,
            pruned: 0,
            unchanged: true,
        });
    }

    fs::create_dir_all(dest).map_err(io_err(dest))?;
    let decoder = zstd::Decoder::new(bundle).map_err(BundleError::Invalid)?;
    let mut archive = tar::Archive::new(decoder);
    let mut kept = BTreeSet::new();
    let mut budget = options.max_unpacked;

    for entry in archive.entries().map_err(BundleError::Invalid)? {
        let mut entry = entry.map_err(BundleError::Invalid)?;
        let rel = entry.path().map_err(BundleError::Invalid)?.into_owned();
        let kind = entry.header().entry_type();
        if !is_safe(&rel) || is_marker(&rel) || crosses_link(dest, &rel) {
            return Err(BundleError::UnsafeEntry(rel.display().to_string()));
        }
        if kind.is_dir() {
            let dir = dest.join(&rel);
            fs::create_dir_all(&dir).map_err(io_err(&dir))?;
            continue;
        }
        if !kind.is_file() {
            return Err(BundleError::UnsafeEntry(rel.display().to_string()));
        }

        let target = dest.join(&rel);
        let parent = target.parent().unwrap_or(dest);
        fs::create_dir_all(parent).map_err(io_err(parent))?;
        let mut tmp = tempfile::NamedTempFile::new_in(parent).map_err(io_err(parent))?;
        let copied =
            io::copy(&mut (&mut entry).take(budget + 1), &mut tmp).map_err(io_err(&target))?;
        if copied > budget {
            return Err(BundleError::TooLarge(options.max_unpacked));
        }
        budget -= copied;
        // Only the executable bit is taken from the bundle.
        let executable = entry.header().mode().unwrap_or(0) & 0o111 != 0;
        let mode = if executable { 0o755 } else { 0o644 };
        tmp.as_file()
            .set_permissions(fs::Permissions::from_mode(mode))
            .map_err(io_err(&target))?;
        tmp.persist(&target).map_err(|e| io_err(&target)(e.error))?;
        kept.insert(rel);
    }

    let pruned = if options.prune {
        prune(dest, Path::new(""), &kept, options.marker)?
    } else {
        0
    };

    let mut marker = tempfile::NamedTempFile::new_in(dest).map_err(io_err(dest))?;
    marker
        .write_all(actual.as_bytes())
        .and_then(|()| {
            marker
                .as_file()
                .set_permissions(fs::Permissions::from_mode(0o644))
        })
        .map_err(io_err(dest))?;
    marker
        .persist(dest.join(options.marker))
        .map_err(|e| io_err(dest)(e.error))?;

    Ok(ApplyReport {
        written: kept.len(),
        pruned,
        unchanged: false,
    })
}

fn is_safe(rel: &Path) -> bool {
    let mut any = false;
    for component in rel.components() {
        match component {
            Component::Normal(_) => any = true,
            Component::CurDir => {}
            _ => return false,
        }
    }
    any
}

/// True for names reserved to the applied-digest markers.
fn is_marker(rel: &Path) -> bool {
    rel.file_name()
        .and_then(|name| name.to_str())
        .is_some_and(|name| name.starts_with(".gorgone-") && name.ends_with(".digest"))
}

/// True when an existing parent directory of `dest/rel` is a symbolic link,
/// which would make the entry land outside `dest`.
fn crosses_link(dest: &Path, rel: &Path) -> bool {
    // The last component is replaced by a rename, never followed.
    let Some(parent) = rel.parent() else {
        return false;
    };
    let mut current = dest.to_path_buf();
    parent.components().any(|component| {
        current.push(component);
        fs::symlink_metadata(&current).is_ok_and(|m| m.file_type().is_symlink())
    })
}

fn prune(
    dest: &Path,
    rel: &Path,
    kept: &BTreeSet<PathBuf>,
    marker: &str,
) -> Result<usize, BundleError> {
    let dir = dest.join(rel);
    let mut removed = 0;
    for entry in fs::read_dir(&dir).map_err(io_err(&dir))? {
        let entry = entry.map_err(io_err(&dir))?;
        let kind = entry.file_type().map_err(io_err(&dir))?;
        let child = rel.join(entry.file_name());
        if kind.is_dir() {
            removed += prune(dest, &child, kept, marker)?;
        } else if kind.is_file() && !kept.contains(&child) && child != Path::new(marker) {
            let path = entry.path();
            fs::remove_file(&path).map_err(io_err(&path))?;
            removed += 1;
        }
    }
    Ok(removed)
}

#[cfg(test)]
mod tests {
    use super::*;

    const OPTIONS: ApplyOptions<'static> = ApplyOptions {
        marker: ".gorgone-engine.digest",
        prune: true,
        max_unpacked: DEFAULT_MAX_UNPACKED,
    };

    fn tree() -> tempfile::TempDir {
        let dir = tempfile::tempdir().unwrap();
        fs::write(dir.path().join("centengine.cfg"), "a=1\n").unwrap();
        fs::create_dir(dir.path().join("objects")).unwrap();
        fs::write(dir.path().join("objects/hosts.cfg"), "define host{}\n").unwrap();
        dir
    }

    #[test]
    fn pack_is_deterministic() {
        let src = tree();
        assert_eq!(
            digest(&pack(src.path()).unwrap()),
            digest(&pack(src.path()).unwrap())
        );
    }

    #[test]
    fn apply_writes_prunes_and_short_circuits() {
        let src = tree();
        let bundle = pack(src.path()).unwrap();
        let sum = digest(&bundle);
        let dest = tempfile::tempdir().unwrap();
        fs::write(dest.path().join("stale.cfg"), "old").unwrap();

        let report = apply(&bundle, &sum, dest.path(), &OPTIONS).unwrap();
        assert_eq!(
            (report.written, report.pruned, report.unchanged),
            (2, 1, false)
        );
        assert_eq!(
            fs::read_to_string(dest.path().join("objects/hosts.cfg")).unwrap(),
            "define host{}\n"
        );
        assert!(!dest.path().join("stale.cfg").exists());

        let again = apply(&bundle, &sum, dest.path(), &OPTIONS).unwrap();
        assert!(again.unchanged);
    }

    #[test]
    fn single_file_bundle_without_prune() {
        let src = tree();
        let bundle = pack(&src.path().join("centengine.cfg")).unwrap();
        let dest = tempfile::tempdir().unwrap();
        fs::write(dest.path().join("other.json"), "{}").unwrap();
        let options = ApplyOptions {
            prune: false,
            ..OPTIONS
        };
        let report = apply(&bundle, &digest(&bundle), dest.path(), &options).unwrap();
        assert_eq!((report.written, report.pruned), (1, 0));
        assert!(dest.path().join("other.json").exists());
    }

    #[test]
    fn refuses_linked_directories_markers_and_linked_sources() {
        let src = tree();
        let bundle = pack(src.path()).unwrap();
        let outside = tempfile::tempdir().unwrap();
        let dest = tempfile::tempdir().unwrap();
        std::os::unix::fs::symlink(outside.path(), dest.path().join("objects")).unwrap();
        let err = apply(&bundle, &digest(&bundle), dest.path(), &OPTIONS).unwrap_err();
        assert!(matches!(err, BundleError::UnsafeEntry(_)));
        assert!(!outside.path().join("hosts.cfg").exists());

        fs::write(src.path().join(".gorgone-engine.digest"), "x").unwrap();
        let bundle = pack(src.path()).unwrap();
        let dest = tempfile::tempdir().unwrap();
        let err = apply(&bundle, &digest(&bundle), dest.path(), &OPTIONS).unwrap_err();
        assert!(matches!(err, BundleError::UnsafeEntry(_)));

        let link = outside.path().join("link");
        std::os::unix::fs::symlink(src.path(), &link).unwrap();
        assert!(matches!(pack(&link), Err(BundleError::UnsafeEntry(_))));
    }

    #[test]
    fn file_modes_are_normalised() {
        let src = tree();
        let script = src.path().join("hook.sh");
        fs::write(&script, "#!/bin/sh\n").unwrap();
        fs::set_permissions(&script, fs::Permissions::from_mode(0o777)).unwrap();
        fs::set_permissions(
            src.path().join("centengine.cfg"),
            fs::Permissions::from_mode(0o666),
        )
        .unwrap();
        let bundle = pack(src.path()).unwrap();
        let dest = tempfile::tempdir().unwrap();
        apply(&bundle, &digest(&bundle), dest.path(), &OPTIONS).unwrap();
        let mode = |name: &str| {
            fs::metadata(dest.path().join(name))
                .unwrap()
                .permissions()
                .mode()
                & 0o777
        };
        assert_eq!((mode("hook.sh"), mode("centengine.cfg")), (0o755, 0o644));
    }

    #[test]
    fn verify_counts_files() {
        let src = tree();
        let bundle = pack(src.path()).unwrap();
        assert_eq!(verify(&bundle, DEFAULT_MAX_UNPACKED).unwrap(), (2, 18));
        assert!(matches!(verify(&bundle, 3), Err(BundleError::TooLarge(3))));
        assert!(matches!(
            verify(b"not a bundle", 10),
            Err(BundleError::Invalid(_))
        ));
    }

    #[test]
    fn rejects_wrong_digest() {
        let src = tree();
        let bundle = pack(src.path()).unwrap();
        let dest = tempfile::tempdir().unwrap();
        let err = apply(&bundle, &"0".repeat(64), dest.path(), &OPTIONS).unwrap_err();
        assert!(matches!(err, BundleError::DigestMismatch { .. }));
    }

    #[test]
    fn rejects_path_traversal() {
        let encoder = zstd::Encoder::new(Vec::new(), 3).unwrap();
        let mut tar = tar::Builder::new(encoder);
        let mut header = tar::Header::new_gnu();
        header.set_entry_type(tar::EntryType::Regular);
        header.set_size(1);
        header.set_mode(0o644);
        // `append_data` refuses `..`, so write the raw name.
        header.as_gnu_mut().unwrap().name[..9].copy_from_slice(b"../escape");
        header.set_cksum();
        tar.append(&header, &b"x"[..]).unwrap();
        let bundle = tar.into_inner().unwrap().finish().unwrap();

        let dest = tempfile::tempdir().unwrap();
        let err = apply(&bundle, &digest(&bundle), dest.path(), &OPTIONS).unwrap_err();
        assert!(matches!(err, BundleError::UnsafeEntry(_)));
    }

    #[test]
    fn rejects_oversized_bundle() {
        let src = tree();
        let bundle = pack(src.path()).unwrap();
        let dest = tempfile::tempdir().unwrap();
        let options = ApplyOptions {
            max_unpacked: 3,
            ..OPTIONS
        };
        let err = apply(&bundle, &digest(&bundle), dest.path(), &options).unwrap_err();
        assert!(matches!(err, BundleError::TooLarge(3)));
    }
}
