//! Legacy `centcore.cmd` intake: reads the command files still written by
//! centreon-web (`CMD:target[:param]` lines) and submits the equivalent jobs.
//!
//! Singleton work: only the replica holding the lease reads the files. A
//! file is renamed to `<name>_read` before being read, each handled line is
//! overwritten with a leading `-`, and the file is removed at the end, so a
//! restart resumes where the previous run stopped.

use std::collections::{BTreeMap, HashSet};
use std::fs::{self, File, OpenOptions};
use std::os::unix::fs::{FileExt, MetadataExt};
use std::path::{Path, PathBuf};
use std::sync::Arc;
use std::time::Duration;

use gorgone_core::bundle;
use gorgone_proto::agent::action::Kind;
use gorgone_proto::agent::service_control::Operation;
use gorgone_proto::agent::{Action, ApplyConfig, EngineCommands, ServiceControl};
use tokio::runtime::Handle;
use tokio_util::sync::CancellationToken;
use tracing::{debug, info, warn};

use crate::config::CentcoreConfig;
use crate::dispatcher::Dispatcher;
use crate::error::Error;
use crate::store::Store;

/// Name of the lease guarding the intake.
const LEASE: &str = "centcore";
/// Suffix of a file being read.
const READ_SUFFIX: &str = "_read";
/// Lines handled between two lease renewals.
const RENEW_EVERY: usize = 200;

