//! Legacy `centcore.cmd` intake: reads the command files still written by
//! centreon-web (`CMD:target[:param]` lines) and submits the equivalent jobs.
//!
//! Singleton work: only the replica holding the lease reads the files. A
//! file is renamed to `<name>.<nonce>_read` before being read, each handled
//! line is overwritten with a leading `-`, and the file is removed at the
//! end, so a restart resumes where the previous run stopped.
//!
//! Replays are harmless: every job gets an id derived from the nonce of its
//! file, the offset of its line and its rank in the line, and submitting an
//! existing id does nothing. A crash between a submit and its mark, or a
//! lease lost in the middle of a file, therefore never duplicates a command.

use std::collections::BTreeMap;
use std::fs::{self, File, OpenOptions};
use std::io::Read;
use std::os::unix::fs::{FileExt, MetadataExt, OpenOptionsExt};
use std::path::{Path, PathBuf};
use std::sync::Arc;
use std::time::{Duration, Instant};

use gorgone_core::bundle;
use gorgone_proto::agent::action::Kind;
use gorgone_proto::agent::service_control::Operation;
use gorgone_proto::agent::{Action, ApplyConfig, EngineCommands, ServiceControl};
use sha2::{Digest, Sha256};
use tokio::runtime::Handle;
use tokio_util::sync::CancellationToken;
use tracing::{debug, info, warn};

use crate::config::CentcoreConfig;
use crate::dispatcher::Dispatcher;
use crate::error::Error;
use crate::metrics;
use crate::store::Store;

/// Name of the lease guarding the intake.
const LEASE: &str = "centcore";
/// Suffix of a file being read.
const READ_SUFFIX: &str = "_read";
/// Length of the nonce inserted in the name of a file being read.
const NONCE_LEN: usize = 32;
/// Largest command file read; a larger one is set aside.
const MAX_FILE_BYTES: u64 = 256 << 20;

/// What one pass over the command files did.
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq)]
pub(crate) struct PassStats {
    /// Files read to their end and removed.
    pub(crate) files: usize,
    /// Jobs created.
    pub(crate) jobs: usize,
}

/// The pass must stop and leave the current file for the next one.
#[derive(Debug)]
struct Abort;

/// One parsed `CMD:target[:param]` line.
#[derive(Debug, PartialEq, Eq)]
struct Line<'a> {
    command: &'a str,
    target: &'a str,
    param: Option<&'a str>,
}

fn parse_line(line: &str) -> Option<Line<'_>> {
    let (command, rest) = line.split_once(':')?;
    if command.is_empty() {
        return None;
    }
    let (target, param) = match rest.split_once(':') {
        Some((target, param)) => (target, Some(param)),
        None => (rest, None),
    };
    Some(Line {
        command,
        target: target.trim(),
        param,
    })
}

/// A target ends up in a path of the cache directory and in a node id.
fn is_safe_target(target: &str) -> bool {
    !target.is_empty()
        && target.len() <= 191
        && target != "."
        && target != ".."
        && !target.contains(['/', '\\'])
        && !target.chars().any(char::is_control)
}

fn service_control(command: &str) -> Option<(&'static str, Operation)> {
    Some(match command {
        "RELOAD" | "ENGINERELOAD" => ("engine", Operation::Reload),
        "RESTART" | "ENGINERESTART" => ("engine", Operation::Restart),
        "START" => ("engine", Operation::Start),
        "STOP" => ("engine", Operation::Stop),
        "RELOADBROKER" => ("broker", Operation::Reload),
        "RELOADCENTREONTRAPD" => ("centreontrapd", Operation::Reload),
        "RESTARTCENTREONTRAPD" => ("centreontrapd", Operation::Restart),
        "VMWARERESTART" => ("vmware", Operation::Restart),
        _ => return None,
    })
}

/// Nonce of a `<base>.<nonce>_read` file name, with its base.
fn split_nonce(name: &str) -> Option<(&str, &str)> {
    let stem = name.strip_suffix(READ_SUFFIX)?;
    let (base, nonce) = stem.rsplit_once('.')?;
    (nonce.len() == NONCE_LEN && nonce.bytes().all(|b| b.is_ascii_hexdigit()))
        .then_some((base, nonce))
}

/// Deterministic job id of the `tag` part of the line at `offset`.
fn job_id(nonce: &str, offset: u64, tag: &str) -> String {
    let mut hasher = Sha256::new();
    hasher.update(nonce.as_bytes());
    hasher.update(offset.to_be_bytes());
    hasher.update(tag.as_bytes());
    format!("cc-{}", crate::hex(&hasher.finalize()[..16]))
}

/// A command file ready to be read, and the nonce naming its jobs.
#[derive(Debug)]
struct Claimed {
    path: PathBuf,
    nonce: String,
}

/// Gives `path` its "being read" name. A file already named so by a
/// previous run keeps its nonce: that is what makes its replay idempotent.
fn claim(path: &Path) -> Option<Claimed> {
    let name = path.file_name()?.to_str()?;
    if let Some((_, nonce)) = split_nonce(name) {
        return Some(Claimed {
            path: path.to_path_buf(),
            nonce: nonce.to_owned(),
        });
    }
    // A plain file, or a leftover of the Perl daemon (`<name>_read`).
    let base = name.strip_suffix(READ_SUFFIX).unwrap_or(name);
    let nonce = match crate::random_id() {
        Ok(nonce) => nonce,
        Err(err) => {
            warn!(file = %path.display(), error = %err, "command file not claimed");
            return None;
        }
    };
    let reading = path.with_file_name(format!("{base}.{nonce}{READ_SUFFIX}"));
    // `rename` never follows a link: a planted one is moved, not its target.
    match fs::rename(path, &reading) {
        Ok(()) => Some(Claimed {
            path: reading,
            nonce,
        }),
        Err(err) => {
            warn!(file = %path.display(), error = %err, "cannot move command file");
            None
        }
    }
}

