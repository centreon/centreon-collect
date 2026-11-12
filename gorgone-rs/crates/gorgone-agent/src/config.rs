//! Agent configuration: one TOML file, every key overridable by environment
//! (`GORGONE_AGENT__SECTION__KEY`).

use std::collections::BTreeMap;
use std::fmt;
use std::path::{Path, PathBuf};
use std::time::Duration;

use figment::Figment;
use figment::providers::{Env, Format, Toml};
use gorgone_proto::agent::service_control::Operation;
use serde::Deserialize;

/// Prefix of the environment overrides; `__` separates nesting levels.
pub const ENV_PREFIX: &str = "GORGONE_AGENT__";

/// Configuration errors.
#[derive(Debug, thiserror::Error)]
pub enum ConfigError {
    /// File or environment could not be parsed.
    #[error("invalid configuration: {0}")]
    Parse(#[from] Box<figment::Error>),
    /// A value is missing or inconsistent.
    #[error("invalid configuration: {0}")]
    Invalid(String),
    /// A referenced file (token, certificate) could not be read.
    #[error("{path}: {source}")]
    Io {
        /// File being read.
        path: PathBuf,
        /// Underlying error.
        source: std::io::Error,
    },
}

/// Text that may arrive typed: the environment loader turns `7` into an
/// integer and `true` into a boolean before the target type is known.
struct Lenient(String);

impl<'de> Deserialize<'de> for Lenient {
    fn deserialize<D: serde::Deserializer<'de>>(deserializer: D) -> Result<Self, D::Error> {
        struct Visitor;

        impl serde::de::Visitor<'_> for Visitor {
            type Value = Lenient;

            fn expecting(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
                f.write_str("a string, a number or a boolean")
            }

            fn visit_str<E: serde::de::Error>(self, value: &str) -> Result<Lenient, E> {
                Ok(Lenient(value.to_owned()))
            }

            fn visit_bool<E: serde::de::Error>(self, value: bool) -> Result<Lenient, E> {
                Ok(Lenient(value.to_string()))
            }

            fn visit_i64<E: serde::de::Error>(self, value: i64) -> Result<Lenient, E> {
                Ok(Lenient(value.to_string()))
            }

            fn visit_u64<E: serde::de::Error>(self, value: u64) -> Result<Lenient, E> {
                Ok(Lenient(value.to_string()))
            }

            fn visit_i128<E: serde::de::Error>(self, value: i128) -> Result<Lenient, E> {
                Ok(Lenient(value.to_string()))
            }

            fn visit_u128<E: serde::de::Error>(self, value: u128) -> Result<Lenient, E> {
                Ok(Lenient(value.to_string()))
            }

            fn visit_f64<E: serde::de::Error>(self, value: f64) -> Result<Lenient, E> {
                Ok(Lenient(value.to_string()))
            }
        }

        deserializer.deserialize_any(Visitor)
    }
}

/// `deserialize_with` helpers applying [`Lenient`] to the text settings.
mod lenient {
    use std::path::PathBuf;

    use serde::{Deserialize, Deserializer};

    use super::Lenient;

    pub(super) fn string<'de, D: Deserializer<'de>>(d: D) -> Result<String, D::Error> {
        Lenient::deserialize(d).map(|text| text.0)
    }

    pub(super) fn option<'de, D: Deserializer<'de>>(d: D) -> Result<Option<String>, D::Error> {
        Ok(Option::<Lenient>::deserialize(d)?.map(|text| text.0))
    }

    pub(super) fn list<'de, D: Deserializer<'de>>(d: D) -> Result<Vec<String>, D::Error> {
        let items = Vec::<Lenient>::deserialize(d)?;
        Ok(items.into_iter().map(|text| text.0).collect())
    }

    pub(super) fn path<'de, D: Deserializer<'de>>(d: D) -> Result<PathBuf, D::Error> {
        string(d).map(PathBuf::from)
    }

    pub(super) fn option_path<'de, D: Deserializer<'de>>(
        d: D,
    ) -> Result<Option<PathBuf>, D::Error> {
        Ok(option(d)?.map(PathBuf::from))
    }
}

