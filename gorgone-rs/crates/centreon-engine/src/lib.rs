//! centreon-engine: the monitoring core of a Centreon poller, in place of
//! Centreon Engine and its Broker module. It takes the poller configuration
//! (the Engine `.cfg` files centreon-web generates, or the structured JSON
//! document of `gorgone-rs/schemas/engine-configuration.v4.json`), runs the
//! checks and sends the results to the central Broker over BBDO. Run by the
//! `centreon-engine` program of the gorgone-agent crate. Design and sources:
//! `docs/scheduler-design.md`.

mod bbdo;
mod book;
mod broker_conf;
mod cfg;
mod client;
mod cmdline;
mod core;
mod crypto;
mod document;
mod events;
mod extcmd;
mod logs;
mod macros;
mod objects;
mod output;
mod recurring;
mod runner;
mod snapshot;
mod state;
mod timeperiod;
mod tls;
mod tz;

use std::path::{Path, PathBuf};
use std::sync::Arc;
use std::sync::atomic::Ordering;
use std::time::Duration;

use tokio::sync::{Mutex, mpsc, oneshot, watch};
use tokio_util::sync::CancellationToken;

pub use crate::client::Stats;
use crate::client::{Client, Target};
use crate::core::{Core, CoreOptions, Request};
pub use crate::document::VERSION as DOCUMENT_VERSION;

/// Low-level pieces, public for the integration tests and the tools of the
/// workspace; not a stable interface.
pub mod wire {
    pub use crate::bbdo::{Decoder, HEADER_SIZE, crc16, encode, event, frame};
}

/// Where the configuration comes from.
#[derive(Debug, Clone)]
pub enum Input {
    /// The Engine files: main file (`centengine.cfg`) and the `cbmod` file
    /// with the poller identity and the central address
    /// (`broker_module_cfg_file` of the main file when `None`).
    Files {
        /// Main Engine file.
        main_file: PathBuf,
        /// `cbmod` file.
        broker_config: Option<PathBuf>,
    },
    /// Structured documents given by [`Scheduler::apply`]; the one in force
    /// is kept in `dir` and loaded again at start.
    Structured {
        /// Private directory of the engine.
        dir: PathBuf,
    },
}

/// Settings of the engine.
#[derive(Debug, Clone)]
pub struct Options {
    /// Source of the configuration.
    pub input: Input,
    /// `host:port` of the central broker, over what the configuration says.
    pub broker_address: Option<String>,
    /// Plugins running at once, at most.
    pub max_concurrent_checks: usize,
    /// Events kept in memory while the central does not take them.
    pub max_pending_events: usize,
    /// Cap on the output read from a plugin, in bytes.
    pub max_output_bytes: usize,
    /// Retention file: states, downtimes, comments and acknowledgements
    /// survive a restart; nothing is kept when `None`.
    pub retention_file: Option<PathBuf>,
    /// Keys of the `encrypt::` macro values (`engine-context.json`), read
    /// at each configuration apply as Engine does.
    pub credentials_file: Option<PathBuf>,
}

/// Where Engine reads the keys of the `encrypt::` values
/// (`engine/src/configuration/applier/state.cc:69`).
pub const DEFAULT_CREDENTIALS_FILE: &str = "/etc/centreon-engine/engine-context.json";

impl Options {
    /// Defaults, configuration read from the Engine files of `main_file`.
    #[must_use]
    pub fn new(main_file: impl Into<PathBuf>) -> Self {
        Self::with(Input::Files {
            main_file: main_file.into(),
            broker_config: None,
        })
    }

    /// Defaults, structured configuration kept in `dir`.
    #[must_use]
    pub fn structured(dir: impl Into<PathBuf>) -> Self {
        Self::with(Input::Structured { dir: dir.into() })
    }

    fn with(input: Input) -> Self {
        Self {
            input,
            broker_address: None,
            max_concurrent_checks: 256,
            max_pending_events: 100_000,
            max_output_bytes: 8192,
            retention_file: None,
            credentials_file: Some(PathBuf::from(DEFAULT_CREDENTIALS_FILE)),
        }
    }
}

/// What the engine runs, for the node state.
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Status {
    /// A configuration is in force.
    pub loaded: bool,
    /// Poller id of that configuration.
    pub poller_id: u64,
    /// Hosts monitored.
    pub hosts: usize,
    /// Services monitored.
    pub services: usize,
    /// Digest the structured configuration in force was applied under.
    pub digest: Option<String>,
}

/// Version reported as the engine version in the node state, with the
/// versions of the configuration document it reads: gorgone-server refuses
/// a document this engine would refuse before submitting it.
pub const VERSION: &str = concat!(
    "centreon-engine ",
    env!("CARGO_PKG_VERSION"),
    " (configuration 1-4)"
);