/// Entries of `dir` accepted by `wanted`, oldest change first. Links and
/// special files are reported, never read.
fn list(dir: &Path, wanted: impl Fn(&str) -> bool) -> Vec<PathBuf> {
    let entries = match fs::read_dir(dir) {
        Ok(entries) => entries,
        Err(err) if err.kind() == std::io::ErrorKind::NotFound => return Vec::new(),
        Err(err) => {
            warn!(dir = %dir.display(), error = %err, "cannot read the command directory");
            return Vec::new();
        }
    };
    let mut files = Vec::new();
    for entry in entries.filter_map(Result::ok) {
        let name = entry.file_name();
        let Some(name) = name.to_str().filter(|name| wanted(name)) else {
            continue;
        };
        // Not followed: this describes the entry itself.
        let Ok(meta) = fs::symlink_metadata(entry.path()) else {
            continue;
        };
        if meta.is_dir() {
            continue;
        }
        if !meta.is_file() {
            warn!(file = %entry.path().display(), "command file is a link or a special file, ignored");
            continue;
        }
        files.push((
            (meta.ctime(), meta.ctime_nsec(), name.to_owned()),
            entry.path(),
        ));
    }
    files.sort();
    files.into_iter().map(|(_, path)| path).collect()
}

/// External commands of one node waiting to be sent together.
#[derive(Debug, Default)]
struct Pending {
    commands: Vec<String>,
    /// File offsets of the lines, marked once the batch is submitted.
    offsets: Vec<u64>,
}

/// State of the file being read.
struct Reading {
    /// `None` when the file is read-only for this process: centreon-web
    /// writes its per-command files that way, and nothing is marked.
    marks: Option<File>,
    nonce: String,
    pending: BTreeMap<String, Pending>,
}

impl Reading {
    /// Overwrites the first byte of a handled line, like the Perl daemon.
    fn mark(&self, offset: u64) {
        let Some(file) = &self.marks else {
            // Harmless: the replayed line gives the same job id.
            return;
        };
        if let Err(err) = file.write_at(b"-", offset) {
            // Harmless: the replayed line gives the same job id.
            warn!(error = %err, "cannot mark a command line as handled");
        }
    }
}

/// Legacy command file reader.
pub(crate) struct Intake {
    config: CentcoreConfig,
    dispatcher: Arc<Dispatcher>,
    store: Store,
    replica_id: String,
    lease: String,
    /// Owners a command file may have.
    allowed_uids: Vec<u32>,
}

/// Effective user id of the process: the owner of what it creates.
fn process_uid() -> u32 {
    // SAFETY: `geteuid` has no precondition and cannot fail.
    unsafe { libc::geteuid() }
}

impl Intake {
    pub(crate) fn new(
        config: CentcoreConfig,
        dispatcher: Arc<Dispatcher>,
        store: Store,
        replica_id: String,
    ) -> Arc<Self> {
        Arc::new(Self::with_lease(
            config,
            dispatcher,
            store,
            replica_id,
            LEASE.to_owned(),
        ))
    }

    fn with_lease(
        config: CentcoreConfig,
        dispatcher: Arc<Dispatcher>,
        store: Store,
        replica_id: String,
        lease: String,
    ) -> Self {
        let allowed_uids = if config.allowed_uids.is_empty() {
            vec![process_uid(), 0]
        } else {
            config.allowed_uids.clone()
        };
        Self {
            config,
            dispatcher,
            store,
            replica_id,
            lease,
            allowed_uids,
        }
    }

    fn lease_ttl(&self) -> Duration {
        Duration::from_millis(self.config.poll_interval_ms.saturating_mul(3))
            .max(Duration::from_secs(15))
    }

    /// Polls the command files until `shutdown`.
    pub(crate) async fn run(self: Arc<Self>, shutdown: CancellationToken) {
        let mut tick = tokio::time::interval(Duration::from_millis(self.config.poll_interval_ms));
        tick.set_missed_tick_behavior(tokio::time::MissedTickBehavior::Delay);
        loop {
            tokio::select! {
                () = shutdown.cancelled() => break,
                _ = tick.tick() => {}
            }
            match self.tick(&shutdown).await {
                Ok(Some(stats)) if stats.files > 0 => {
                    info!(
                        files = stats.files,
                        jobs = stats.jobs,
                        "centcore commands handled"
                    );
                }
                Ok(_) => {}
                Err(err) => warn!(error = %err, "centcore intake failed"),
            }
        }
        // Lets another replica take over without waiting for the expiry.
        if let Err(err) = self
            .store
            .release_lease(&self.lease, &self.replica_id)
            .await
        {
            warn!(error = %err, "centcore lease not released");
        }
    }

    async fn hold_lease(&self) -> Result<bool, Error> {
        let ttl = i64::try_from(self.lease_ttl().as_millis()).unwrap_or(i64::MAX);
        self.store
            .acquire_lease(&self.lease, &self.replica_id, ttl)
            .await
    }

    /// One poll: `None` when another replica holds the lease.
    pub(crate) async fn tick(
        self: &Arc<Self>,
        shutdown: &CancellationToken,
    ) -> Result<Option<PassStats>, Error> {
        if !self.hold_lease().await? {
            return Ok(None);
        }
        let this = Arc::clone(self);
        let runtime = Handle::current();
        let stop = shutdown.clone();
        // File access and packing are blocking: keep them off the workers.
        tokio::task::spawn_blocking(move || {
            let mut pass = Pass {
                intake: &this,
                runtime: &runtime,
                stop: &stop,
                stats: PassStats::default(),
                renewed: Instant::now(),
            };
            pass.run();
            pass.stats
        })
        .await
        .map(Some)
        .map_err(|err| Error::Invalid(format!("centcore pass aborted: {err}")))
    }