/// String that never appears in logs or debug output.
#[derive(Clone, Deserialize)]
#[serde(transparent)]
pub struct Secret(#[serde(deserialize_with = "lenient::string")] String);

impl Secret {
    /// Wraps a secret value.
    #[must_use]
    pub fn new(value: impl Into<String>) -> Self {
        Self(value.into())
    }

    /// The secret itself.
    #[must_use]
    pub fn expose(&self) -> &str {
        &self.0
    }
}

impl fmt::Debug for Secret {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str("<redacted>")
    }
}

/// Agent configuration.
#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct AgentConfig {
    /// Identifier of this poller on the central (`nagios_server.id`).
    #[serde(deserialize_with = "lenient::string")]
    pub node_id: String,
    /// URL of the server gateway, `https://host:port` (`http://` disables TLS).
    pub server_url: String,
    /// Enrolment token, sent as bearer metadata.
    #[serde(default)]
    pub token: Option<Secret>,
    /// File holding the enrolment token; used when `token` is not set.
    #[serde(default, deserialize_with = "lenient::option_path")]
    pub token_file: Option<PathBuf>,
    /// Directory where the job journal survives a restart; memory only when unset.
    #[serde(default, deserialize_with = "lenient::option_path")]
    pub state_dir: Option<PathBuf>,
    /// Time between SIGTERM and SIGKILL when a command is stopped, in milliseconds.
    #[serde(default = "default_kill_grace_ms")]
    pub kill_grace_ms: u64,
    /// TLS settings of the connection to the server.
    #[serde(default)]
    pub tls: TlsConfig,
    /// Reconnection backoff.
    #[serde(default)]
    pub reconnect: ReconnectConfig,
    /// Number of `RunCommands` jobs executed at the same time.
    #[serde(default = "default_max_concurrent_jobs")]
    pub max_concurrent_jobs: usize,
    /// Timeout of a command that does not carry its own, in seconds.
    #[serde(default = "default_command_timeout_s")]
    pub default_command_timeout_s: u64,
    /// Cap on the captured stdout and on the captured stderr of a command.
    #[serde(default = "default_max_output_bytes")]
    pub max_output_bytes: usize,
    /// Time given to running jobs when the agent stops, in seconds.
    #[serde(default = "default_shutdown_grace_s")]
    pub shutdown_grace_s: u64,
    /// Largest configuration bundle accepted, in bytes.
    #[serde(default = "default_max_bundle_bytes")]
    pub max_bundle_bytes: u64,
    /// Cap on the unpacked size of a configuration bundle, in bytes.
    #[serde(default = "default_max_unpacked_bytes")]
    pub max_unpacked_bytes: u64,
    /// Number of recent jobs remembered for deduplication and replay.
    #[serde(default = "default_job_history")]
    pub job_history: usize,
    /// Cap on the job events kept in memory, in bytes.
    #[serde(default = "default_max_buffered_event_bytes")]
    pub max_buffered_event_bytes: usize,
    /// `RunCommands` policy.
    #[serde(default)]
    pub commands: CommandsConfig,
    /// Engine command pipe.
    #[serde(default)]
    pub engine: EngineConfig,
    /// Configuration bundle destinations, by target name.
    #[serde(default)]
    pub targets: BTreeMap<String, TargetConfig>,
    /// Service control command lines, by service name.
    #[serde(default)]
    pub services: BTreeMap<String, ServiceConfig>,
}

/// TLS settings.
#[derive(Debug, Clone, Default, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct TlsConfig {
    /// PEM bundle of the authorities signing the server certificate; the
    /// system trust store is used when unset.
    #[serde(default, deserialize_with = "lenient::option_path")]
    pub ca_file: Option<PathBuf>,
    /// PEM client certificate (mTLS); requires `key_file`.
    #[serde(default, deserialize_with = "lenient::option_path")]
    pub cert_file: Option<PathBuf>,
    /// PEM client private key (mTLS); requires `cert_file`.
    #[serde(default, deserialize_with = "lenient::option_path")]
    pub key_file: Option<PathBuf>,
    /// Name expected in the server certificate when it differs from the URL host.
    #[serde(default, deserialize_with = "lenient::option")]
    pub server_name: Option<String>,
}

/// Jittered exponential backoff between connection attempts.
#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct ReconnectConfig {
    /// First and smallest delay, in milliseconds.
    #[serde(default = "default_reconnect_min_ms")]
    pub min_ms: u64,
    /// Largest delay, in milliseconds.
    #[serde(default = "default_reconnect_max_ms")]
    pub max_ms: u64,
}

