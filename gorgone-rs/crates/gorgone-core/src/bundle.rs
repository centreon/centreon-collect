//! Configuration bundles: deterministic `tar.zst` archives addressed by their
//! SHA-256, applied file by file with atomic renames.

use std::collections::BTreeSet;
use std::fmt;
use std::fs;
use std::io::{self, Read, Write};
use std::os::unix::fs::{OpenOptionsExt, PermissionsExt};
use std::path::{Component, Path, PathBuf};
use std::sync::atomic::{AtomicU64, Ordering};

use sha2::{Digest, Sha256};

/// Name prefix of the directories where a bundle is unpacked aside to be
/// checked before it is applied. Pruning never enters them.
pub const STAGING_PREFIX: &str = ".gorgone-staging-";

/// Default cap on the unpacked size of a bundle.
pub const DEFAULT_MAX_UNPACKED: u64 = 1 << 30;

/// Bundle errors.
#[derive(Debug)]
pub enum BundleError {
    /// I/O failure, with the path involved.
    Io {
        /// Path being read or written.
        path: PathBuf,
        /// Underlying error.
        source: io::Error,
    },
    /// Archive could not be decoded.
    Invalid(io::Error),
    /// Entry that must never be unpacked (link, device, absolute or `..` path).
    UnsafeEntry(String),
    /// Bundle bytes do not match the announced digest.
    DigestMismatch {
        /// Digest announced by the server.
        expected: String,
        /// Digest computed on the received bytes.
        actual: String,
    },
    /// Unpacked size above the configured cap.
    TooLarge(u64),
}

impl fmt::Display for BundleError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Io { path, source } => write!(f, "{}: {source}", path.display()),
            Self::Invalid(source) => write!(f, "invalid bundle: {source}"),
            Self::UnsafeEntry(entry) => write!(f, "unsafe bundle entry: {entry}"),
            Self::DigestMismatch { expected, actual } => {
                write!(f, "digest mismatch: expected {expected}, got {actual}")
            }
            Self::TooLarge(max) => write!(f, "bundle larger than {max} bytes once unpacked"),
        }
    }
}

impl std::error::Error for BundleError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            Self::Io { source, .. } | Self::Invalid(source) => Some(source),
            _ => None,
        }
    }
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
    use fmt::Write as _;
    Sha256::digest(bytes)
        .iter()
        .fold(String::with_capacity(64), |mut hex, byte| {
            // Writing to a String cannot fail.
            let _ = write!(hex, "{byte:02x}");
            hex
        })
}

/// Writes `target` through a sibling temporary file renamed over it, so a
/// reader never sees a partial file.
fn write_atomic<T>(
    target: &Path,
    mode: u32,
    fill: impl FnOnce(&mut fs::File) -> Result<T, BundleError>,
) -> Result<T, BundleError> {
    static COUNTER: AtomicU64 = AtomicU64::new(0);
    let parent = target.parent().unwrap_or(Path::new("."));
    let tmp = parent.join(format!(
        ".gorgone-tmp-{}-{}",
        std::process::id(),
        COUNTER.fetch_add(1, Ordering::Relaxed)
    ));
    // Left over by a crashed run with the same pid.
    let _ = fs::remove_file(&tmp);
    // `create_new` never follows a link nor reuses an existing file.
    let mut file = fs::OpenOptions::new()
        .write(true)
        .create_new(true)
        .mode(mode)
        .open(&tmp)
        .map_err(io_err(&tmp))?;
    let result = fill(&mut file).and_then(|value| {
        // The mode given at creation is reduced by the umask.
        file.set_permissions(fs::Permissions::from_mode(mode))
            .and_then(|()| fs::rename(&tmp, target))
            .map_err(io_err(target))?;
        Ok(value)
    });
    if result.is_err() {
        let _ = fs::remove_file(&tmp);
    }
    result
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
    let mut entries = Vec::with_capacity(files.len());
    for (rel, abs) in files {
        let data = fs::read(&abs).map_err(io_err(&abs))?;
        let mode = fs::metadata(&abs)
            .map_err(io_err(&abs))?
            .permissions()
            .mode();
        entries.push((rel, mode, data));
    }
    build(entries)
}

