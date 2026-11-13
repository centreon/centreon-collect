//! Check of an engine configuration before it goes live: the bundle is
//! unpacked in a staging directory inside the target path, the only place the
//! engine is sure to see, and verified there by the engine that will load it.

use std::collections::hash_map::RandomState;
use std::ffi::OsString;
use std::fs;
use std::hash::{BuildHasher, Hasher};
use std::io;
use std::path::{Component, Path, PathBuf};
use std::time::{Duration, Instant};

use gorgone_core::bundle::{self, ApplyOptions, BundleError, STAGING_PREFIX};
use tokio_util::sync::CancellationToken;

use crate::config::EngineConfig;
use crate::engine_api::EngineApi;
use crate::process::{self, CommandSpec, ProcessOutput};

/// Suffix of the main file written for the check, next to the staged one.
/// Not `.cfg`: a `cfg_dir` covering the staging directory must not load it.
const VERIFY_SUFFIX: &str = ".gorgone-verify";
/// Digest marker of a staging directory; never read.
const STAGING_MARKER: &str = ".gorgone-staging.digest";
/// Directives of the main file naming other configuration files.
const PATH_DIRECTIVES: [&str; 3] = ["cfg_file", "cfg_dir", "resource_file"];

/// Who checks a staged configuration.
pub(crate) enum ConfigChecker {
    /// `VerifyConfiguration` of the engine API.
    Grpc(EngineApi),
    /// Local command, exiting with 0 when the configuration is valid.
    Command {
        /// Arguments of `engine.verify_command`, placeholder included.
        argv: Vec<String>,
        timeout: Duration,
        kill_grace: Duration,
    },
}

impl ConfigChecker {
    /// Checks the configuration whose main file is `file`. `Err` tells why
    /// the check could not run at all.
    pub(crate) async fn check(
        &self,
        file: &str,
        max_output: usize,
        cancel: &CancellationToken,
    ) -> Result<ProcessOutput, String> {
        match self {
            Self::Grpc(engine) => {
                let started = Instant::now();
                let answer = tokio::select! {
                    answer = engine.verify_configuration(file) => Some(answer?),
                    () = cancel.cancelled() => None,
                };
                let cancelled = answer.is_none();
                let answer = answer.unwrap_or_default();
                Ok(ProcessOutput {
                    exit_code: if cancelled { -1 } else { answer.exit_code },
                    stdout: shorten(answer.output, max_output),
                    stderr: String::new(),
                    duration: started.elapsed(),
                    timed_out: false,
                    cancelled,
                })
            }
            Self::Command {
                argv,
                timeout,
                kill_grace,
            } => {
                // Replaced inside an argument: the path is never split.
                let argv: Vec<String> = argv
                    .iter()
                    .map(|argument| argument.replace(EngineConfig::VERIFY_PLACEHOLDER, file))
                    .collect();
                let spec = CommandSpec {
                    line: "",
                    shell: false,
                    argv: Some(&argv),
                    timeout: *timeout,
                    kill_grace: *kill_grace,
                    max_output,
                };
                process::run(&spec, cancel)
                    .await
                    .map_err(|error| error.to_string())
            }
        }
    }
}

/// Share of a capped output kept for its end, in bytes at most.
const KEPT_END: usize = 1024;

/// Cuts the middle of an output above `max` bytes: its end holds the totals
/// the central reads.
fn shorten(output: String, max: usize) -> String {
    if output.len() <= max {
        return output;
    }
    let mut cut = output.len() - KEPT_END.min(max / 4);
    while !output.is_char_boundary(cut) {
        cut += 1;
    }
    let end = output[cut..].to_owned();
    let start = process::truncate_output(output, max - end.len());
    format!("{start}\n{end}")
}

/// Number of errors a checker reports on its `Total Errors:` line, if any.
pub(crate) fn total_errors(output: &str) -> Option<u64> {
    output
        .lines()
        .rev()
        .find_map(|line| line.trim().strip_prefix("Total Errors:"))
        .and_then(|count| count.trim().parse().ok())
}

/// Reasons a bundle could not be staged.
#[derive(Debug, thiserror::Error)]
pub(crate) enum StageError {
    #[error("{path}: {source}")]
    Io { path: PathBuf, source: io::Error },
    #[error(transparent)]
    Bundle(#[from] BundleError),
    #[error("the bundle does not hold the engine main file {0}")]
    NoMainFile(PathBuf),
    #[error("{0}: the path is not UTF-8")]
    NotUtf8(PathBuf),
}

fn io_err(path: &Path) -> impl FnOnce(io::Error) -> StageError + '_ {
    move |source| StageError::Io {
        path: path.to_path_buf(),
        source,
    }
}