#[cfg(test)]
mod tests {
    #[test]
    fn the_version_names_the_documents_read() {
        assert!(
            super::VERSION.ends_with(&format!("(configuration 1-{})", super::DOCUMENT_VERSION))
        );
    }
}

/// Handle on a running engine; clones share it.
#[derive(Clone)]
pub struct Scheduler {
    requests: mpsc::UnboundedSender<Request>,
    status: watch::Receiver<Status>,
    stats: Arc<Stats>,
    cancel: CancellationToken,
    /// Directory of the kept document, with a structured input.
    structured: Option<PathBuf>,
    /// One document applied at a time, in submission order.
    applying: Arc<Mutex<()>>,
}

/// A request the engine task did not answer: it is gone.
const GONE: &str = "the engine is not running";

impl Scheduler {
    /// Starts the engine and its connection to the central broker on the
    /// current runtime. A missing or invalid configuration is not an error:
    /// the engine waits for one.
    #[must_use]
    pub fn start(options: Options) -> Self {
        let cancel = CancellationToken::new();
        let stats = Arc::new(Stats::default());
        let (out_tx, out_rx) = mpsc::channel(options.max_pending_events.max(1024));
        let (target_tx, target_rx) = watch::channel(None::<Target>);
        let (notice_tx, notice_rx) = mpsc::unbounded_channel();
        let (status_tx, status_rx) = watch::channel(Status::default());
        let (request_tx, request_rx) = mpsc::unbounded_channel();
        let client = Client {
            events: out_rx,
            target: target_rx,
            notices: notice_tx,
            max_pending: options.max_pending_events,
            stats: stats.clone(),
            cancel: cancel.clone(),
        };
        tokio::spawn(client.run());
        let structured = match &options.input {
            Input::Structured { dir } => Some(dir.clone()),
            Input::Files { .. } => None,
        };
        let core = Core::new(
            CoreOptions {
                input: options.input,
                broker_address: options.broker_address,
                max_concurrent_checks: options.max_concurrent_checks.max(1),
                max_output_bytes: options.max_output_bytes.max(1),
                retention_file: options.retention_file,
                credentials_file: options.credentials_file,
            },
            out_tx,
            target_tx,
            status_tx,
        );
        tokio::spawn(core.run(request_rx, notice_rx, cancel.clone()));
        let scheduler = Self {
            requests: request_tx,
            status: status_rx,
            stats,
            cancel,
            structured,
            applying: Arc::new(Mutex::new(())),
        };
        if scheduler.structured.is_some() {
            let starting = scheduler.clone();
            // Under the apply lock: a document applied meanwhile wins.
            let guard = scheduler.applying.clone().try_lock_owned();
            tokio::spawn(async move {
                let _guard = guard;
                match starting.load_kept().await {
                    Ok(summary) => tracing::info!("{}", first_line(&summary)),
                    Err(error) => tracing::warn!(
                        "the engine waits for a configuration: {}",
                        first_error(&error)
                    ),
                }
            });
        }
        scheduler
    }

    /// Reads the configuration again and puts it in force: the files, or
    /// the kept document. On error the one in force stays and the error is
    /// the report.
    pub async fn reload(&self) -> Result<String, String> {
        if self.structured.is_some() {
            let _guard = self.applying.lock().await;
            return self.load_kept().await;
        }
        let (reply, answer) = oneshot::channel();
        self.requests
            .send(Request::Reload(reply))
            .map_err(|_| GONE.to_owned())?;
        answer.await.map_err(|_| GONE.to_owned())?
    }

    /// Puts the kept document in force again.
    async fn load_kept(&self) -> Result<String, String> {
        let dir = self.structured.clone().ok_or("no structured input")?;
        let kept = tokio::task::spawn_blocking(move || {
            let kept = snapshot::read(&dir)
                .map_err(|error| format!("{}: {error}", dir.display()))?
                .ok_or("no configuration applied yet")?;
            let (parsed, report) = document::parse(&kept.document)?;
            Ok::<_, String>((parsed, report, kept.digest))
        })
        .await
        .map_err(|error| format!("configuration task failed: {error}"))??;
        let (parsed, report, digest) = kept;
        self.install(parsed, &report, digest).await
    }