impl Default for ReconnectConfig {
    fn default() -> Self {
        Self {
            min_ms: default_reconnect_min_ms(),
            max_ms: default_reconnect_max_ms(),
        }
    }
}

/// `RunCommands` policy.
#[derive(Debug, Clone, Default, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct CommandsConfig {
    /// Regular expressions; a command runs only when one matches it entirely.
    #[serde(default, deserialize_with = "lenient::list")]
    pub allowlist: Vec<String>,
    /// Run commands through `sh -c` instead of executing them directly.
    #[serde(default)]
    pub shell: bool,
}

/// Engine command pipe.
#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct EngineConfig {
    /// Path of the external command FIFO of centengine.
    #[serde(default = "default_command_file", deserialize_with = "lenient::path")]
    pub command_file: PathBuf,
}

impl Default for EngineConfig {
    fn default() -> Self {
        Self {
            command_file: default_command_file(),
        }
    }
}

/// Destination of the configuration bundles of one target.
#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct TargetConfig {
    /// Directory the bundle is synchronised into.
    #[serde(deserialize_with = "lenient::path")]
    pub path: PathBuf,
    /// Remove the files of `path` that are not in the bundle.
    #[serde(default)]
    pub prune: bool,
}

/// Command lines controlling one service; an unset operation is refused.
#[derive(Debug, Clone, Default, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct ServiceConfig {
    /// Command line starting the service.
    #[serde(default, deserialize_with = "lenient::option")]
    pub start: Option<String>,
    /// Command line stopping the service.
    #[serde(default, deserialize_with = "lenient::option")]
    pub stop: Option<String>,
    /// Command line reloading the service.
    #[serde(default, deserialize_with = "lenient::option")]
    pub reload: Option<String>,
    /// Command line restarting the service.
    #[serde(default, deserialize_with = "lenient::option")]
    pub restart: Option<String>,
}

impl ServiceConfig {
    /// Command line mapped to `operation`, if the administrator set one.
    #[must_use]
    pub fn command(&self, operation: Operation) -> Option<&str> {
        match operation {
            Operation::Start => self.start.as_deref(),
            Operation::Stop => self.stop.as_deref(),
            Operation::Reload => self.reload.as_deref(),
            Operation::Restart => self.restart.as_deref(),
            Operation::Unspecified => None,
        }
    }
}

fn default_kill_grace_ms() -> u64 {
    2000
}
fn default_max_concurrent_jobs() -> usize {
    4
}
fn default_command_timeout_s() -> u64 {
    30
}
fn default_max_output_bytes() -> usize {
    64 * 1024
}
fn default_shutdown_grace_s() -> u64 {
    30
}
fn default_max_bundle_bytes() -> u64 {
    256 * 1024 * 1024
}
fn default_max_unpacked_bytes() -> u64 {
    gorgone_core::bundle::DEFAULT_MAX_UNPACKED
}
fn default_job_history() -> usize {
    1024
}
fn default_max_buffered_event_bytes() -> usize {
    64 * 1024 * 1024
}
fn default_reconnect_min_ms() -> u64 {
    500
}
fn default_reconnect_max_ms() -> u64 {
    30_000
}
fn default_command_file() -> PathBuf {
    PathBuf::from("/var/lib/centreon-engine/rw/centengine.cmd")
}

impl AgentConfig {
    /// Loads the configuration from `file` (skipped when absent or `None`)
    /// and from the `GORGONE_AGENT__` environment variables, which win.
    pub fn load(file: Option<&Path>) -> Result<Self, ConfigError> {
        let mut figment = Figment::new();
        if let Some(file) = file {
            figment = figment.merge(Toml::file(file));
        }
        figment = figment.merge(Env::prefixed(ENV_PREFIX).split("__"));
        Self::finish(figment)
    }

    /// Parses a TOML document, ignoring the environment.
    pub fn from_toml(toml: &str) -> Result<Self, ConfigError> {
        Self::finish(Figment::new().merge(Toml::string(toml)))
    }