    /// Files to read: leftovers of the main command file and the file
    /// itself, then the files of the directory, oldest first.
    fn candidates(&self) -> Vec<PathBuf> {
        let mut files = Vec::new();
        let main = &self.config.cmd_file;
        if let (Some(dir), Some(base)) = (main.parent(), main.file_name().and_then(|n| n.to_str()))
        {
            let legacy = format!("{base}{READ_SUFFIX}");
            let mut own = list(dir, |name| {
                name == legacy || split_nonce(name).is_some_and(|(stem, _)| stem == base)
            });
            // What a previous run left goes before what was written since.
            own.extend(list(dir, |name| name == base));
            files.extend(own);
        }
        files.extend(list(&self.config.cmd_dir, |name| !name.starts_with('.')));
        files
    }

    /// Opens a claimed file without following links and checks it is a
    /// regular file owned by an expected user. Returns its content, and the
    /// file itself when lines can be marked in it.
    fn open(&self, path: &Path) -> Result<(Option<File>, Vec<u8>), String> {
        let open = |write: bool| {
            OpenOptions::new()
                .read(true)
                .write(write)
                .custom_flags(libc::O_NOFOLLOW | libc::O_NONBLOCK)
                .open(path)
        };
        let (mut file, writable) = match open(true) {
            Ok(file) => (file, true),
            Err(err) if err.kind() == std::io::ErrorKind::PermissionDenied => {
                (open(false).map_err(|err| err.to_string())?, false)
            }
            Err(err) => return Err(err.to_string()),
        };
        // On the descriptor: what is checked is what is read.
        let meta = file.metadata().map_err(|err| err.to_string())?;
        if !meta.is_file() {
            return Err("not a regular file".into());
        }
        if !self.allowed_uids.contains(&meta.uid()) {
            return Err(format!("owned by uid {}, which is not allowed", meta.uid()));
        }
        if meta.len() > MAX_FILE_BYTES {
            return Err(format!("larger than {MAX_FILE_BYTES} bytes"));
        }
        let mut content = Vec::new();
        file.read_to_end(&mut content)
            .map_err(|err| err.to_string())?;
        Ok((writable.then_some(file), content))
    }
}

/// Moves a refused file out of the way, under a hidden name the intake
/// never lists, so that it is reported once and kept for inspection.
fn set_aside(path: &Path) {
    let Some(name) = path.file_name().and_then(|name| name.to_str()) else {
        return;
    };
    let aside = path.with_file_name(format!(".rejected-{name}"));
    if let Err(err) = fs::rename(path, &aside) {
        warn!(file = %path.display(), error = %err, "cannot set a refused command file aside");
    }
}

/// One pass over the command files, on a blocking thread.
struct Pass<'a> {
    intake: &'a Intake,
    runtime: &'a Handle,
    stop: &'a CancellationToken,
    stats: PassStats,
    /// When the lease was last confirmed.
    renewed: Instant,
}