/// Turns a plain `tar` archive, as an API client without `zstd` can produce,
/// into a bundle. Only regular files are kept; anything unsafe is refused.
pub fn from_tar(archive: &[u8], max_unpacked: u64) -> Result<Vec<u8>, BundleError> {
    let mut entries = Vec::new();
    let mut total = 0u64;
    let mut tar = tar::Archive::new(archive);
    for entry in tar.entries().map_err(BundleError::Invalid)? {
        let mut entry = entry.map_err(BundleError::Invalid)?;
        let path = entry.path().map_err(BundleError::Invalid)?.into_owned();
        let kind = entry.header().entry_type();
        if kind.is_dir() {
            continue;
        }
        if !kind.is_file() || !is_safe(&path) || is_marker(&path) {
            return Err(BundleError::UnsafeEntry(path.display().to_string()));
        }
        // `./name` and `name` are the same file.
        let rel: PathBuf = path
            .components()
            .filter(|c| matches!(c, Component::Normal(_)))
            .collect();
        let mode = entry.header().mode().unwrap_or(0o644);
        let mut data = Vec::new();
        (&mut entry)
            .take(max_unpacked - total + 1)
            .read_to_end(&mut data)
            .map_err(BundleError::Invalid)?;
        total += data.len() as u64;
        if total > max_unpacked {
            return Err(BundleError::TooLarge(max_unpacked));
        }
        entries.push((rel, mode, data));
    }
    build(entries)
}

/// Turns a bundle back into a plain `tar` archive, for a destination that
/// takes no `zstd`: the inverse of [`from_tar`], with the same checks. Entries
/// keep the order, the names and the permission bits of the bundle.
pub fn to_tar(bundle: &[u8], max_unpacked: u64) -> Result<Vec<u8>, BundleError> {
    let decoder = zstd::Decoder::new(bundle).map_err(BundleError::Invalid)?;
    let mut archive = tar::Archive::new(decoder);
    let mut plain = tar::Builder::new(Vec::new());
    let mut total = 0u64;
    for entry in archive.entries().map_err(BundleError::Invalid)? {
        let mut entry = entry.map_err(BundleError::Invalid)?;
        let path = entry.path().map_err(BundleError::Invalid)?.into_owned();
        let kind = entry.header().entry_type();
        if kind.is_dir() {
            continue;
        }
        if !kind.is_file() || !is_safe(&path) || is_marker(&path) {
            return Err(BundleError::UnsafeEntry(path.display().to_string()));
        }
        let rel: PathBuf = path
            .components()
            .filter(|c| matches!(c, Component::Normal(_)))
            .collect();
        let mode = entry.header().mode().unwrap_or(0o644);
        let mut data = Vec::new();
        // Real bytes, not the header size, count against the cap.
        (&mut entry)
            .take(max_unpacked - total + 1)
            .read_to_end(&mut data)
            .map_err(BundleError::Invalid)?;
        total += data.len() as u64;
        if total > max_unpacked {
            return Err(BundleError::TooLarge(max_unpacked));
        }
        plain
            .append_data(&mut entry_header(data.len(), mode), &rel, data.as_slice())
            .map_err(BundleError::Invalid)?;
    }
    plain.into_inner().map_err(BundleError::Invalid)
}

/// Header of a regular file, without time nor owner.
fn entry_header(size: usize, mode: u32) -> tar::Header {
    let mut header = tar::Header::new_gnu();
    header.set_entry_type(tar::EntryType::Regular);
    header.set_size(size as u64);
    header.set_mode(mode & 0o777);
    header.set_mtime(0);
    header.set_uid(0);
    header.set_gid(0);
    header
}