    fn finish(figment: Figment) -> Result<Self, ConfigError> {
        let mut config: Self = figment.extract().map_err(Box::new)?;
        config.resolve_token()?;
        config.validate()?;
        Ok(config)
    }

    fn resolve_token(&mut self) -> Result<(), ConfigError> {
        if self.token.is_some() {
            return Ok(());
        }
        if let Some(path) = &self.token_file {
            let raw = std::fs::read_to_string(path).map_err(|source| ConfigError::Io {
                path: path.clone(),
                source,
            })?;
            self.token = Some(Secret::new(raw.trim()));
        }
        Ok(())
    }

    fn validate(&self) -> Result<(), ConfigError> {
        let invalid = |message: &str| Err(ConfigError::Invalid(message.to_owned()));
        if self.node_id.trim().is_empty() {
            return invalid("node_id must not be empty");
        }
        if !(self.server_url.starts_with("https://") || self.server_url.starts_with("http://")) {
            return invalid("server_url must start with https:// or http://");
        }
        if self.tls.cert_file.is_some() != self.tls.key_file.is_some() {
            return invalid("tls.cert_file and tls.key_file must be set together");
        }
        match &self.token {
            Some(token) if token.expose().is_empty() => return invalid("token must not be empty"),
            // A client certificate is the only accepted substitute for the token.
            None if self.tls.cert_file.is_none() => {
                return invalid("token or token_file is required without a client certificate");
            }
            _ => {}
        }
        if self.max_concurrent_jobs == 0 {
            return invalid("max_concurrent_jobs must be at least 1");
        }
        if self.default_command_timeout_s == 0 {
            return invalid("default_command_timeout_s must be at least 1");
        }
        if self.job_history == 0 {
            return invalid("job_history must be at least 1");
        }
        if self.reconnect.min_ms == 0 || self.reconnect.min_ms > self.reconnect.max_ms {
            return invalid("reconnect.min_ms must be between 1 and reconnect.max_ms");
        }
        Ok(())
    }

    /// True when the connection to the server uses TLS.
    #[must_use]
    pub fn uses_tls(&self) -> bool {
        self.server_url.starts_with("https://")
    }

    pub(crate) fn default_command_timeout(&self) -> Duration {
        Duration::from_secs(self.default_command_timeout_s)
    }

