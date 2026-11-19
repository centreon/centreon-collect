//! Command line of the programs built on the agent: `gorgone-agent`, and
//! `centreon-engine` (the agent with its built-in engine always on).
//! Configuration, logging and signals around [`crate::run`].

use std::ffi::OsString;
use std::path::{Path, PathBuf};

use anyhow::Context;
use tokio::signal::unix::{SignalKind, signal};
use tracing::level_filters::LevelFilter;

use crate::{AgentConfig, ConfigError};

/// What differs between the programs.
#[derive(Debug)]
pub struct Program {
    /// Name printed by `--version` and in messages.
    pub name: &'static str,
    /// Help text.
    pub help: &'static str,
    /// Configuration file when neither `--config` nor `config_env` is set.
    pub default_config: &'static str,
    /// Variable giving the configuration file.
    pub config_env: &'static str,
    /// Variable giving the log level.
    pub log_env: &'static str,
    /// Reads the configuration file and the environment.
    pub load: fn(Option<&Path>) -> Result<AgentConfig, ConfigError>,
}

/// `gorgone-agent`: runs on a node the jobs gorgone-server sends.
pub const GORGONE_AGENT: Program = Program {
    name: "gorgone-agent",
    help: "\
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
  -V, --version        Print version",
    default_config: "/etc/centreon-gorgone/agent.toml",
    config_env: "GORGONE_AGENT_CONFIG",
    log_env: "GORGONE_AGENT_LOG",
    load: AgentConfig::load,
};

/// `centreon-engine`: the whole poller, the agent and its built-in engine
/// in place of centengine and cbd.
pub const CENTREON_ENGINE: Program = Program {
    name: "centreon-engine",
    help: "\
centreon-engine: the monitoring engine of a Centreon poller. Takes its
configuration from gorgone-server, runs the checks and sends the results to
the central broker; also runs the jobs of gorgone-server on the poller.

Usage: centreon-engine [OPTIONS]

Options:
  -c, --config <FILE>  Configuration file (format of gorgone-agent, the
                       [scheduler] always enabled, input \"structured\" by
                       default); skipped when absent, the environment
                       (GORGONE_AGENT__..., then CENTREON_ENGINE__... which
                       wins) is always read
                       [env: CENTREON_ENGINE_CONFIG] [default: /etc/centreon-engine/centreon-engine.toml]
      --log <LEVEL>    Log level: trace, debug, info, warn, error or off
                       [env: CENTREON_ENGINE_LOG] [default: info]
      --check          Validate the configuration and exit
  -h, --help           Print help
  -V, --version        Print version",
    default_config: "/etc/centreon-engine/centreon-engine.toml",
    config_env: "CENTREON_ENGINE_CONFIG",
    log_env: "CENTREON_ENGINE_LOG",
    load: AgentConfig::load_engine,
};

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
    program: &Program,
    args: impl IntoIterator<Item = OsString>,
    env: impl Fn(&str) -> Option<OsString>,
) -> Result<Cli, String> {
    let mut config = env(program.config_env);
    let mut log = env(program.log_env);
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
        config: config.map_or_else(|| PathBuf::from(program.default_config), PathBuf::from),
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

/// Runs `program` with the arguments and environment of the process.
///
/// # Errors
/// An invalid configuration, or the agent failing to start.
pub fn main(program: &Program) -> anyhow::Result<()> {
    tokio::runtime::Builder::new_multi_thread()
        .enable_all()
        .build()
        .context("starting the runtime")?
        .block_on(run(program))
}

async fn run(program: &Program) -> anyhow::Result<()> {
    let parsed = parse_cli(program, std::env::args_os().skip(1), |name| {
        std::env::var_os(name)
    });
    let args = match parsed {
        Ok(Cli::Run(args)) => args,
        Ok(Cli::Help) => {
            println!("{}", program.help);
            return Ok(());
        }
        Ok(Cli::Version) => {
            println!("{} {}", program.name, env!("CARGO_PKG_VERSION"));
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

    let config = (program.load)(Some(&args.config))
        .with_context(|| format!("loading {}", args.config.display()))?;
    if args.check {
        println!("configuration is valid");
        return Ok(());
    }

    let terminate = signal(SignalKind::terminate()).context("installing the SIGTERM handler")?;
    let interrupt = signal(SignalKind::interrupt()).context("installing the SIGINT handler")?;
    crate::run(config, shutdown_signal(terminate, interrupt))
        .await
        .context("agent failed")
}

#[cfg(test)]
mod tests {
    use super::*;

    const CONFIG_ENV: &str = GORGONE_AGENT.config_env;
    const LOG_ENV: &str = GORGONE_AGENT.log_env;
    const DEFAULT_CONFIG: &str = GORGONE_AGENT.default_config;

    fn parse(args: &[&str], env: &[(&str, &str)]) -> Result<Cli, String> {
        parse_with(&GORGONE_AGENT, args, env)
    }

    fn parse_with(program: &Program, args: &[&str], env: &[(&str, &str)]) -> Result<Cli, String> {
        parse_cli(program, args.iter().map(OsString::from), |name| {
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
    fn centreon_engine_has_its_own_defaults() {
        let Ok(Cli::Run(args)) = parse_with(&CENTREON_ENGINE, &[], &[]) else {
            panic!("no run");
        };
        assert_eq!(
            args.config,
            PathBuf::from("/etc/centreon-engine/centreon-engine.toml")
        );
        let env = [
            ("CENTREON_ENGINE_CONFIG", "/e.toml"),
            (CONFIG_ENV, "/g.toml"),
        ];
        let Ok(Cli::Run(args)) = parse_with(&CENTREON_ENGINE, &[], &env) else {
            panic!("no run");
        };
        assert_eq!(args.config, PathBuf::from("/e.toml"));
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