/// A bundle unpacked aside. Dropping it removes the directory: blocking, so
/// drop it off the runtime threads when possible.
#[derive(Debug)]
pub(crate) struct Staging {
    dir: PathBuf,
    /// Main file to give to the checker.
    pub verify_file: String,
    /// Paths of the main file left pointing outside the target path.
    pub outside: Vec<String>,
}

impl Drop for Staging {
    fn drop(&mut self) {
        if let Err(error) = fs::remove_dir_all(&self.dir)
            && error.kind() != io::ErrorKind::NotFound
        {
            tracing::warn!(dir = %self.dir.display(), %error, "staging directory not removed");
        }
    }
}

/// Removes what a previous run left in `target_path`; an absent directory has
/// nothing to remove.
pub(crate) fn remove_leftovers(target_path: &Path) {
    let Ok(entries) = fs::read_dir(target_path) else {
        return;
    };
    for entry in entries.flatten() {
        if !entry
            .file_name()
            .to_str()
            .is_some_and(|name| name.starts_with(STAGING_PREFIX))
        {
            continue;
        }
        let path = entry.path();
        // A link is removed, never followed.
        let removed = match entry.file_type() {
            Ok(kind) if kind.is_dir() => fs::remove_dir_all(&path),
            _ => fs::remove_file(&path),
        };
        match removed {
            Ok(()) => tracing::info!(path = %path.display(), "leftover staging removed"),
            Err(error) => {
                tracing::warn!(path = %path.display(), %error, "leftover staging not removed");
            }
        }
    }
}

/// Unpacks `blob` in a fresh staging directory of `target_path` and writes
/// the main file to verify. Blocking.
pub(crate) fn stage(
    blob: &[u8],
    digest: &str,
    target_path: &Path,
    main_file: &Path,
    max_unpacked: u64,
) -> Result<Staging, StageError> {
    fs::create_dir_all(target_path).map_err(io_err(target_path))?;
    // The engine reads the staged files by this path.
    let root = fs::canonicalize(target_path).map_err(io_err(target_path))?;
    // One staging at a time: whatever is there was abandoned.
    remove_leftovers(&root);
    let random = RandomState::new().build_hasher().finish();
    let dir = root.join(format!("{STAGING_PREFIX}{random:016x}"));
    fs::create_dir(&dir).map_err(io_err(&dir))?;
    let mut staging = Staging {
        dir: dir.clone(),
        verify_file: String::new(),
        outside: Vec::new(),
    };

    let options = ApplyOptions {
        marker: STAGING_MARKER,
        prune: false,
        max_unpacked,
    };
    bundle::apply(blob, digest, &dir, &options)?;

    let staged_main = dir.join(main_file);
    let content = fs::read(&staged_main).map_err(|source| match source.kind() {
        io::ErrorKind::NotFound => StageError::NoMainFile(main_file.to_path_buf()),
        _ => io_err(&staged_main)(source),
    })?;
    let staged_root = dir
        .to_str()
        .ok_or_else(|| StageError::NotUtf8(dir.clone()))?;
    let (derived, outside) = redirect(&content, &[target_path, &root], staged_root);
    let mut verify_file = OsString::from(&staged_main);
    verify_file.push(VERIFY_SUFFIX);
    let verify_file = PathBuf::from(verify_file);
    fs::write(&verify_file, derived).map_err(io_err(&verify_file))?;
    staging.verify_file = verify_file
        .into_os_string()
        .into_string()
        .map_err(|path| StageError::NotUtf8(path.into()))?;
    staging.outside = outside;
    Ok(staging)
}

/// Copy of a main file where the paths under one of `roots` point to the
/// same place under `staging`. Also returns the absolute paths left as they
/// are because they are outside every root. Relative paths are kept: the
/// engine resolves them from the directory of the main file it is given.
fn redirect(content: &[u8], roots: &[&Path], staging: &str) -> (Vec<u8>, Vec<String>) {
    let mut derived = Vec::with_capacity(content.len() + 256);
    let mut outside = Vec::new();
    for line in content.split_inclusive(|byte| *byte == b'\n') {
        let directive = std::str::from_utf8(line).ok().and_then(path_directive);
        let Some((key, value, ending)) = directive else {
            derived.extend_from_slice(line);
            continue;
        };
        let path = Path::new(value);
        // `..` would defeat a comparison of prefixes.
        let plain = path
            .components()
            .all(|part| matches!(part, Component::RootDir | Component::Normal(_)));
        let inside = roots
            .iter()
            .find_map(|root| path.strip_prefix(root).ok())
            .filter(|_| plain)
            .and_then(Path::to_str);
        match inside {
            Some("") => derived.extend_from_slice(format!("{key}={staging}{ending}").as_bytes()),
            Some(rest) => {
                derived.extend_from_slice(format!("{key}={staging}/{rest}{ending}").as_bytes());
            }
            None => {
                outside.push(value.to_owned());
                derived.extend_from_slice(line);
            }
        }
    }
    (derived, outside)
}

