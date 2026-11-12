//! `gorgone-server` binary: loads the configuration and runs one replica.

use std::path::PathBuf;

use anyhow::Context;
use clap::Parser;
use gorgone_server::Config;
use tokio::signal::unix::{SignalKind, signal};
use tracing_subscriber::EnvFilter;

/// Gorgone central control plane.
#[derive(Debug, Parser)]
#[command(version, about)]
struct Cli {
    /// TOML configuration file; every key can also be set with
    /// `GORGONE_SERVER__…` environment variables.
    #[arg(short, long, env = "GORGONE_SERVER_CONFIG")]
    config: Option<PathBuf>,
    /// Validate the configuration and exit.
    #[arg(long)]
    check: bool,
}

fn init_logging(json: bool) {
    let filter = EnvFilter::try_from_default_env().unwrap_or_else(|_| EnvFilter::new("info"));
    let builder = tracing_subscriber::fmt().with_env_filter(filter);
    if json {
        builder.json().init();
    } else {
        builder.init();
    }
}

async fn termination() -> anyhow::Result<()> {
    let mut term = signal(SignalKind::terminate()).context("installing the SIGTERM handler")?;
    let mut int = signal(SignalKind::interrupt()).context("installing the SIGINT handler")?;
    tokio::select! {
        _ = term.recv() => {}
        _ = int.recv() => {}
    }
    Ok(())
}

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    let cli = Cli::parse();
    let config = Config::load(cli.config.as_deref()).context("loading the configuration")?;
    if cli.check {
        println!("configuration is valid");
        return Ok(());
    }
    init_logging(config.log_json);

    let handle = gorgone_server::Server::start(config)
        .await
        .context("starting gorgone-server")?;
    termination().await?;
    tracing::info!("termination signal received");
    handle.shutdown().await;
    Ok(())
}