impl Pass<'_> {
    fn run(&mut self) {
        for path in self.intake.candidates() {
            let Some(claimed) = claim(&path) else {
                continue;
            };
            if self.handle_file(&claimed).is_err() {
                break;
            }
        }
    }

    /// Confirms the lease is still ours; renewing it is how it is checked.
    fn ensure_lease(&mut self) -> Result<(), Abort> {
        match self.runtime.block_on(self.intake.hold_lease()) {
            Ok(true) => {
                self.renewed = Instant::now();
                Ok(())
            }
            Ok(false) => {
                warn!("centcore lease lost, leaving the files to its new holder");
                Err(Abort)
            }
            Err(err) => {
                warn!(error = %err, "centcore lease not renewed");
                Err(Abort)
            }
        }
    }

    fn handle_file(&mut self, claimed: &Claimed) -> Result<(), Abort> {
        let path = &claimed.path;
        let (marks, content) = match self.intake.open(path) {
            Ok(opened) => opened,
            Err(reason) => {
                warn!(file = %path.display(), reason, "command file refused, set aside");
                metrics::CENTCORE_FILES_REFUSED.inc(1);
                set_aside(path);
                return Ok(());
            }
        };
        debug!(file = %path.display(), "processing command file");
        let mut reading = Reading {
            marks,
            nonce: claimed.nonce.clone(),
            pending: BTreeMap::new(),
        };

        let mut offset = 0u64;
        for raw in content.split(|byte| *byte == b'\n') {
            let line_offset = offset;
            offset += raw.len() as u64 + 1;
            if self.stop.is_cancelled() {
                return Err(Abort);
            }
            // Time based: a long file of dropped lines keeps the lease too.
            if self.renewed.elapsed() > self.intake.lease_ttl() / 3 {
                self.ensure_lease()?;
            }
            let text = String::from_utf8_lossy(raw);
            let text = text.trim_end_matches('\r');
            // A leading dash marks a line handled by a previous run.
            if text.is_empty() || text.starts_with('-') {
                continue;
            }
            self.handle_line(text, line_offset, &mut reading)?;
        }
        let targets: Vec<String> = reading.pending.keys().cloned().collect();
        for target in targets {
            self.flush(&target, &mut reading)?;
        }
        drop(reading);
        if let Err(err) = fs::remove_file(path) {
            warn!(file = %path.display(), error = %err, "cannot remove command file");
        }
        self.stats.files += 1;
        Ok(())
    }

    fn dropped(text: &str, reason: &'static str) {
        warn!(line = text, reason, "centcore command dropped");
        metrics::CENTCORE_DROPPED.add(reason, 1);
    }

    /// False, after logging, when jobs for `node` would be refused.
    fn node_known(&mut self, node: &str, text: &str) -> Result<bool, Abort> {
        match self
            .runtime
            .block_on(self.intake.dispatcher.accepts_node(node))
        {
            Ok(true) => Ok(true),
            Ok(false) => {
                Self::dropped(text, "unknown_node");
                Ok(false)
            }
            Err(err) => {
                warn!(line = text, error = %err, "node lookup failed, will retry");
                Err(Abort)
            }
        }
    }

    fn handle_line(&mut self, text: &str, offset: u64, reading: &mut Reading) -> Result<(), Abort> {
        let Some(line) = parse_line(text).filter(|line| {
            // STARTWORKER has no target; everything else needs a usable one.
            is_safe_target(line.target) || matches!(line.command, "STARTWORKER")
        }) else {
            Self::dropped(text, "malformed");
            reading.mark(offset);
            return Ok(());
        };
        debug!(
            command = line.command,
            target = line.target,
            "handling centcore command"
        );

        if line.command == "EXTERNALCMD" {
            let illegal = &self.intake.config.illegal_characters;
            let param: String = line
                .param
                .unwrap_or_default()
                .chars()
                .filter(|c| !illegal.contains(*c))
                .collect();
            // The engine command pipe is line based: no control character
            // may reach it (tabulations are legitimate in plugin outputs).
            if param.is_empty() || param.chars().any(|c| c.is_control() && c != '\t') {
                Self::dropped(text, "malformed");
                reading.mark(offset);
                return Ok(());
            }
            let pending = reading.pending.entry(line.target.to_owned()).or_default();
            pending.commands.push(param);
            pending.offsets.push(offset);
            if pending.commands.len() >= self.intake.config.bulk_external_cmd {
                self.flush(line.target, reading)?;
            }
            return Ok(());
        }

        // Keep the order: what was written before this command goes first.
        self.flush(line.target, reading)?;
        let id = |tag: &str| job_id(&reading.nonce, offset, tag);
        match line.command {
            "SENDCFGFILE" => {
                if self.node_known(line.target, text)? {
                    self.send_config(&line, text, &id)?;
                }
            }
            "SYNCTRAP" => {
                if self.node_known(line.target, text)? {
                    let root = self.intake.config.cache_dir_trap.clone();
                    let source = root.join(line.target).join("centreontrapd.sdb");
                    if source.is_file() {
                        self.apply_config(
                            line.target,
                            "centreontrapd",
                            &root,
                            &source,
                            text,
                            &id("centreontrapd"),
                        )?;
                    } else {
                        warn!(line = text, source = %source.display(), "trap database not found, command dropped");
                    }
                }
            }
            // Remote-server export and CLAPI worker are not ported.
            "SENDEXPORTFILE" | "STARTWORKER" => {
                warn!(
                    command = line.command,
                    line = text,
                    "unsupported centcore command dropped"
                );
                metrics::CENTCORE_UNSUPPORTED.add(line.command, 1);
            }
            command => match service_control(command) {
                Some((service, operation)) => {
                    let kind = Kind::ServiceControl(ServiceControl {
                        service: service.to_owned(),
                        operation: operation.into(),
                    });
                    self.submit(&id("control"), line.target, kind, text)?;
                }
                None => Self::dropped(text, "unknown"),
            },
        }
        reading.mark(offset);
        Ok(())
    }

    /// Sends the external commands waiting for `target` as one job.
    fn flush(&mut self, target: &str, reading: &mut Reading) -> Result<(), Abort> {
        let Some(pending) = reading
            .pending
            .remove(target)
            .filter(|p| !p.commands.is_empty())
        else {
            return Ok(());
        };
        let description = format!("EXTERNALCMD x{} for {target}", pending.commands.len());
        // Named after its first line: a replay rebuilds the same batch.
        let id = job_id(&reading.nonce, pending.offsets[0], "external");
        let kind = Kind::EngineCommands(EngineCommands {
            commands: pending.commands,
        });
        self.submit(&id, target, kind, &description)?;
        for offset in pending.offsets {
            reading.mark(offset);
        }
        Ok(())
    }

    /// One `ApplyConfig` per configuration present in the cache for the node.
    fn send_config(
        &mut self,
        line: &Line<'_>,
        text: &str,
        id: &impl Fn(&str) -> String,
    ) -> Result<(), Abort> {
        let root = self.intake.config.cache_dir.clone();
        let config = root.join("config");
        // Optional: only nodes running the VMware connector have it.
        let vmware = config
            .join("vmware")
            .join(line.target)
            .join("centreon_vmware.json");
        if vmware.is_file() {
            self.apply_config(line.target, "vmware", &root, &vmware, text, &id("vmware"))?;
        }
        for target in ["engine", "broker"] {
            let source = config.join(target).join(line.target);
            if source.is_dir() {
                self.apply_config(line.target, target, &root, &source, text, &id(target))?;
            } else {
                warn!(line = text, source = %source.display(), "configuration not generated, nothing sent");
            }
        }
        Ok(())
    }

    fn apply_config(
        &mut self,
        node: &str,
        target: &str,
        root: &Path,
        source: &Path,
        text: &str,
        id: &str,
    ) -> Result<(), Abort> {
        // The source must really live under its root: no link may lead the
        // packing elsewhere (the packer itself refuses a linked root).
        let inside = fs::canonicalize(root)
            .and_then(|root| Ok(fs::canonicalize(source)?.starts_with(root)))
            .unwrap_or(false);
        let linked = fs::symlink_metadata(source).map_or(true, |meta| meta.is_symlink());
        if !inside || linked {
            warn!(line = text, source = %source.display(), "configuration source is outside its directory, nothing sent");
            metrics::CENTCORE_DROPPED.add("unsafe_source", 1);
            return Ok(());
        }
        let bytes = match bundle::pack(source) {
            Ok(bytes) => bytes,
            Err(err) => {
                warn!(line = text, error = %err, "cannot pack the configuration, nothing sent");
                return Ok(());
            }
        };
        let (digest, size) = self
            .runtime
            .block_on(self.intake.store.put_blob(&bytes))
            .map_err(|err| {
                warn!(line = text, error = %err, "cannot store the configuration, will retry");
                Abort
            })?;
        let kind = Kind::ApplyConfig(ApplyConfig {
            target: target.to_owned(),
            digest,
            size: u64::try_from(size).unwrap_or_default(),
        });
        self.submit(id, node, kind, text)
    }

    /// Submits one job under a deterministic id. A refused action or an
    /// unknown node is logged and dropped; a store failure or a lost lease
    /// aborts the pass so that the line is read again.
    fn submit(&mut self, id: &str, node: &str, kind: Kind, text: &str) -> Result<(), Abort> {
        // Still the holder? Another replica may have taken over meanwhile.
        self.ensure_lease()?;
        let action = Action { kind: Some(kind) };
        let submitted = self.runtime.block_on(
            self.intake
                .dispatcher
                .submit_with_id(id, node, action, None),
        );
        match submitted {
            Ok(true) => {
                debug!(job = id, node, "centcore command submitted");
                metrics::CENTCORE_JOBS.inc(1);
                self.stats.jobs += 1;
                Ok(())
            }
            Ok(false) => {
                debug!(
                    job = id,
                    node, "centcore command already submitted by a previous run"
                );
                Ok(())
            }
            Err(Error::UnknownNode(_)) => {
                Self::dropped(text, "unknown_node");
                Ok(())
            }
            Err(Error::Invalid(reason)) => {
                warn!(line = text, reason, "centcore command refused");
                Self::dropped(text, "refused");
                Ok(())
            }
            Err(err) => {
                warn!(line = text, error = %err, "centcore command not submitted, will retry");
                Err(Abort)
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use prost::Message;

    use super::*;
    use crate::dispatcher::DispatcherConfig;
    use crate::hub::Hub;
    use crate::peer::PeerClients;
    use crate::store::testing;

    struct Fixture {
        dir: tempfile::TempDir,
        store: Store,
        config: CentcoreConfig,
        lease: String,
        auto_enroll: bool,
    }

    impl Fixture {
        async fn new() -> Self {
            let (store, dir) = testing::store().await;
            let config = CentcoreConfig {
                enabled: true,
                cmd_file: dir.path().join("centcore.cmd"),
                cmd_dir: dir.path().join("centcore"),
                cache_dir: dir.path().join("cache"),
                cache_dir_trap: dir.path().join("traps"),
                ..CentcoreConfig::default()
            };
            fs::create_dir_all(&config.cmd_dir).unwrap();
            Self {
                store,
                config,
                // Tests may share one database: one lease each.
                lease: testing::unique("centcore"),
                auto_enroll: true,
                dir,
            }
        }

        fn intake(&self, replica: &str) -> Arc<Intake> {
            let dispatcher = Arc::new(Dispatcher::new(
                self.store.clone(),
                Arc::new(Hub::new(Duration::from_secs(1))),
                PeerClients::default(),
                DispatcherConfig {
                    replica_id: replica.to_owned(),
                    peer_addr: "http://127.0.0.1:1".into(),
                    default_deadline: Duration::from_secs(60),
                    validation_deadline: Duration::from_secs(120),
                    sweep_interval: Duration::from_secs(1),
                    retention: Duration::from_secs(60),
                    auto_enroll: self.auto_enroll,
                },
            ));
            Arc::new(Intake::with_lease(
                self.config.clone(),
                dispatcher,
                self.store.clone(),
                replica.to_owned(),
                self.lease.clone(),
            ))
        }

        async fn tick(&self, intake: &Arc<Intake>) -> Option<PassStats> {
            intake.tick(&CancellationToken::new()).await.unwrap()
        }

        /// Kinds of the jobs of a node, in submission order.
        async fn jobs(&self, node: &str) -> Vec<Kind> {
            self.store
                .jobs_of_node(node)
                .await
                .unwrap()
                .iter()
                .map(|job| Action::decode(job.action.as_slice()).unwrap().kind.unwrap())
                .collect()
        }

        /// Names left in the directory of the main command file.
        fn left(&self) -> Vec<String> {
            let mut names: Vec<String> = fs::read_dir(self.dir.path())
                .unwrap()
                .map(|entry| entry.unwrap().file_name().into_string().unwrap())
                .filter(|name| name.contains("centcore.cmd"))
                .collect();
            names.sort();
            names
        }
    }

    fn node() -> String {
        testing::unique("n")
    }

    fn engine(commands: &[&str]) -> Kind {
        Kind::EngineCommands(EngineCommands {
            commands: commands.iter().map(|c| (*c).to_owned()).collect(),
        })
    }

    fn control(service: &str, operation: Operation) -> Kind {
        Kind::ServiceControl(ServiceControl {
            service: service.to_owned(),
            operation: operation.into(),
        })
    }

    #[test]
    fn line_format() {
        assert_eq!(
            parse_line("EXTERNALCMD:12:[1] CMD;a:b"),
            Some(Line {
                command: "EXTERNALCMD",
                target: "12",
                param: Some("[1] CMD;a:b")
            })
        );
        assert_eq!(
            parse_line("RELOAD:3"),
            Some(Line {
                command: "RELOAD",
                target: "3",
                param: None
            })
        );
        assert_eq!(parse_line("no separator"), None);
        assert_eq!(parse_line(":3"), None);
        assert!(is_safe_target("12"));
        for bad in ["", ".", "..", "../1", "a/b", "a\0b", "a\tb"] {
            assert!(!is_safe_target(bad), "{bad:?}");
        }
    }

    #[test]
    fn read_names_and_job_ids() {
        let nonce = "0123456789abcdef0123456789abcdef";
        assert_eq!(
            split_nonce(&format!("centcore.cmd.{nonce}_read")),
            Some(("centcore.cmd", nonce))
        );
        assert_eq!(split_nonce("centcore.cmd_read"), None);
        assert_eq!(split_nonce("centcore.cmd.short_read"), None);
        // Stable for a given place, distinct for anything else.
        assert_eq!(job_id(nonce, 10, "engine"), job_id(nonce, 10, "engine"));
        assert_ne!(job_id(nonce, 10, "engine"), job_id(nonce, 10, "broker"));
        assert_ne!(job_id(nonce, 10, "engine"), job_id(nonce, 11, "engine"));
        assert_ne!(
            job_id(nonce, 10, "engine"),
            job_id(&nonce.replace('0', "f"), 10, "engine")
        );
    }

    #[tokio::test(flavor = "multi_thread")]
    async fn service_commands_map_to_service_control() {
        let fixture = Fixture::new().await;
        let node = node();
        let lines: String = [
            "RELOAD",
            "ENGINERELOAD",
            "RESTART",
            "ENGINERESTART",
            "START",
            "STOP",
            "RELOADBROKER",
            "RELOADCENTREONTRAPD",
            "RESTARTCENTREONTRAPD",
            "VMWARERESTART",
        ]
        .iter()
        .map(|command| format!("{command}:{node}\n"))
        .collect();
        fs::write(&fixture.config.cmd_file, lines).unwrap();

        let stats = fixture.tick(&fixture.intake("a")).await.unwrap();
        assert_eq!(stats, PassStats { files: 1, jobs: 10 });
        assert_eq!(
            fixture.jobs(&node).await,
            [
                control("engine", Operation::Reload),
                control("engine", Operation::Reload),
                control("engine", Operation::Restart),
                control("engine", Operation::Restart),
                control("engine", Operation::Start),
                control("engine", Operation::Stop),
                control("broker", Operation::Reload),
                control("centreontrapd", Operation::Reload),
                control("centreontrapd", Operation::Restart),
                control("vmware", Operation::Restart),
            ]
        );
    }

    #[tokio::test(flavor = "multi_thread")]
    async fn external_commands_are_batched_and_keep_their_order() {
        let mut fixture = Fixture::new().await;
        fixture.config.bulk_external_cmd = 2;
        fixture.config.illegal_characters = "~!".into();
        let (first, second) = (node(), node());
        let lines = format!(
            "EXTERNALCMD:{first}:[1] A;h~ost\n\
             EXTERNALCMD:{second}:[1] OTHER\n\
             EXTERNALCMD:{first}:[2] B!\n\
             EXTERNALCMD:{first}:[3] C\n\
             RELOAD:{first}\n\
             EXTERNALCMD:{first}:[4] D:with:colons\tand tab\n\
             EXTERNALCMD:{first}:[5] bad\u{1b}[31m escape\n\
             EXTERNALCMD:{first}:\n"
        );
        fs::write(&fixture.config.cmd_file, lines).unwrap();

        let stats = fixture.tick(&fixture.intake("a")).await.unwrap();
        assert_eq!(stats, PassStats { files: 1, jobs: 5 });
        // Batch of two, then the rest flushed before the reload, then the
        // tail flushed at the end of the file. The control character and
        // the empty command are dropped.
        assert_eq!(
            fixture.jobs(&first).await,
            [
                engine(&["[1] A;host", "[2] B"]),
                engine(&["[3] C"]),
                control("engine", Operation::Reload),
                engine(&["[4] D:with:colons\tand tab"]),
            ]
        );
        assert_eq!(fixture.jobs(&second).await, [engine(&["[1] OTHER"])]);
    }

    #[tokio::test(flavor = "multi_thread")]
    async fn sendcfgfile_and_synctrap_ship_bundles() {
        let fixture = Fixture::new().await;
        let (full, partial) = (node(), node());
        let config = fixture.config.cache_dir.join("config");
        for (kind, file) in [
            ("engine", "centengine.cfg"),
            ("broker", "central-module.json"),
        ] {
            let dir = config.join(kind).join(&full);
            fs::create_dir_all(&dir).unwrap();
            fs::write(dir.join(file), format!("{kind} of {full}")).unwrap();
        }
        let vmware = config.join("vmware").join(&full);
        fs::create_dir_all(&vmware).unwrap();
        fs::write(vmware.join("centreon_vmware.json"), "{}").unwrap();
        let traps = fixture.config.cache_dir_trap.join(&full);
        fs::create_dir_all(&traps).unwrap();
        fs::write(traps.join("centreontrapd.sdb"), "sqlite").unwrap();
        // The second node has a broker configuration, and an engine
        // "configuration" that is a link leading out of the cache.
        let broker_only = config.join("broker").join(&partial);
        fs::create_dir_all(&broker_only).unwrap();
        fs::write(broker_only.join("poller-module.json"), "{}").unwrap();
        let outside = fixture.dir.path().join("outside");
        fs::create_dir_all(&outside).unwrap();
        fs::write(outside.join("secret"), "not for pollers").unwrap();
        std::os::unix::fs::symlink(&outside, config.join("engine").join(&partial)).unwrap();

        fs::write(
            &fixture.config.cmd_file,
            format!(
                "SENDCFGFILE:{full}\nSYNCTRAP:{full}\nSENDCFGFILE:{partial}\nSYNCTRAP:{partial}\n"
            ),
        )
        .unwrap();
        let stats = fixture.tick(&fixture.intake("a")).await.unwrap();
        assert_eq!(stats, PassStats { files: 1, jobs: 5 });

        let jobs = fixture.jobs(&full).await;
        let targets: Vec<&str> = jobs
            .iter()
            .map(|kind| match kind {
                Kind::ApplyConfig(apply) => apply.target.as_str(),
                other => panic!("unexpected {other:?}"),
            })
            .collect();
        assert_eq!(targets, ["vmware", "engine", "broker", "centreontrapd"]);
        // The blob is stored and is the bundle of the cache directory.
        let Kind::ApplyConfig(engine) = &jobs[1] else {
            unreachable!()
        };
        let expected = bundle::pack(&config.join("engine").join(&full)).unwrap();
        assert_eq!(engine.digest, bundle::digest(&expected));
        assert_eq!(engine.size, expected.len() as u64);
        assert_eq!(
            fixture.store.blob_size(&engine.digest).await.unwrap(),
            Some(expected.len() as i64)
        );
        let chunk = fixture.store.blob_chunk(&engine.digest, 0).await.unwrap();
        assert_eq!(chunk.unwrap(), expected);

        // The linked engine directory was not packed.
        let jobs = fixture.jobs(&partial).await;
        assert_eq!(jobs.len(), 1);
        assert!(matches!(&jobs[0], Kind::ApplyConfig(apply) if apply.target == "broker"));
    }

    #[tokio::test(flavor = "multi_thread")]
    async fn files_are_consumed_exactly_once() {
        let fixture = Fixture::new().await;
        let node = node();
        let intake = fixture.intake("a");
        fs::write(&fixture.config.cmd_file, format!("RELOAD:{node}\n")).unwrap();
        // Directory: a hidden file is ignored, a leftover of the Perl daemon
        // resumes after its handled lines.
        let dir = &fixture.config.cmd_dir;
        fs::write(dir.join(".hidden"), format!("STOP:{node}\n")).unwrap();
        fs::write(
            dir.join("old.cmd_read"),
            format!("-ESTART:{node}\nRELOADBROKER:{node}\n"),
        )
        .unwrap();
        fs::write(dir.join("new.cmd"), format!("EXTERNALCMD:{node}:[1] X\n")).unwrap();

        let stats = fixture.tick(&intake).await.unwrap();
        assert_eq!(stats, PassStats { files: 3, jobs: 3 });
        let mut jobs = fixture.jobs(&node).await;
        assert_eq!(jobs.remove(0), control("engine", Operation::Reload));
        // Directory files follow, in change-time order.
        jobs.sort_by_key(|kind| matches!(kind, Kind::EngineCommands(_)));
        assert_eq!(
            jobs,
            [control("broker", Operation::Reload), engine(&["[1] X"])]
        );

        // Everything was removed but the hidden file; nothing is replayed.
        assert!(fixture.left().is_empty());
        let left: Vec<_> = fs::read_dir(dir)
            .unwrap()
            .map(|entry| entry.unwrap().file_name())
            .collect();
        assert_eq!(left, [".hidden"]);
        let stats = fixture.tick(&intake).await.unwrap();
        assert_eq!(stats, PassStats::default());
        assert_eq!(fixture.jobs(&node).await.len(), 3);
    }

    #[tokio::test(flavor = "multi_thread")]
    async fn read_only_files_are_consumed() {
        use std::os::unix::fs::PermissionsExt;

        // centreon-web drops one file per command, not writable by the daemon.
        let fixture = Fixture::new().await;
        let node = node();
        let intake = fixture.intake("a");
        let file = fixture.config.cmd_dir.join("1-externalcommand.cmd");
        fs::write(&file, format!("RESTART:{node}\n")).unwrap();
        fs::set_permissions(&file, fs::Permissions::from_mode(0o444)).unwrap();

        let stats = fixture.tick(&intake).await.unwrap();
        assert_eq!(stats, PassStats { files: 1, jobs: 1 });
        assert_eq!(
            fixture.jobs(&node).await,
            [control("engine", Operation::Restart)]
        );
        assert_eq!(fs::read_dir(&fixture.config.cmd_dir).unwrap().count(), 0);
    }

    #[tokio::test(flavor = "multi_thread")]
    async fn replay_of_a_file_does_not_duplicate_its_commands() {
        let mut fixture = Fixture::new().await;
        fixture.config.bulk_external_cmd = 2;
        let node = node();
        let config = fixture.config.cache_dir.join("config/broker").join(&node);
        fs::create_dir_all(&config).unwrap();
        fs::write(config.join("module.json"), "{}").unwrap();
        // A file a previous run was reading: it already has its nonce.
        let nonce = crate::random_id().unwrap();
        let reading = fixture
            .dir
            .path()
            .join(format!("centcore.cmd.{nonce}_read"));
        let content = format!(
            "EXTERNALCMD:{node}:[1] A\nEXTERNALCMD:{node}:[2] B\nEXTERNALCMD:{node}:[3] C\n\
             SENDCFGFILE:{node}\nRELOAD:{node}\n"
        );
        fs::write(&reading, &content).unwrap();
        let stats = fixture.tick(&fixture.intake("a")).await.unwrap();
        assert_eq!(stats, PassStats { files: 1, jobs: 4 });
        assert!(fixture.left().is_empty());

        // The crash case: the jobs were committed but neither the marks nor
        // the removal reached the disk. Another replica reads it all again.
        fs::write(&reading, &content).unwrap();
        fixture
            .store
            .release_lease(&fixture.lease, "a")
            .await
            .unwrap();
        let stats = fixture.tick(&fixture.intake("b")).await.unwrap();
        assert_eq!(stats, PassStats { files: 1, jobs: 0 });
        assert_eq!(fixture.jobs(&node).await.len(), 4);
        assert!(fixture.left().is_empty());
    }

    #[tokio::test(flavor = "multi_thread")]
    async fn unusable_lines_are_dropped_without_job() {
        let fixture = Fixture::new().await;
        let node = node();
        fs::write(
            &fixture.config.cmd_file,
            format!(
                "SENDEXPORTFILE:{node}:42\nSTARTWORKER:\nNOSUCHCOMMAND:{node}\ngarbage\n\
                 RELOAD:../{node}\nSENDCFGFILE:{node}\nSYNCTRAP:{node}\n\nSTOP:{node}\r\n"
            ),
        )
        .unwrap();
        let stats = fixture.tick(&fixture.intake("a")).await.unwrap();
        assert_eq!(stats, PassStats { files: 1, jobs: 1 });
        assert_eq!(
            fixture.jobs(&node).await,
            [control("engine", Operation::Stop)]
        );
        assert!(fixture.left().is_empty());
    }

    #[tokio::test(flavor = "multi_thread")]
    async fn commands_for_an_unknown_node_create_no_job() {
        let mut fixture = Fixture::new().await;
        fixture.auto_enroll = false;
        let (unknown, enrolled) = (node(), node());
        fixture.store.enrol_node(&enrolled, "poller").await.unwrap();
        let config = fixture
            .config
            .cache_dir
            .join("config/broker")
            .join(&unknown);
        fs::create_dir_all(&config).unwrap();
        fs::write(config.join("module.json"), "{}").unwrap();
        fs::write(
            &fixture.config.cmd_file,
            format!(
                "RELOAD:{unknown}\nEXTERNALCMD:{unknown}:[1] X\nSENDCFGFILE:{unknown}\n\
                 RELOAD:{enrolled}\n"
            ),
        )
        .unwrap();
        let stats = fixture.tick(&fixture.intake("a")).await.unwrap();
        assert_eq!(stats, PassStats { files: 1, jobs: 1 });
        assert!(fixture.jobs(&unknown).await.is_empty());
        assert_eq!(
            fixture.jobs(&enrolled).await,
            [control("engine", Operation::Reload)]
        );
    }

    #[tokio::test(flavor = "multi_thread")]
    async fn links_and_foreign_files_are_never_read() {
        let mut fixture = Fixture::new().await;
        let node = node();
        // A link planted as the command file: its target must stay intact.
        let target = fixture.dir.path().join("target");
        fs::write(&target, format!("RELOAD:{node}\n")).unwrap();
        std::os::unix::fs::symlink(&target, &fixture.config.cmd_file).unwrap();
        std::os::unix::fs::symlink(&target, fixture.config.cmd_dir.join("link.cmd")).unwrap();
        let stats = fixture.tick(&fixture.intake("a")).await.unwrap();
        assert_eq!(stats, PassStats::default());
        assert_eq!(
            fs::read_to_string(&target).unwrap(),
            format!("RELOAD:{node}\n")
        );
        fs::remove_file(&fixture.config.cmd_file).unwrap();

        // A regular file owned by a user that is not allowed: set aside.
        fixture.config.allowed_uids = vec![u32::MAX - 7];
        fs::write(&fixture.config.cmd_file, format!("STOP:{node}\n")).unwrap();
        let stats = fixture.tick(&fixture.intake("a")).await.unwrap();
        assert_eq!(stats, PassStats::default());
        assert!(fixture.jobs(&node).await.is_empty());
        let left = fixture.left();
        assert_eq!(left.len(), 1, "{left:?}");
        assert!(left[0].starts_with(".rejected-centcore.cmd."));
        // Reported once: the next pass does not see it.
        let stats = fixture.tick(&fixture.intake("a")).await.unwrap();
        assert_eq!(stats, PassStats::default());
    }

    #[tokio::test(flavor = "multi_thread")]
    async fn lease_keeps_a_second_replica_away() {
        let fixture = Fixture::new().await;
        let node = node();
        let (holder, other) = (fixture.intake("a"), fixture.intake("b"));

        fs::write(&fixture.config.cmd_file, format!("RELOAD:{node}\n")).unwrap();
        assert_eq!(
            fixture.tick(&holder).await,
            Some(PassStats { files: 1, jobs: 1 })
        );

        // The other replica does not hold the lease: the file stays.
        fs::write(&fixture.config.cmd_file, format!("STOP:{node}\n")).unwrap();
        assert_eq!(fixture.tick(&other).await, None);
        assert!(fixture.config.cmd_file.exists());
        assert_eq!(fixture.jobs(&node).await.len(), 1);

        assert_eq!(
            fixture.tick(&holder).await,
            Some(PassStats { files: 1, jobs: 1 })
        );
        assert_eq!(
            fixture.jobs(&node).await,
            [
                control("engine", Operation::Reload),
                control("engine", Operation::Stop)
            ]
        );

        // Once released, the other replica takes over.
        fixture
            .store
            .release_lease(&fixture.lease, "a")
            .await
            .unwrap();
        fs::write(&fixture.config.cmd_file, format!("START:{node}\n")).unwrap();
        assert_eq!(
            fixture.tick(&other).await,
            Some(PassStats { files: 1, jobs: 1 })
        );
    }
}