/// Writes the deterministic archive: sorted names, no time, no owner.
fn build(mut entries: Vec<(PathBuf, u32, Vec<u8>)>) -> Result<Vec<u8>, BundleError> {
    entries.sort();
    let encoder = zstd::Encoder::new(Vec::new(), 3).map_err(BundleError::Invalid)?;
    let mut tar = tar::Builder::new(encoder);
    for (rel, mode, data) in &entries {
        tar.append_data(&mut entry_header(data.len(), *mode), rel, data.as_slice())
            .map_err(BundleError::Invalid)?;
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
/// volumes, unlike a directory swap. A file gets 0644, or 0755 when it is
/// executable in the bundle.
pub fn apply(
    bundle: &[u8],
    expected_digest: &str,
    dest: &Path,
    options: &ApplyOptions<'_>,
) -> Result<ApplyReport, BundleError> {
    apply_with_file_mode(bundle, expected_digest, dest, options, None)
}

/// [`apply`], every file of the bundle written with `file_mode` when it is
/// set: only its permission bits (`0o777`) are used. The marker keeps 0644.
pub fn apply_with_file_mode(
    bundle: &[u8],
    expected_digest: &str,
    dest: &Path,
    options: &ApplyOptions<'_>,
    file_mode: Option<u32>,
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
        // Only the executable bit is taken from the bundle.
        let executable = entry.header().mode().unwrap_or(0) & 0o111 != 0;
        let mode = match file_mode {
            Some(mode) => mode & 0o777,
            None if executable => 0o755,
            None => 0o644,
        };
        budget -= write_atomic(&target, mode, |file| {
            let copied =
                io::copy(&mut (&mut entry).take(budget + 1), file).map_err(io_err(&target))?;
            if copied > budget {
                return Err(BundleError::TooLarge(options.max_unpacked));
            }
            Ok(copied)
        })?;
        kept.insert(rel);
    }

    let manifest = manifest_name(options.marker);
    let pruned = if options.prune {
        let mut protected = kept.clone();
        protected.extend(files_of_other_targets(dest, &manifest)?);
        prune(dest, Path::new(""), &protected)?
    } else {
        0
    };

    // Written before the marker: a target counts as applied only once the
    // other targets of the directory can see which files are its own.
    write_atomic(&dest.join(&manifest), 0o644, |file| {
        for rel in &kept {
            let line = rel
                .to_str()
                .ok_or_else(|| BundleError::UnsafeEntry(rel.display().to_string()))?;
            writeln!(file, "{line}").map_err(io_err(dest))?;
        }
        Ok(())
    })?;
    write_atomic(&dest.join(options.marker), 0o644, |file| {
        file.write_all(actual.as_bytes()).map_err(io_err(dest))
    })?;

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

/// True for names reserved to the bookkeeping of the targets: the
/// applied-digest markers and the file manifests.
fn is_marker(rel: &Path) -> bool {
    rel.file_name()
        .and_then(|name| name.to_str())
        .is_some_and(|name| {
            name.starts_with(".gorgone-") && (name.ends_with(".digest") || name.ends_with(".files"))
        })
}

/// Name of the manifest listing the files a target wrote, next to its marker.
fn manifest_name(marker: &str) -> String {
    match marker.strip_suffix(".digest") {
        Some(stem) => format!("{stem}.files"),
        None => format!("{marker}.files"),
    }
}

/// Files that the other targets applied to `dest` wrote, from their
/// manifests: pruning one target must never remove them.
fn files_of_other_targets(dest: &Path, own: &str) -> Result<BTreeSet<PathBuf>, BundleError> {
    let mut files = BTreeSet::new();
    for entry in fs::read_dir(dest).map_err(io_err(dest))? {
        let entry = entry.map_err(io_err(dest))?;
        let name = entry.file_name();
        let Some(name) = name.to_str() else { continue };
        if name == own || !name.starts_with(".gorgone-") || !name.ends_with(".files") {
            continue;
        }
        if !entry.file_type().map_err(io_err(dest))?.is_file() {
            continue;
        }
        let path = entry.path();
        let list = fs::read_to_string(&path).map_err(io_err(&path))?;
        files.extend(list.lines().filter(|l| !l.is_empty()).map(PathBuf::from));
    }
    Ok(files)
}

/// True when `dir` is the root of a target, other than the one being pruned.
fn holds_another_target(dir: &Path) -> Result<bool, BundleError> {
    for entry in fs::read_dir(dir).map_err(io_err(dir))? {
        let entry = entry.map_err(io_err(dir))?;
        if is_marker(Path::new(&entry.file_name())) {
            return Ok(true);
        }
    }
    Ok(false)
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

fn prune(dest: &Path, rel: &Path, kept: &BTreeSet<PathBuf>) -> Result<usize, BundleError> {
    let dir = dest.join(rel);
    let mut removed = 0;
    for entry in fs::read_dir(&dir).map_err(io_err(&dir))? {
        let entry = entry.map_err(io_err(&dir))?;
        let kind = entry.file_type().map_err(io_err(&dir))?;
        let child = rel.join(entry.file_name());
        if kind.is_dir() {
            // Another job may be checking a bundle in there.
            let staging = entry
                .file_name()
                .to_str()
                .is_some_and(|name| name.starts_with(STAGING_PREFIX));
            if !staging && !holds_another_target(&entry.path())? {
                removed += prune(dest, &child, kept)?;
            }
        } else if kind.is_file() && !kept.contains(&child) && !is_marker(&child) {
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
    fn prune_spares_files_of_other_targets() {
        // The key of the engine_context target sits in the engine directory.
        let dest = tempfile::tempdir().unwrap();
        let key_dir = tempfile::tempdir().unwrap();
        fs::write(key_dir.path().join("engine-context.json"), "{}").unwrap();
        let key = pack(key_dir.path()).unwrap();
        let key_options = ApplyOptions {
            marker: ".gorgone-engine_context.digest",
            prune: false,
            ..OPTIONS
        };
        apply(&key, &digest(&key), dest.path(), &key_options).unwrap();
        assert_eq!(
            fs::read_to_string(dest.path().join(".gorgone-engine_context.files")).unwrap(),
            "engine-context.json\n"
        );

        // A pruning engine push keeps the key and removes a stale file.
        fs::write(dest.path().join("stale.cfg"), "old").unwrap();
        let engine = pack(tree().path()).unwrap();
        let report = apply(&engine, &digest(&engine), dest.path(), &OPTIONS).unwrap();
        assert_eq!(report.pruned, 1);
        assert!(dest.path().join("engine-context.json").exists());
        assert!(!dest.path().join("stale.cfg").exists());
        assert!(dest.path().join(".gorgone-engine_context.files").exists());

        // A file the engine target wrote before and no longer ships goes away.
        let smaller_dir = tempfile::tempdir().unwrap();
        fs::write(smaller_dir.path().join("centengine.cfg"), "a=2\n").unwrap();
        let smaller = pack(smaller_dir.path()).unwrap();
        let report = apply(&smaller, &digest(&smaller), dest.path(), &OPTIONS).unwrap();
        assert_eq!(report.pruned, 1);
        assert!(!dest.path().join("objects/hosts.cfg").exists());
        assert!(dest.path().join("engine-context.json").exists());
    }

    #[test]
    fn prune_skips_the_directory_of_another_target() {
        let dest = tempfile::tempdir().unwrap();
        let nested = dest.path().join("vmware");
        fs::create_dir(&nested).unwrap();
        fs::write(nested.join("centreon_vmware.json"), "{}").unwrap();
        fs::write(nested.join(".gorgone-vmware.digest"), "x").unwrap();
        let bundle = pack(tree().path()).unwrap();
        apply(&bundle, &digest(&bundle), dest.path(), &OPTIONS).unwrap();
        assert!(nested.join("centreon_vmware.json").exists());
    }

    #[test]
    fn bundle_cannot_carry_a_manifest() {
        let src = tree();
        fs::write(src.path().join(".gorgone-engine_context.files"), "x").unwrap();
        let bundle = pack(src.path()).unwrap();
        let dest = tempfile::tempdir().unwrap();
        let err = apply(&bundle, &digest(&bundle), dest.path(), &OPTIONS).unwrap_err();
        assert!(matches!(err, BundleError::UnsafeEntry(_)));
    }

    #[test]
    fn prune_spares_staging_directories_and_markers() {
        let src = tree();
        let bundle = pack(src.path()).unwrap();
        let dest = tempfile::tempdir().unwrap();
        let staging = dest.path().join(format!("{STAGING_PREFIX}abc"));
        fs::create_dir(&staging).unwrap();
        fs::write(staging.join("staged.cfg"), "x").unwrap();
        fs::write(dest.path().join(".gorgone-other.digest"), "x").unwrap();
        fs::write(dest.path().join("stale.cfg"), "x").unwrap();

        let report = apply(&bundle, &digest(&bundle), dest.path(), &OPTIONS).unwrap();
        assert_eq!(report.pruned, 1);
        assert!(staging.join("staged.cfg").exists());
        assert!(dest.path().join(".gorgone-other.digest").exists());
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
    fn plain_tar_gives_the_same_bundle_as_the_directory() {
        let src = tree();
        let mut plain = tar::Builder::new(Vec::new());
        // Reverse order and `./` prefixes: the result must not depend on them.
        for name in ["objects/hosts.cfg", "centengine.cfg"] {
            let data = fs::read(src.path().join(name)).unwrap();
            let mut header = tar::Header::new_gnu();
            header.set_size(data.len() as u64);
            header.set_mode(
                fs::metadata(src.path().join(name))
                    .unwrap()
                    .permissions()
                    .mode(),
            );
            header.set_mtime(1_700_000_000);
            header.set_cksum();
            plain
                .append_data(&mut header, format!("./{name}"), data.as_slice())
                .unwrap();
        }
        let plain = plain.into_inner().unwrap();
        assert_eq!(
            digest(&from_tar(&plain, DEFAULT_MAX_UNPACKED).unwrap()),
            digest(&pack(src.path()).unwrap())
        );
        assert!(matches!(from_tar(&plain, 3), Err(BundleError::TooLarge(3))));
    }

    fn mode_of(path: &Path) -> u32 {
        fs::metadata(path).unwrap().permissions().mode() & 0o7777
    }

    #[test]
    fn forced_file_mode_applies_to_every_file_but_the_marker() {
        let src = tree();
        let script = src.path().join("hook.sh");
        fs::write(&script, "#!/bin/sh\n").unwrap();
        fs::set_permissions(&script, fs::Permissions::from_mode(0o755)).unwrap();
        let bundle = pack(src.path()).unwrap();
        let sum = digest(&bundle);
        let dest = tempfile::tempdir().unwrap();
        apply_with_file_mode(&bundle, &sum, dest.path(), &OPTIONS, Some(0o640)).unwrap();
        for name in ["hook.sh", "centengine.cfg", "objects/hosts.cfg"] {
            assert_eq!(mode_of(&dest.path().join(name)), 0o640, "{name}");
        }
        assert_eq!(mode_of(&dest.path().join(OPTIONS.marker)), 0o644);

        // Only the permission bits are taken.
        let dest = tempfile::tempdir().unwrap();
        apply_with_file_mode(&bundle, &sum, dest.path(), &OPTIONS, Some(0o4600)).unwrap();
        assert_eq!(mode_of(&dest.path().join("hook.sh")), 0o600);
    }

    #[test]
    fn to_tar_round_trips_and_keeps_names_and_modes() {
        let src = tree();
        let script = src.path().join("hook.sh");
        fs::write(&script, "#!/bin/sh\n").unwrap();
        fs::set_permissions(&script, fs::Permissions::from_mode(0o750)).unwrap();
        let bundle = pack(src.path()).unwrap();
        let plain = to_tar(&bundle, DEFAULT_MAX_UNPACKED).unwrap();
        assert_eq!(from_tar(&plain, DEFAULT_MAX_UNPACKED).unwrap(), bundle);

        let mut archive = tar::Archive::new(plain.as_slice());
        let entries: Vec<(String, u32, Vec<u8>)> = archive
            .entries()
            .unwrap()
            .map(|entry| {
                let mut entry = entry.unwrap();
                let name = entry.path().unwrap().display().to_string();
                let mode = entry.header().mode().unwrap();
                let mut data = Vec::new();
                entry.read_to_end(&mut data).unwrap();
                (name, mode, data)
            })
            .collect();
        assert_eq!(
            entries,
            [
                ("centengine.cfg".to_owned(), 0o644, b"a=1\n".to_vec()),
                ("hook.sh".to_owned(), 0o750, b"#!/bin/sh\n".to_vec()),
                (
                    "objects/hosts.cfg".to_owned(),
                    0o644,
                    b"define host{}\n".to_vec()
                ),
            ]
        );
    }

    #[test]
    fn to_tar_refuses_unsafe_entries_and_oversized_bundles() {
        let src = tree();
        let bundle = pack(src.path()).unwrap();
        assert!(matches!(to_tar(&bundle, 3), Err(BundleError::TooLarge(3))));
        assert!(matches!(
            to_tar(b"not a bundle", 10),
            Err(BundleError::Invalid(_))
        ));

        let bundle_of = |header: &tar::Header, data: &[u8]| {
            let encoder = zstd::Encoder::new(Vec::new(), 3).unwrap();
            let mut tar = tar::Builder::new(encoder);
            tar.append(header, data).unwrap();
            tar.into_inner().unwrap().finish().unwrap()
        };
        let mut traversal = tar::Header::new_gnu();
        traversal.set_entry_type(tar::EntryType::Regular);
        traversal.set_size(1);
        // `set_path` refuses `..`, so write the raw name.
        traversal.as_gnu_mut().unwrap().name[..9].copy_from_slice(b"../escape");
        traversal.set_cksum();
        let mut link = tar::Header::new_gnu();
        link.set_entry_type(tar::EntryType::Symlink);
        link.set_size(0);
        link.set_path("passwd").unwrap();
        link.set_link_name("/etc/passwd").unwrap();
        link.set_cksum();
        let mut marker = tar::Header::new_gnu();
        marker.set_entry_type(tar::EntryType::Regular);
        marker.set_size(1);
        marker.set_path(".gorgone-engine.digest").unwrap();
        marker.set_cksum();
        for (header, data) in [(&traversal, &b"x"[..]), (&link, b""), (&marker, b"x")] {
            let error = to_tar(&bundle_of(header, data), DEFAULT_MAX_UNPACKED).unwrap_err();
            assert!(matches!(error, BundleError::UnsafeEntry(_)), "{error}");
        }
    }

    #[test]
    fn plain_tar_with_a_link_is_refused() {
        let mut plain = tar::Builder::new(Vec::new());
        let mut header = tar::Header::new_gnu();
        header.set_entry_type(tar::EntryType::Symlink);
        header.set_size(0);
        plain
            .append_link(&mut header, "passwd", "/etc/passwd")
            .unwrap();
        let plain = plain.into_inner().unwrap();
        assert!(matches!(
            from_tar(&plain, DEFAULT_MAX_UNPACKED),
            Err(BundleError::UnsafeEntry(_))
        ));
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
