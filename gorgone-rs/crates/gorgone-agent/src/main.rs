//! `gorgone-agent` binary: configuration, logging and signals around the library.

use std::ffi::OsString;
use std::path::PathBuf;

use anyhow::Context;
use gorgone_agent::AgentConfig;
use tokio::signal::unix::{SignalKind, signal};
use tracing::level_filters::LevelFilter;

const DEFAULT_CONFIG: &str = "/etc/centreon-gorgone/agent.toml";
const CONFIG_ENV: &str = "GORGONE_AGENT_CONFIG";
const LOG_ENV: &str = "GORGONE_AGENT_LOG";

const HELP: &str = "\
Gorgone agent: runs on a poller the jobs sent by gorgone-server

Usage: gorgone-agent [OPTIONS]

Options:
  -c, --config <FILE>  Configuration file; skipped when absent, the environment
                       (GORGONE_AGENT__...) is always read
                       [env: GORGONE_AGENT_CONFIG] [default: /etc/centreon-gorgone/agent.toml]
      --log <LEVEL>    Log level: trace, debug, info, warn, error or off
                       [env: GORGONE_AGENT_LOG] [default: info]
      --check          Validate the configuration and exit
  -h, --help           Print help
  -V, --version        Print version";

/// What the command line asks for.
#[derive(Debug, PartialEq, Eq)]
enum Cli {
    Run(Args),
    Help,
    Version,
}

#[derive(Debug, PartialEq, Eq)]
struct Args {
    config: PathBuf,
    log: LevelFilter,
    check: bool,
}

/// Parses the arguments that follow the program name; `env` gives the
/// defaults of `--config` and `--log`.
fn parse_cli(
    args: impl IntoIterator<Item = OsString>,
    env: impl Fn(&str) -> Option<OsString>,
) -> Result<Cli, String> {
    let mut config = env(CONFIG_ENV);
    let mut log = env(LOG_ENV);
    let mut check = false;
    let mut args = args.into_iter();
    while let Some(arg) = args.next() {
        let text = arg.to_string_lossy();
        // `--name=value` and `--name value` are both accepted.
        let (name, inline) = match text.split_once('=') {
            Some((name, value)) if name.starts_with("--") => (name, Some(value)),
            _ => (text.as_ref(), None),
        };
        let mut value = || {
            inline
                .map(OsString::from)
                .or_else(|| args.next())
                .ok_or_else(|| format!("{name} needs a value"))
        };
        match name {
            "-c" | "--config" => config = Some(value()?),
            "--log" => log = Some(value()?),
            "--check" if inline.is_none() => check = true,
            "-h" | "--help" if inline.is_none() => return Ok(Cli::Help),
            "-V" | "--version" if inline.is_none() => return Ok(Cli::Version),
            _ => return Err(format!("unexpected argument {text:?}")),
        }
    }
    let log = match log {
        Some(level) => level
            .to_string_lossy()
            .parse()
            .map_err(|_| format!("invalid log level {level:?}"))?,
        None => LevelFilter::INFO,
    };
    Ok(Cli::Run(Args {
        config: config.map_or_else(|| PathBuf::from(DEFAULT_CONFIG), PathBuf::from),
        log,
        check,
    }))
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
    let args = match parse_cli(std::env::args_os().skip(1), |name| std::env::var_os(name)) {
        Ok(Cli::Run(args)) => args,
        Ok(Cli::Help) => {
            println!("{HELP}");
            return Ok(());
        }
        Ok(Cli::Version) => {
            println!("gorgone-agent {}", env!("CARGO_PKG_VERSION"));
            return Ok(());
        }
        Err(message) => {
            eprintln!("error: {message}\n\nFor more information, try '--help'.");
            std::process::exit(2);
        }
    };
    tracing_subscriber::fmt()
        .with_max_level(args.log)
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

#[cfg(test)]
mod tests {
    use super::*;

    fn parse(args: &[&str], env: &[(&str, &str)]) -> Result<Cli, String> {
        parse_cli(args.iter().map(OsString::from), |name| {
            let found = env.iter().find(|(key, _)| *key == name);
            found.map(|(_, value)| OsString::from(value))
        })
    }

    fn run(args: &[&str], env: &[(&str, &str)]) -> Args {
        match parse(args, env) {
            Ok(Cli::Run(args)) => args,
            other => panic!("{other:?}"),
        }
    }

    #[test]
    fn defaults_then_environment_then_arguments() {
        let args = run(&[], &[]);
        assert_eq!(args.config, PathBuf::from(DEFAULT_CONFIG));
        assert_eq!(args.log, LevelFilter::INFO);
        assert!(!args.check);

        let env = [(CONFIG_ENV, "/env.toml"), (LOG_ENV, "warn")];
        let args = run(&[], &env);
        assert_eq!(args.config, PathBuf::from("/env.toml"));
        assert_eq!(args.log, LevelFilter::WARN);

        let args = run(&["-c", "/a.toml", "--log=DEBUG", "--check"], &env);
        assert_eq!(args.config, PathBuf::from("/a.toml"));
        assert_eq!(args.log, LevelFilter::DEBUG);
        assert!(args.check);

        let args = run(&["--config=/b=c.toml", "--log", "off"], &[]);
        assert_eq!(args.config, PathBuf::from("/b=c.toml"));
        assert_eq!(args.log, LevelFilter::OFF);
    }

    #[test]
    fn help_and_version() {
        assert_eq!(parse(&["--check", "-h"], &[]), Ok(Cli::Help));
        assert_eq!(parse(&["--help"], &[]), Ok(Cli::Help));
        assert_eq!(parse(&["-V"], &[]), Ok(Cli::Version));
        assert_eq!(parse(&["--version"], &[]), Ok(Cli::Version));
    }

    #[test]
    fn rejects_what_it_does_not_know() {
        for (args, needle) in [
            (&["--config"][..], "needs a value"),
            (&["--log"][..], "needs a value"),
            (&["--log", "loud"][..], "invalid log level"),
            (&["--check=yes"][..], "unexpected"),
            (&["--verbose"][..], "unexpected"),
            (&["agent.toml"][..], "unexpected"),
        ] {
            let message = parse(args, &[]).unwrap_err();
            assert!(message.contains(needle), "{args:?}: {message}");
        }
        let message = parse(&[], &[(LOG_ENV, "loud")]).unwrap_err();
        assert!(message.contains("invalid log level"), "{message}");
    }
}