/// Splits a `cfg_file=`, `cfg_dir=` or `resource_file=` line holding an
/// absolute path into its key, its path and its line ending.
fn path_directive(line: &str) -> Option<(&str, &str, &str)> {
    let (key, value) = line.split_once('=')?;
    if !PATH_DIRECTIVES.contains(&key.trim()) {
        return None;
    }
    let ending = &value[value.trim_end().len()..];
    let value = value.trim();
    value.starts_with('/').then_some((key, value, ending))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn bundle_of(files: &[(&str, &str)]) -> (String, Vec<u8>) {
        let src = tempfile::tempdir().unwrap();
        for (name, content) in files {
            let path = src.path().join(name);
            fs::create_dir_all(path.parent().unwrap()).unwrap();
            fs::write(path, content).unwrap();
        }
        let blob = bundle::pack(src.path()).unwrap();
        (bundle::digest(&blob), blob)
    }

    fn staging_dirs(target: &Path) -> Vec<PathBuf> {
        fs::read_dir(target)
            .unwrap()
            .map(|entry| entry.unwrap().path())
            .filter(|path| {
                let name = path.file_name().unwrap().to_str().unwrap();
                name.starts_with(STAGING_PREFIX)
            })
            .collect()
    }

    #[test]
    fn output_keeps_its_end_and_tells_its_errors() {
        let totals = "\nTotal Warnings: 2\nTotal Errors:   3\n";
        let short = format!("Error: x{totals}");
        assert_eq!(shorten(short.clone(), 4096), short);
        assert_eq!(total_errors(&short), Some(3));
        assert_eq!(total_errors("Error: x\n"), None);
        assert_eq!(total_errors("Total Errors: many\n"), None);

        let long = format!("{}{totals}", "é".repeat(4000));
        let cut = shorten(long, 400);
        assert!(cut.len() <= 400 + "\n[output truncated]\n".len(), "{cut}");
        assert!(cut.starts_with("éé") && cut.contains("\n[output truncated]\n"));
        assert!(cut.ends_with(totals), "{cut}");
        assert_eq!(total_errors(&cut), Some(3));
    }

    #[test]
    fn redirects_only_the_paths_inside_the_roots() {
        let main = "# cfg_file=/etc/engine/commented.cfg\n\
                    cfg_file=/etc/engine/hosts.cfg\n\
                    \x20 cfg_dir = /etc/engine/objects \r\n\
                    cfg_dir=/etc/engine\n\
                    cfg_dir=/link/engine/extra\n\
                    resource_file=/etc/engine/resource.cfg\n\
                    cfg_file=relative.cfg\n\
                    cfg_file=/etc/engine-other/hosts.cfg\n\
                    cfg_file=/etc/engine/../passwd\n\
                    log_file=/etc/engine/centengine.log\n\
                    broker_module=/usr/lib/cbmod.so /etc/engine/cbmod.json\n\
                    cfg_file=/etc/engine/last.cfg";
        let roots = [Path::new("/link/engine"), Path::new("/etc/engine")];
        let (derived, outside) = redirect(main.as_bytes(), &roots, "/etc/engine/.s");
        assert_eq!(
            String::from_utf8(derived).unwrap(),
            "# cfg_file=/etc/engine/commented.cfg\n\
             cfg_file=/etc/engine/.s/hosts.cfg\n\
             \x20 cfg_dir =/etc/engine/.s/objects \r\n\
             cfg_dir=/etc/engine/.s\n\
             cfg_dir=/etc/engine/.s/extra\n\
             resource_file=/etc/engine/.s/resource.cfg\n\
             cfg_file=relative.cfg\n\
             cfg_file=/etc/engine-other/hosts.cfg\n\
             cfg_file=/etc/engine/../passwd\n\
             log_file=/etc/engine/centengine.log\n\
             broker_module=/usr/lib/cbmod.so /etc/engine/cbmod.json\n\
             cfg_file=/etc/engine/.s/last.cfg"
        );
        assert_eq!(
            outside,
            ["/etc/engine-other/hosts.cfg", "/etc/engine/../passwd"]
        );

        // Bytes that are not UTF-8 are copied as they are.
        let latin1 = b"# caf\xe9\ncfg_file=/etc/engine/a.cfg\n";
        let (derived, _) = redirect(latin1, &roots, "/s");
        assert_eq!(derived, b"# caf\xe9\ncfg_file=/s/a.cfg\n");
    }

    #[test]
    fn stages_inside_the_target_and_cleans_up() {
        let target = tempfile::tempdir().unwrap();
        let live = target.path().to_str().unwrap();
        fs::write(target.path().join("centengine.cfg"), "live\n").unwrap();
        let main = format!("cfg_file={live}/conf/hosts.cfg\ncfg_file=/opt/extra.cfg\n");
        let (digest, blob) = bundle_of(&[("conf/centengine.cfg", &main), ("conf/hosts.cfg", "h")]);

        let main_file = Path::new("conf/centengine.cfg");
        let staging = stage(&blob, &digest, target.path(), main_file, 1 << 20).unwrap();
        let dirs = staging_dirs(target.path());
        assert_eq!(dirs.len(), 1);
        let dir = fs::canonicalize(&dirs[0]).unwrap();
        let expected = dir.join("conf/centengine.cfg.gorgone-verify");
        assert_eq!(Path::new(&staging.verify_file), expected);
        assert_eq!(
            fs::read_to_string(&expected).unwrap(),
            format!(
                "cfg_file={}/conf/hosts.cfg\ncfg_file=/opt/extra.cfg\n",
                dir.display()
            )
        );
        // The staged main file itself is the one of the bundle.
        assert_eq!(
            fs::read_to_string(dir.join("conf/centengine.cfg")).unwrap(),
            main
        );
        assert!(dir.join("conf/hosts.cfg").exists());
        assert_eq!(staging.outside, ["/opt/extra.cfg"]);

        drop(staging);
        assert!(staging_dirs(target.path()).is_empty());
        assert_eq!(
            fs::read_to_string(target.path().join("centengine.cfg")).unwrap(),
            "live\n"
        );
    }

    #[test]
    fn failed_staging_leaves_nothing_and_leftovers_are_removed() {
        let target = tempfile::tempdir().unwrap();
        let (digest, blob) = bundle_of(&[("hosts.cfg", "h")]);
        let main_file = Path::new("centengine.cfg");

        let error = stage(&blob, &digest, target.path(), main_file, 1 << 20).unwrap_err();
        assert!(matches!(error, StageError::NoMainFile(_)), "{error}");
        let error = stage(&blob, &"0".repeat(64), target.path(), main_file, 1 << 20).unwrap_err();
        assert!(error.to_string().contains("digest mismatch"), "{error}");
        let error = stage(&blob, &digest, target.path(), main_file, 0).unwrap_err();
        assert!(error.to_string().contains("larger than"), "{error}");
        assert!(staging_dirs(target.path()).is_empty());

        // What a killed agent left: a directory, a file and a link.
        let outside = tempfile::tempdir().unwrap();
        fs::write(outside.path().join("kept"), "x").unwrap();
        let leftover = |name: &str| target.path().join(format!("{STAGING_PREFIX}{name}"));
        fs::create_dir_all(leftover("dir").join("objects")).unwrap();
        fs::write(leftover("dir").join("objects/hosts.cfg"), "h").unwrap();
        fs::write(leftover("file"), "x").unwrap();
        std::os::unix::fs::symlink(outside.path(), leftover("link")).unwrap();
        fs::write(target.path().join("hosts.cfg"), "live").unwrap();
        assert_eq!(staging_dirs(target.path()).len(), 3);

        remove_leftovers(target.path());
        assert!(staging_dirs(target.path()).is_empty());
        assert!(target.path().join("hosts.cfg").exists());
        assert!(outside.path().join("kept").exists());
        // A target never created has nothing to clean.
        remove_leftovers(&target.path().join("absent"));

        // A new staging also sweeps what is there.
        fs::create_dir(leftover("old")).unwrap();
        let (digest, blob) = bundle_of(&[("centengine.cfg", "a=1\n")]);
        let staging = stage(&blob, &digest, target.path(), main_file, 1 << 20).unwrap();
        assert_eq!(staging_dirs(target.path()).len(), 1);
        assert!(!leftover("old").exists());
        drop(staging);
    }
}