    pub(crate) fn kill_grace(&self) -> Duration {
        Duration::from_millis(self.kill_grace_ms)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const MINIMAL: &str = r#"
        node_id = "7"
        server_url = "https://central:5556"
        token = "t"
    "#;

    #[test]
    fn minimal_config_gets_defaults() {
        let config = AgentConfig::from_toml(MINIMAL).unwrap();
        assert_eq!(config.max_concurrent_jobs, 4);
        assert_eq!(config.kill_grace_ms, 2000);
        assert!(config.state_dir.is_none());
        assert!(!config.commands.shell);
        assert!(config.commands.allowlist.is_empty());
        assert!(config.targets.is_empty() && config.services.is_empty());
        assert_eq!(
            config.engine.command_file,
            PathBuf::from("/var/lib/centreon-engine/rw/centengine.cmd")
        );
        assert!(config.uses_tls());
    }

    #[test]
    fn example_file_is_valid() {
        // The token file of the example does not exist here.
        let example = include_str!("../config.example.toml").replace("token_file = ", "token = ");
        let config = AgentConfig::from_toml(&example).unwrap();
        assert_eq!(config.node_id, "1");
        assert!(config.targets["engine"].prune);
        assert!(!config.targets["broker"].prune);
        assert_eq!(
            config.services["engine"].command(Operation::Reload),
            Some("sudo systemctl reload centengine")
        );
        assert_eq!(
            config.services["broker"].command(Operation::Unspecified),
            None
        );
    }

    #[test]
    fn rejects_bad_values() {
        for (toml, needle) in [
            (
                r#"node_id = ""
                   server_url = "https://c"
                   token = "t""#,
                "node_id",
            ),
            (
                r#"node_id = "1"
                   server_url = "central:5556"
                   token = "t""#,
                "server_url",
            ),
            (
                r#"node_id = "1"
                   server_url = "https://c""#,
                "token",
            ),
            (
                r#"node_id = "1"
                   server_url = "https://c"
                   token = "t"
                   typo = 1"#,
                "typo",
            ),
        ] {
            let message = AgentConfig::from_toml(toml).unwrap_err().to_string();
            assert!(message.contains(needle), "{message}");
        }
    }

    #[test]
    fn secret_is_redacted() {
        let config = AgentConfig::from_toml(MINIMAL).unwrap();
        assert!(!format!("{config:?}").contains("\"t\""));
    }

    // `Jail` imposes `figment::Error` as the error type of its closure.
    #[test]
    #[allow(clippy::result_large_err)]
    fn typed_looking_environment_values_stay_text() {
        figment::Jail::expect_with(|jail| {
            jail.set_env("GORGONE_AGENT__NODE_ID", "7");
            jail.set_env("GORGONE_AGENT__SERVER_URL", "https://central:5556");
            jail.set_env("GORGONE_AGENT__TOKEN", "123456");
            jail.set_env("GORGONE_AGENT__STATE_DIR", "1");
            jail.set_env("GORGONE_AGENT__KILL_GRACE_MS", "250");
            jail.set_env("GORGONE_AGENT__TLS__SERVER_NAME", "10");
            jail.set_env("GORGONE_AGENT__TLS__CA_FILE", "2");
            jail.set_env("GORGONE_AGENT__ENGINE__COMMAND_FILE", "3");
            jail.set_env(
                "GORGONE_AGENT__COMMANDS__ALLOWLIST",
                r#"[true, 42, "echo .*"]"#,
            );
            jail.set_env("GORGONE_AGENT__SERVICES__ENGINE__RELOAD", "true");
            jail.set_env("GORGONE_AGENT__SERVICES__ENGINE__STOP", "1.5");
            // Inner double quotes keep a value exactly as written.
            jail.set_env("GORGONE_AGENT__SERVICES__ENGINE__START", r#""1.50""#);
            jail.set_env("GORGONE_AGENT__SERVICES__ENGINE__RESTART", r#""[x]""#);
            jail.set_env("GORGONE_AGENT__TARGETS__7__PATH", "8");
            let config = AgentConfig::load(None).unwrap();
            assert_eq!(config.node_id, "7");
            assert_eq!(config.token.unwrap().expose(), "123456");
            assert_eq!(config.state_dir, Some(PathBuf::from("1")));
            assert_eq!(config.kill_grace_ms, 250);
            assert_eq!(config.tls.server_name.as_deref(), Some("10"));
            assert_eq!(config.tls.ca_file, Some(PathBuf::from("2")));
            assert_eq!(config.engine.command_file, PathBuf::from("3"));
            assert_eq!(config.commands.allowlist, ["true", "42", "echo .*"]);
            assert_eq!(config.services["engine"].reload.as_deref(), Some("true"));
            assert_eq!(config.services["engine"].stop.as_deref(), Some("1.5"));
            assert_eq!(config.services["engine"].start.as_deref(), Some("1.50"));
            assert_eq!(config.services["engine"].restart.as_deref(), Some("[x]"));
            assert_eq!(config.targets["7"].path, PathBuf::from("8"));
            Ok(())
        });
    }

    #[test]
    #[allow(clippy::result_large_err)]
    fn environment_overrides_file_and_reads_token_file() {
        figment::Jail::expect_with(|jail| {
            jail.create_file("agent.toml", MINIMAL.replace("token = \"t\"", "").as_str())?;
            jail.create_file("token", "from-file\n")?;
            jail.set_env("GORGONE_AGENT__NODE_ID", "42");
            jail.set_env("GORGONE_AGENT__TOKEN_FILE", "token");
            jail.set_env("GORGONE_AGENT__COMMANDS__SHELL", "true");
            jail.set_env("GORGONE_AGENT__SERVICES__ENGINE__RELOAD", "sudo reload");
            jail.set_env("GORGONE_AGENT__TARGETS__ENGINE__PATH", "/tmp/engine");
            let config = AgentConfig::load(Some(Path::new("agent.toml"))).unwrap();
            assert_eq!(config.node_id, "42");
            assert_eq!(config.token.unwrap().expose(), "from-file");
            assert!(config.commands.shell);
            assert_eq!(
                config.services["engine"].reload.as_deref(),
                Some("sudo reload")
            );
            assert_eq!(config.targets["engine"].path, PathBuf::from("/tmp/engine"));
            Ok(())
        });
    }
}