/// What one pass over the command files did.
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq)]
pub(crate) struct PassStats {
    /// Files read to their end and removed.
    pub(crate) files: usize,
    /// Jobs submitted.
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
        && !target.contains(['/', '\\', '\0'])
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

fn with_suffix(path: &Path) -> PathBuf {
    let mut name = path.as_os_str().to_owned();
    name.push(READ_SUFFIX);
    PathBuf::from(name)
}

/// Returns the file to read for `path`: a leftover of a previous run first,
/// else `path` renamed. `None` when there is nothing to read.
fn claim(path: &Path) -> Option<PathBuf> {
    let reading = with_suffix(path);
    if reading.is_file() {
        return Some(reading);
    }
    if !path.is_file() {
        return None;
    }
    match fs::rename(path, &reading) {
        Ok(()) => Some(reading),
        Err(err) => {
            warn!(file = %path.display(), error = %err, "cannot move command file");
            None
        }
    }
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
    file: File,
    pending: BTreeMap<String, Pending>,
}

impl Reading {
    /// Overwrites the first byte of a handled line, like the Perl daemon.
    fn mark(&self, offset: u64) {
        if let Err(err) = self.file.write_at(b"-", offset) {
            // Worst case the line is replayed after a restart.
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
}

impl Intake {
    pub(crate) fn new(
        config: CentcoreConfig,
        dispatcher: Arc<Dispatcher>,
        store: Store,
        replica_id: String,
    ) -> Arc<Self> {
        Arc::new(Self {
            config,
            dispatcher,
            store,
            replica_id,
            lease: LEASE.to_owned(),
        })
    }

    fn lease_ttl_ms(&self) -> i64 {
        let ttl = Duration::from_millis(self.config.poll_interval_ms.saturating_mul(3))
            .max(Duration::from_secs(15));
        i64::try_from(ttl.as_millis()).unwrap_or(i64::MAX)
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

    /// One poll: `None` when another replica holds the lease.
    pub(crate) async fn tick(
        self: &Arc<Self>,
        shutdown: &CancellationToken,
    ) -> Result<Option<PassStats>, Error> {
        if !self
            .store
            .acquire_lease(&self.lease, &self.replica_id, self.lease_ttl_ms())
            .await?
        {
            return Ok(None);
        }
        let this = Arc::clone(self);
        let runtime = Handle::current();
        let stop = shutdown.clone();
        // File access and packing are blocking: keep them off the workers.
        tokio::task::spawn_blocking(move || this.pass(&runtime, &stop))
            .await
            .map(Some)
            .map_err(|err| Error::Invalid(format!("centcore pass aborted: {err}")))
    }

    /// Reads the main command file, then the files of the directory, oldest
    /// first.
    fn pass(&self, runtime: &Handle, stop: &CancellationToken) -> PassStats {
        let mut stats = PassStats::default();
        let mut files = Vec::new();
        if let Some(file) = claim(&self.config.cmd_file) {
            files.push(file);
        }
        let mut seen: HashSet<PathBuf> = files.iter().cloned().collect();
        for path in self.directory_files() {
            let claimed = if path.to_string_lossy().ends_with(READ_SUFFIX) {
                Some(path)
            } else {
                claim(&path)
            };
            if let Some(file) = claimed
                && seen.insert(file.clone())
            {
                files.push(file);
            }
        }
        for file in files {
            if self.handle_file(&file, runtime, stop, &mut stats).is_err() {
                break;
            }
        }
        stats
    }

    /// Regular, non-hidden files of the command directory, oldest first.
    fn directory_files(&self) -> Vec<PathBuf> {
        let dir = &self.config.cmd_dir;
        let entries = match fs::read_dir(dir) {
            Ok(entries) => entries,
            Err(err) if err.kind() == std::io::ErrorKind::NotFound => return Vec::new(),
            Err(err) => {
                warn!(dir = %dir.display(), error = %err, "cannot read the command directory");
                return Vec::new();
            }
        };
        let mut files: Vec<_> = entries
            .filter_map(Result::ok)
            .filter(|entry| !entry.file_name().to_string_lossy().starts_with('.'))
            .filter_map(|entry| {
                let meta = entry.metadata().ok().filter(fs::Metadata::is_file)?;
                Some((
                    (meta.ctime(), meta.ctime_nsec(), entry.file_name()),
                    entry.path(),
                ))
            })
            .collect();
        files.sort();
        files.into_iter().map(|(_, path)| path).collect()
    }

    fn handle_file(
        &self,
        path: &Path,
        runtime: &Handle,
        stop: &CancellationToken,
        stats: &mut PassStats,
    ) -> Result<(), Abort> {
        let open = || -> std::io::Result<(Vec<u8>, File)> {
            let content = fs::read(path)?;
            Ok((content, OpenOptions::new().write(true).open(path)?))
        };
        let (content, file) = open().map_err(|err| {
            warn!(file = %path.display(), error = %err, "cannot read command file");
            Abort
        })?;
        debug!(file = %path.display(), "processing command file");
        let mut reading = Reading {
            file,
            pending: BTreeMap::new(),
        };

        let mut offset = 0u64;
        for (index, raw) in content.split(|byte| *byte == b'\n').enumerate() {
            let line_offset = offset;
            offset += raw.len() as u64 + 1;
            if stop.is_cancelled() {
                return Err(Abort);
            }
            if index % RENEW_EVERY == RENEW_EVERY - 1 {
                self.renew_lease(runtime)?;
            }
            let text = String::from_utf8_lossy(raw);
            let text = text.trim_end_matches('\r');
            // A leading dash marks a line handled by a previous run.
            if text.is_empty() || text.starts_with('-') {
                continue;
            }
            self.handle_line(text, line_offset, &mut reading, runtime, stats)?;
        }
        let targets: Vec<String> = reading.pending.keys().cloned().collect();
        for target in targets {
            self.flush(&target, &mut reading, runtime, stats)?;
        }
        drop(reading);
        if let Err(err) = fs::remove_file(path) {
            warn!(file = %path.display(), error = %err, "cannot remove command file");
        }
        stats.files += 1;
        Ok(())
    }

    fn renew_lease(&self, runtime: &Handle) -> Result<(), Abort> {
        let renewed = runtime.block_on(self.store.acquire_lease(
            &self.lease,
            &self.replica_id,
            self.lease_ttl_ms(),
        ));
        match renewed {
            Ok(true) => Ok(()),
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

    fn handle_line(
        &self,
        text: &str,
        offset: u64,
        reading: &mut Reading,
        runtime: &Handle,
        stats: &mut PassStats,
    ) -> Result<(), Abort> {
        let Some(line) = parse_line(text).filter(|line| {
            // STARTWORKER has no target; everything else needs a usable one.
            is_safe_target(line.target) || matches!(line.command, "STARTWORKER")
        }) else {
            warn!(line = text, "malformed centcore command dropped");
            metrics::counter!("gorgone_centcore_dropped_total", "reason" => "malformed")
                .increment(1);
            reading.mark(offset);
            return Ok(());
        };
        debug!(
            command = line.command,
            target = line.target,
            "handling centcore command"
        );

        if line.command == "EXTERNALCMD" {
            let param: String = line
                .param
                .unwrap_or_default()
                .chars()
                .filter(|c| !self.config.illegal_characters.contains(*c))
                .collect();
            if param.is_empty() {
                warn!(line = text, "external command without content dropped");
                metrics::counter!("gorgone_centcore_dropped_total", "reason" => "malformed")
                    .increment(1);
                reading.mark(offset);
                return Ok(());
            }
            let pending = reading.pending.entry(line.target.to_owned()).or_default();
            pending.commands.push(param);
            pending.offsets.push(offset);
            if pending.commands.len() >= self.config.bulk_external_cmd {
                self.flush(line.target, reading, runtime, stats)?;
            }
            return Ok(());
        }

        // Keep the order: what was written before this command goes first.
        self.flush(line.target, reading, runtime, stats)?;
        match line.command {
            "SENDCFGFILE" => self.send_config(&line, text, runtime, stats)?,
            "SYNCTRAP" => {
                let source = self
                    .config
                    .cache_dir_trap
                    .join(line.target)
                    .join("centreontrapd.sdb");
                if source.is_file() {
                    self.apply_config(line.target, "centreontrapd", &source, text, runtime, stats)?;
                } else {
                    warn!(line = text, source = %source.display(), "trap database not found, command dropped");
                }
            }
            // Remote-server export and CLAPI worker are not ported.
            "SENDEXPORTFILE" | "STARTWORKER" => {
                warn!(
                    command = line.command,
                    line = text,
                    "unsupported centcore command dropped"
                );
                metrics::counter!(
                    "gorgone_centcore_unsupported_total",
                    "command" => line.command.to_owned()
                )
                .increment(1);
            }
            command => match service_control(command) {
                Some((service, operation)) => {
                    let kind = Kind::ServiceControl(ServiceControl {
                        service: service.to_owned(),
                        operation: operation.into(),
                    });
                    self.submit(line.target, kind, text, runtime, stats)?;
                }
                None => {
                    warn!(line = text, "unknown centcore command dropped");
                    metrics::counter!("gorgone_centcore_dropped_total", "reason" => "unknown")
                        .increment(1);
                }
            },
        }
        reading.mark(offset);
        Ok(())
    }

    /// Sends the external commands waiting for `target` as one job.
    fn flush(
        &self,
        target: &str,
        reading: &mut Reading,
        runtime: &Handle,
        stats: &mut PassStats,
    ) -> Result<(), Abort> {
        let Some(pending) = reading
            .pending
            .remove(target)
            .filter(|p| !p.commands.is_empty())
        else {
            return Ok(());
        };
        let description = format!("EXTERNALCMD x{} for {target}", pending.commands.len());
        let kind = Kind::EngineCommands(EngineCommands {
            commands: pending.commands,
        });
        self.submit(target, kind, &description, runtime, stats)?;
        for offset in pending.offsets {
            reading.mark(offset);
        }
        Ok(())
    }

    /// One `ApplyConfig` per configuration present in the cache for the node.
    fn send_config(
        &self,
        line: &Line<'_>,
        text: &str,
        runtime: &Handle,
        stats: &mut PassStats,
    ) -> Result<(), Abort> {
        let config = self.config.cache_dir.join("config");
        // Optional: only nodes running the VMware connector have it.
        let vmware = config
            .join("vmware")
            .join(line.target)
            .join("centreon_vmware.json");
        if vmware.is_file() {
            self.apply_config(line.target, "vmware", &vmware, text, runtime, stats)?;
        }
        for target in ["engine", "broker"] {
            let source = config.join(target).join(line.target);
            if source.is_dir() {
                self.apply_config(line.target, target, &source, text, runtime, stats)?;
            } else {
                warn!(line = text, source = %source.display(), "configuration not generated, nothing sent");
            }
        }
        Ok(())
    }

    fn apply_config(
        &self,
        node: &str,
        target: &str,
        source: &Path,
        text: &str,
        runtime: &Handle,
        stats: &mut PassStats,
    ) -> Result<(), Abort> {
        let bytes = match bundle::pack(source) {
            Ok(bytes) => bytes,
            Err(err) => {
                warn!(line = text, error = %err, "cannot pack the configuration, nothing sent");
                return Ok(());
            }
        };
        let (digest, size) = runtime
            .block_on(self.store.put_blob(&bytes))
            .map_err(|err| {
                warn!(line = text, error = %err, "cannot store the configuration, will retry");
                Abort
            })?;
        let kind = Kind::ApplyConfig(ApplyConfig {
            target: target.to_owned(),
            digest,
            size: u64::try_from(size).unwrap_or_default(),
        });
        self.submit(node, kind, text, runtime, stats)
    }

    /// Submits one job. A refused action is logged and dropped; a store
    /// failure aborts the pass so that the line is read again.
    fn submit(
        &self,
        node: &str,
        kind: Kind,
        text: &str,
        runtime: &Handle,
        stats: &mut PassStats,
    ) -> Result<(), Abort> {
        let action = Action { kind: Some(kind) };
        match runtime.block_on(self.dispatcher.submit(node, action, None)) {
            Ok(job) => {
                debug!(job = %job, node, "centcore command submitted");
                metrics::counter!("gorgone_centcore_jobs_total").increment(1);
                stats.jobs += 1;
                Ok(())
            }
            Err(Error::Invalid(reason)) => {
                warn!(line = text, reason, "centcore command refused and dropped");
                metrics::counter!("gorgone_centcore_dropped_total", "reason" => "refused")
                    .increment(1);
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

    struct Fixture {
        _dir: tempfile::TempDir,
        store: Store,
        config: CentcoreConfig,
        lease: String,
    }

    impl Fixture {
        async fn new() -> Self {
            let dir = tempfile::tempdir().unwrap();
            let url = std::env::var("GORGONE_TEST_DATABASE_URL").unwrap_or_else(|_| {
                format!("sqlite://{}/gorgone.db?mode=rwc", dir.path().display())
            });
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
                store: Store::connect(&url).await.unwrap(),
                config,
                // Tests may share one database: one lease each.
                lease: format!("centcore-{}", uuid::Uuid::now_v7()),
                _dir: dir,
            }
        }

        fn intake(&self, replica: &str) -> Arc<Intake> {
            let dispatcher = Arc::new(Dispatcher::new(
                self.store.clone(),
                Arc::new(Hub::new()),
                PeerClients::default(),
                DispatcherConfig {
                    replica_id: replica.to_owned(),
                    peer_addr: "http://127.0.0.1:1".into(),
                    default_deadline: Duration::from_secs(60),
                    sweep_interval: Duration::from_secs(1),
                    retention: Duration::from_secs(60),
                },
            ));
            Arc::new(Intake {
                config: self.config.clone(),
                dispatcher,
                store: self.store.clone(),
                replica_id: replica.to_owned(),
                lease: self.lease.clone(),
            })
        }

        async fn tick(&self, intake: &Arc<Intake>) -> Option<PassStats> {
            intake.tick(&CancellationToken::new()).await.unwrap()
        }

        /// Kinds of the jobs of a node, oldest first.
        async fn jobs(&self, node: &str) -> Vec<Kind> {
            self.store
                .jobs_of_node(node)
                .await
                .unwrap()
                .iter()
                .map(|job| Action::decode(job.action.as_slice()).unwrap().kind.unwrap())
                .collect()
        }
    }

    fn node() -> String {
        format!("n{}", uuid::Uuid::now_v7().simple())
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
        for bad in ["", ".", "..", "../1", "a/b"] {
            assert!(!is_safe_target(bad), "{bad}");
        }
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
             EXTERNALCMD:{first}:[4] D:with:colons\n\
             EXTERNALCMD:{first}:\n"
        );
        fs::write(&fixture.config.cmd_file, lines).unwrap();

        let stats = fixture.tick(&fixture.intake("a")).await.unwrap();
        assert_eq!(stats, PassStats { files: 1, jobs: 5 });
        // Batch of two, then the rest flushed before the reload, then the
        // tail flushed at the end of the file.
        assert_eq!(
            fixture.jobs(&first).await,
            [
                engine(&["[1] A;host", "[2] B"]),
                engine(&["[3] C"]),
                control("engine", Operation::Reload),
                engine(&["[4] D:with:colons"]),
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
        // The second node only has a broker configuration.
        let broker_only = config.join("broker").join(&partial);
        fs::create_dir_all(&broker_only).unwrap();
        fs::write(broker_only.join("poller-module.json"), "{}").unwrap();

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
        // Directory: a hidden file is ignored, a leftover of a previous run
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
        assert!(!fixture.config.cmd_file.exists());
        assert!(!with_suffix(&fixture.config.cmd_file).exists());
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
    async fn unusable_lines_are_dropped_without_job() {
        let fixture = Fixture::new().await;
        let node = node();
        fs::write(
            &fixture.config.cmd_file,
            format!(
                "SENDEXPORTFILE:{node}:42\nSTARTWORKER:\nNOSUCHCOMMAND:{node}\ngarbage\n\
                 RELOAD:../{node}\nSENDCFGFILE:{node}\nSYNCTRAP:{node}\n\nSTOP:{node}\n"
            ),
        )
        .unwrap();
        let stats = fixture.tick(&fixture.intake("a")).await.unwrap();
        assert_eq!(stats, PassStats { files: 1, jobs: 1 });
        assert_eq!(
            fixture.jobs(&node).await,
            [control("engine", Operation::Stop)]
        );
        assert!(!with_suffix(&fixture.config.cmd_file).exists());
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
