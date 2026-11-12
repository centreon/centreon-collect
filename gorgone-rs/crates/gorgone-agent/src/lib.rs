//! Gorgone agent: dials `gorgone-server`, keeps one control stream and runs
//! the typed jobs it receives, within what the local configuration allows.

mod client;
pub mod config;
mod executor;
mod journal;
mod process;
mod scheduler;
mod session;
mod store;

use std::future::Future;
use std::path::PathBuf;
use std::sync::Arc;
use std::time::Duration;

use gorgone_core::allowlist::{Allowlist, AllowlistError};
use tokio_util::sync::CancellationToken;

pub use crate::config::{AgentConfig, ConfigError};
use crate::executor::{
    ApplyConfigExecutor, EngineCommandsExecutor, RunCommandsExecutor, ServiceControlExecutor,
};
use crate::journal::Journal;
use crate::scheduler::{Executors, Scheduler};
use crate::session::{Backoff, Session};

/// Time left to cancelled jobs to stop, then to the last events to be
/// acknowledged, when the agent stops.
const SHUTDOWN_FLUSH: Duration = Duration::from_secs(5);

/// Errors stopping the agent before or while it runs.
#[derive(Debug, thiserror::Error)]
pub enum AgentError {
    /// Invalid configuration.
    #[error(transparent)]
    Config(#[from] ConfigError),
    /// Invalid `commands.allowlist` pattern.
    #[error(transparent)]
    Allowlist(#[from] AllowlistError),
    /// Invalid server URL or TLS material.
    #[error("invalid server connection settings: {0}")]
    Transport(#[from] tonic::transport::Error),
    /// A value cannot be sent as gRPC metadata.
    #[error("{0} contains characters that cannot be sent as metadata")]
    Metadata(String),
    /// A TLS file could not be read, or the state directory is unusable.
    #[error("{path}: {source}")]
    Io {
        /// File or directory involved.
        path: PathBuf,
        /// Underlying error.
        source: std::io::Error,
    },
    /// Another agent already runs with the same `state_dir`.
    #[error("{0} is locked by another gorgone-agent: one state_dir per agent")]
    StateDirLocked(PathBuf),
    /// The session task stopped on its own, which is a bug.
    #[error("session task stopped unexpectedly: {0}")]
    SessionTask(#[from] tokio::task::JoinError),
}

/// Runs the agent until `shutdown` resolves, then lets the running jobs
/// finish for `shutdown_grace_s` and returns.
///
/// Connection failures are never fatal: the agent reconnects forever. Only
/// invalid settings make this return an error, before anything is started.
pub async fn run(
    config: AgentConfig,
    shutdown: impl Future<Output = ()>,
) -> Result<(), AgentError> {
    let client = client::connect(&config)?;
    let executors = Executors {
        run_commands: RunCommandsExecutor {
            allowlist: Allowlist::new(&config.commands.allowlist)?,
            shell: config.commands.shell,
            default_timeout: config.default_command_timeout(),
            kill_grace: config.kill_grace(),
            max_output: config.max_output_bytes,
        },
        engine_commands: EngineCommandsExecutor {
            command_file: config.engine.command_file.clone(),
            allowed: Allowlist::new(&config.engine.allowed_commands)?,
        },
        apply_config: ApplyConfigExecutor {
            targets: config.targets.clone(),
            max_bundle_bytes: config.max_bundle_bytes,
            max_unpacked_bytes: config.max_unpacked_bytes,
            fetcher: client::GrpcBlobFetcher {
                client: client.clone(),
            },
        },
        service_control: ServiceControlExecutor {
            services: config.services.clone(),
            timeout: config.default_command_timeout(),
            kill_grace: config.kill_grace(),
            max_output: config.max_output_bytes,
        },
    };
    // Held until this function returns: two agents must not share a journal.
    let _state_lock = match &config.state_dir {
        Some(dir) => {
            let lock = store::lock_state_dir(dir).map_err(|source| AgentError::Io {
                path: dir.clone(),
                source,
            })?;
            Some(lock.ok_or_else(|| AgentError::StateDirLocked(dir.clone()))?)
        }
        None => None,
    };
    let journal_dir = config.state_dir.as_ref().map(|dir| dir.join("journal"));
    let journal = Journal::open(
        config.job_history,
        config.max_buffered_event_bytes,
        journal_dir.as_deref(),
    )
    .map_err(|source| AgentError::Io {
        path: journal_dir.unwrap_or_default(),
        source,
    })?;
    let journal = Arc::new(journal);
    let scheduler = Arc::new(Scheduler::new(
        journal.clone(),
        executors,
        config.max_concurrent_jobs,
    ));
    let session = Session {
        client,
        node_id: config.node_id.clone(),
        journal: journal.clone(),
        scheduler: scheduler.clone(),
        backoff: Backoff::new(
            Duration::from_millis(config.reconnect.min_ms),
            Duration::from_millis(config.reconnect.max_ms),
        ),
    };

    tracing::info!(node_id = %config.node_id, server_url = %config.server_url, "agent started");
    let stop = CancellationToken::new();
    let mut session = tokio::spawn(session.run(stop.clone()));
    tokio::pin!(shutdown);
    tokio::select! {
        () = &mut shutdown => {}
        ended = &mut session => {
            ended?;
            return Ok(());
        }
    }

    tracing::info!("stopping: no new job is taken");
    scheduler.drain();
    let grace = Duration::from_secs(config.shutdown_grace_s);
    if tokio::time::timeout(grace, journal.idle()).await.is_err() {
        tracing::warn!("jobs still running after {grace:?}: cancelling them");
        journal.cancel_all();
        let _ = tokio::time::timeout(SHUTDOWN_FLUSH, journal.idle()).await;
    }
    // The session stays up a little longer so that the last events get through.
    if tokio::time::timeout(SHUTDOWN_FLUSH, journal.settled())
        .await
        .is_err()
    {
        tracing::warn!("stopping with events the server did not acknowledge");
    }
    stop.cancel();
    session.await?;
    tracing::info!("agent stopped");
    Ok(())
}
