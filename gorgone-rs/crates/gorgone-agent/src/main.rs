//! `gorgone-agent` binary: configuration, logging and signals around the library.

use std::path::PathBuf;

use anyhow::Context;
use clap::Parser;
use gorgone_agent::AgentConfig;
use tokio::signal::unix::{SignalKind, signal};

/// Gorgone agent: runs on a poller the jobs sent by gorgone-server.
#[derive(Debug, Parser)]
#[command(version, about)]
struct Args {
    /// Configuration file; skipped when absent, the environment
    /// (`GORGONE_AGENT__…`) is always read.
    #[arg(
        short,
        long,
        env = "GORGONE_AGENT_CONFIG",
        default_value = "/etc/centreon-gorgone/agent.toml"
    )]
    config: PathBuf,
    /// Log filter, as a level (`info`) or tracing directives (`gorgone_agent=debug`).
    #[arg(long, env = "GORGONE_AGENT_LOG", default_value = "info")]
    log: String,
    /// Validate the configuration and exit.
    #[arg(long)]
    check: bool,
}

async fn shutdown_signal(
    mut terminate: tokio::signal::unix::Signal,
    mut interrupt: tokio::signal::unix::Signal,
) {
    tokio::select! {
        _ = terminate.recv() => {}
        _ = interrupt.recv() => {}
    }
}

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    let args = Args::parse();
    let filter = tracing_subscriber::EnvFilter::try_new(&args.log)
        .with_context(|| format!("invalid log filter {:?}", args.log))?;
    tracing_subscriber::fmt()
        .with_env_filter(filter)
        .with_writer(std::io::stderr)
        .init();

    let config = AgentConfig::load(Some(&args.config))
        .with_context(|| format!("loading {}", args.config.display()))?;
    if args.check {
        println!("configuration is valid");
        return Ok(());
    }

    let terminate = signal(SignalKind::terminate()).context("installing the SIGTERM handler")?;
    let interrupt = signal(SignalKind::interrupt()).context("installing the SIGINT handler")?;
    gorgone_agent::run(config, shutdown_signal(terminate, interrupt))
        .await
        .context("agent failed")
}