    /// Checks a structured document (`gorgone-rs/schemas/`), keeps it and
    /// puts it in force, sending the central only what changed. An invalid
    /// document is refused as a whole, with the report (`Total Errors:`
    /// last), and the configuration in force stays. `digest` names it in
    /// the node state.
    pub async fn apply(&self, document: Vec<u8>, digest: String) -> Result<String, String> {
        let Some(dir) = self.structured.clone() else {
            return Err(
                "this engine reads its configuration from files (scheduler.input = \"files\")"
                    .to_owned(),
            );
        };
        let _guard = self.applying.lock().await;
        let kept_digest = digest.clone();
        let checked = tokio::task::spawn_blocking(move || {
            let (parsed, report) = document::parse(&document)?;
            // Kept before it is in force: a restart never runs an older one.
            snapshot::write(&dir, &kept_digest, &document).map_err(|error| {
                format!("{}: cannot keep the configuration: {error}", dir.display())
            })?;
            Ok::<_, String>((parsed, report))
        })
        .await
        .map_err(|error| format!("configuration task failed: {error}"))??;
        let (parsed, report) = checked;
        self.install(parsed, &report, digest).await
    }

    async fn install(
        &self,
        parsed: document::Parsed,
        report: &cfg::Report,
        digest: String,
    ) -> Result<String, String> {
        let (reply, answer) = oneshot::channel();
        self.requests
            .send(Request::Install(Box::new((parsed, digest)), reply))
            .map_err(|_| GONE.to_owned())?;
        let summary = answer.await.map_err(|_| GONE.to_owned())?;
        let counts = (summary.counts.0, summary.counts.1, summary.counts.2);
        Ok(format!("{}\n{}", summary.text, report.render(Some(counts))))
    }

    /// Runs one external command line (`[time] NAME;args`).
    pub async fn external_command(&self, line: &str) -> Result<String, String> {
        let (reply, answer) = oneshot::channel();
        self.requests
            .send(Request::Command(line.to_owned(), reply))
            .map_err(|_| GONE.to_owned())?;
        answer.await.map_err(|_| GONE.to_owned())?
    }

    /// What the engine runs now.
    #[must_use]
    pub fn status(&self) -> Status {
        self.status.borrow().clone()
    }

    /// Waits until the status changes.
    pub async fn status_changed(&mut self) -> Option<Status> {
        self.status.changed().await.ok()?;
        Some(self.status.borrow_and_update().clone())
    }

    /// Connection counters.
    #[must_use]
    pub fn stats(&self) -> &Stats {
        &self.stats
    }

    /// Connected to the central broker.
    #[must_use]
    pub fn connected(&self) -> bool {
        self.stats.connected.load(Ordering::Relaxed)
    }

    /// Tells the central the poller stops, waits for its acknowledgement
    /// (bounded), then stops every task.
    pub async fn shutdown(&self) {
        let (reply, answer) = oneshot::channel();
        if self.requests.send(Request::Shutdown(reply)).is_ok() {
            let _ = tokio::time::timeout(Duration::from_secs(15), answer).await;
        }
        self.cancel.cancel();
    }
}

fn first_line(text: &str) -> &str {
    text.lines().next().unwrap_or_default()
}

/// First `Error:` line of a report, else its first line.
fn first_error(report: &str) -> &str {
    report
        .lines()
        .find(|line| line.starts_with("Error"))
        .unwrap_or_else(|| first_line(report))
}

/// Digest of the structured document kept in `dir`, without loading it:
/// what the node reports before the engine has started.
#[must_use]
pub fn kept_digest(dir: &Path) -> Option<String> {
    snapshot::read(dir).ok().flatten().map(|kept| kept.digest)
}

/// Checks a structured document without applying it; `true` when valid,
/// with the report (`Total Errors:` last).
#[must_use]
pub fn validate_document(document: &[u8]) -> (bool, String) {
    match document::parse(document) {
        Ok((parsed, report)) => {
            let config = &parsed.config;
            let counts = (
                config.commands.len(),
                config.hosts.len(),
                config.services.len(),
            );
            (true, report.render(Some(counts)))
        }
        Err(report) => (false, report),
    }
}

/// Checks the configuration whose main file is `main_file`, as
/// `centengine -v` does: the report ends with `Total Warnings:` and
/// `Total Errors:`; `true` when it has no error. A `cbmod` file that cannot
/// be read is a warning: it is pushed as another target, maybe later.
#[must_use]
pub fn validate(main_file: &Path, broker_config: Option<&Path>) -> (bool, String) {
    let (config, mut report) = cfg::read(main_file);
    let broker_file = broker_config
        .map(Path::to_path_buf)
        .or_else(|| config.settings.broker_module_cfg_file.clone());
    match broker_file {
        Some(path) => {
            if let Err(error) = broker_conf::read(&path) {
                report.warnings.push(error);
            }
        }
        None => report.warnings.push(
            "no broker_module_cfg_file: the poller cannot reach the central broker".to_owned(),
        ),
    }
    let counts = (
        config.commands.len(),
        config.hosts.len(),
        config.services.len(),
    );
    (report.errors.is_empty(), report.render(Some(counts)))
}
