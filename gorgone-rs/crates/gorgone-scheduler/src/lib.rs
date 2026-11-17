//! Monitoring scheduler of a Centreon poller, run inside gorgone-agent in
//! place of Centreon Engine and its Broker module: it reads the Engine
//! configuration centreon-web generates (commands, hosts, services and their
//! templates), runs the checks and sends the results to the central Broker
//! over BBDO. Design and sources: `docs/scheduler-design.md`.

mod bbdo;
mod broker_conf;
mod cfg;
mod client;
mod cmdline;
mod core;
mod events;
mod extcmd;
mod macros;
mod output;
mod runner;
mod state;

use std::path::{Path, PathBuf};
use std::sync::Arc;
use std::sync::atomic::Ordering;
use std::time::Duration;

use tokio::sync::{mpsc, oneshot, watch};
use tokio_util::sync::CancellationToken;

pub use crate::client::Stats;
use crate::client::{Client, Target};
use crate::core::{Core, CoreOptions, Request};

/// Low-level pieces, public for the integration tests and the tools of the
/// workspace; not a stable interface.
pub mod wire {
    pub use crate::bbdo::{Decoder, HEADER_SIZE, crc16, encode, event, frame};
}

/// Settings of the scheduler (the `[scheduler]` section of the agent).
#[derive(Debug, Clone)]
pub struct Options {
    /// Main Engine file (`centengine.cfg`) of the configuration to run.
    pub main_file: PathBuf,
    /// `cbmod` file to read the poller identity and the central address
    /// from; `broker_module_cfg_file` of the main file when unset.
    pub broker_config: Option<PathBuf>,
    /// `host:port` of the central broker; the TCP output of the `cbmod`
    /// file when unset.
    pub broker_address: Option<String>,
    /// Plugins running at once, at most.
    pub max_concurrent_checks: usize,
    /// Events kept in memory while the central does not take them.
    pub max_pending_events: usize,
    /// Cap on the output read from a plugin, in bytes.
    pub max_output_bytes: usize,
}

impl Options {
    /// Defaults for `main_file`.
    #[must_use]
    pub fn new(main_file: impl Into<PathBuf>) -> Self {
        Self {
            main_file: main_file.into(),
            broker_config: None,
            broker_address: None,
            max_concurrent_checks: 256,
            max_pending_events: 100_000,
            max_output_bytes: 8192,
        }
    }
}

/// What the scheduler runs, for the node state.
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
}

/// Version reported as the engine version.
pub const VERSION: &str = concat!("gorgone-scheduler ", env!("CARGO_PKG_VERSION"));

/// Handle on a running scheduler; clones share it.
#[derive(Clone)]
pub struct Scheduler {
    requests: mpsc::UnboundedSender<Request>,
    status: watch::Receiver<Status>,
    stats: Arc<Stats>,
    cancel: CancellationToken,
}

/// A request the scheduler task did not answer: it is gone.
const GONE: &str = "the scheduler is not running";

impl Scheduler {
    /// Starts the scheduler and its connection to the central broker on the
    /// current runtime. A missing or invalid configuration is not an error:
    /// the scheduler waits for a reload.
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
        let core = Core::new(
            CoreOptions {
                main_file: options.main_file,
                broker_config: options.broker_config,
                broker_address: options.broker_address,
                max_concurrent_checks: options.max_concurrent_checks.max(1),
                max_output_bytes: options.max_output_bytes.max(1),
            },
            out_tx,
            target_tx,
            status_tx,
        );
        tokio::spawn(core.run(request_rx, notice_rx, cancel.clone()));
        Self {
            requests: request_tx,
            status: status_rx,
            stats,
            cancel,
        }
    }

    /// Reads the configuration again and puts it in force; on error the one
    /// in force stays and the error is the report.
    pub async fn reload(&self) -> Result<String, String> {
        let (reply, answer) = oneshot::channel();
        self.requests
            .send(Request::Reload(reply))
            .map_err(|_| GONE.to_owned())?;
        answer.await.map_err(|_| GONE.to_owned())?
    }

    /// Runs one external command line (`[time] NAME;args`).
    pub async fn external_command(&self, line: &str) -> Result<String, String> {
        let (reply, answer) = oneshot::channel();
        self.requests
            .send(Request::Command(line.to_owned(), reply))
            .map_err(|_| GONE.to_owned())?;
        answer.await.map_err(|_| GONE.to_owned())?
    }

    /// What the scheduler runs now.
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
