//! `gorgone-server` binary: loads the configuration and runs one replica.

use std::path::PathBuf;
use std::process::ExitCode;

use anyhow::Context;
use gorgone_server::Config;
use tokio::signal::unix::{SignalKind, signal};
use tracing::level_filters::LevelFilter;

const USAGE: &str = "\
Gorgone central control plane.

Usage: gorgone-server [OPTIONS]

Options:
  -c, --config <CONFIG>  TOML configuration file; every key can also be set with
                         `GORGONE_SERVER__…` environment variables
                         [env: GORGONE_SERVER_CONFIG]
      --check            Validate the configuration and exit
  -h, --help             Print help
  -V, --version          Print version
";

/// What the command line asks for.
#[derive(Debug, PartialEq, Eq)]
enum Cli {
    Run {
        config: Option<PathBuf>,
        check: bool,
    },
    Help,
    Version,
}

/// Parses the arguments (without the program name); `config_env` is the
/// fallback of `--config`.
fn parse_args(
    args: impl IntoIterator<Item = String>,
    config_env: Option<PathBuf>,
) -> Result<Cli, String> {
    let mut config = None;
    let mut check = false;
    let mut args = args.into_iter();
    while let Some(arg) = args.next() {
        match arg.as_str() {
            "-h" | "--help" => return Ok(Cli::Help),
            "-V" | "--version" => return Ok(Cli::Version),
            "--check" => check = true,
            "-c" | "--config" => {
                let path = args
                    .next()
                    .ok_or_else(|| format!("a value is required for '{arg}'"))?;
                config = Some(PathBuf::from(path));
            }
            other => match other.strip_prefix("--config=") {
                Some(path) => config = Some(PathBuf::from(path)),
                None => return Err(format!("unexpected argument '{other}'")),
            },
        }
    }
    Ok(Cli::Run {
        config: config.or(config_env),
        check,
    })
}

/// The level comes from `RUST_LOG` (`trace` to `error`, or `off`).
fn init_logging(json: bool) {
    let requested = std::env::var("RUST_LOG").ok();
    let parsed = requested.as_deref().map(str::parse::<LevelFilter>);
    let invalid = matches!(parsed, Some(Err(_)));
    let level = parsed.and_then(Result::ok).unwrap_or(LevelFilter::INFO);
    let builder = tracing_subscriber::fmt().with_max_level(level);
    if json {
        builder.json().init();
    } else {
        builder.init();
    }
    if invalid {
        tracing::warn!(
            RUST_LOG = requested.as_deref(),
            "not a log level (trace, debug, info, warn, error, off): using info"
        );
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

async fn run(config: Config) -> anyhow::Result<()> {
    let handle = gorgone_server::Server::start(config)
        .await
        .context("starting gorgone-server")?;
    termination().await?;
    tracing::info!("termination signal received");
    handle.shutdown().await;
    Ok(())
}

fn main() -> anyhow::Result<ExitCode> {
    let config_env = std::env::var_os("GORGONE_SERVER_CONFIG").map(PathBuf::from);
    let (config, check) = match parse_args(std::env::args().skip(1), config_env) {
        Ok(Cli::Run { config, check }) => (config, check),
        Ok(Cli::Help) => {
            print!("{USAGE}");
            return Ok(ExitCode::SUCCESS);
        }
        Ok(Cli::Version) => {
            println!("gorgone-server {}", env!("CARGO_PKG_VERSION"));
            return Ok(ExitCode::SUCCESS);
        }
        Err(err) => {
            eprintln!("error: {err}\n\n{USAGE}");
            return Ok(ExitCode::from(2));
        }
    };
    let config = Config::load(config.as_deref()).context("loading the configuration")?;
    if check {
        println!("configuration is valid");
        return Ok(ExitCode::SUCCESS);
    }
    init_logging(config.log_json);

    tokio::runtime::Builder::new_multi_thread()
        .enable_all()
        .build()
        .context("starting the runtime")?
        .block_on(run(config))?;
    Ok(ExitCode::SUCCESS)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn parse(args: &[&str]) -> Result<Cli, String> {
        parse_args(args.iter().map(|arg| (*arg).to_owned()), None)
    }

    #[test]
    fn command_line() {
        let run = |config: Option<&str>, check| Cli::Run {
            config: config.map(PathBuf::from),
            check,
        };
        assert_eq!(parse(&[]), Ok(run(None, false)));
        assert_eq!(
            parse(&["--config", "a.toml"]),
            Ok(run(Some("a.toml"), false))
        );
        assert_eq!(
            parse(&["-c", "a.toml", "--check"]),
            Ok(run(Some("a.toml"), true))
        );
        assert_eq!(
            parse(&["--check", "--config=a.toml"]),
            Ok(run(Some("a.toml"), true))
        );
        assert_eq!(parse(&["--help", "--bogus"]), Ok(Cli::Help));
        assert_eq!(parse(&["-V"]), Ok(Cli::Version));
        assert!(parse(&["--config"]).is_err());
        assert!(parse(&["--bogus"]).is_err());
        assert!(parse(&["a.toml"]).is_err());

        // The environment names the file unless the command line does.
        let env = || Some(PathBuf::from("env.toml"));
        assert_eq!(
            parse_args(Vec::new(), env()),
            Ok(run(Some("env.toml"), false))
        );
        assert_eq!(
            parse_args(["-c".to_owned(), "a.toml".to_owned()], env()),
            Ok(run(Some("a.toml"), false))
        );
    }
}
