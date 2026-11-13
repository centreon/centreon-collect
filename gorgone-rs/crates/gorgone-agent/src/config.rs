//! Agent configuration: one TOML file, every key overridable by environment
//! (`GORGONE_AGENT__SECTION__KEY`).

use std::collections::BTreeMap;
use std::fmt;
use std::net::IpAddr;
use std::path::{Component, Path, PathBuf};
use std::time::Duration;

use gorgone_proto::agent::service_control::Operation;
use serde::Deserialize;
use toml::{Table, Value};
use tonic::transport::Uri;

/// Prefix of the environment overrides; `__` separates nesting levels.
pub const ENV_PREFIX: &str = "GORGONE_AGENT__";

/// Configuration errors.
#[derive(Debug, thiserror::Error)]
pub enum ConfigError {
    /// File or environment could not be parsed.
    #[error("invalid configuration: {0}")]
    Parse(#[from] Box<toml::de::Error>),
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

/// Text that may arrive typed: the environment overlay turns `7` into an
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
    /// PEM bundle of the authorities signing the server certificate. Required
    /// with an `https://` URL unless the agent is built with the `native-roots`
    /// feature, which falls back to the system trust store.
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

/// How the agent reaches the engine: its command pipe, or its gRPC API when
/// `grpc_url` is set.
#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct EngineConfig {
    /// Path of the external command FIFO of centengine; unused with `grpc_url`.
    #[serde(default = "default_command_file", deserialize_with = "lenient::path")]
    pub command_file: PathBuf,
    /// URL of the engine gRPC API: `unix:/absolute/path` of its socket, or
    /// `http://host:port`. When set, external commands go through it instead
    /// of `command_file`.
    #[serde(default, deserialize_with = "lenient::option")]
    pub grpc_url: Option<String>,
    /// Time given to the engine API to accept a connection, and to answer one
    /// call, in milliseconds.
    #[serde(default = "default_grpc_timeout_ms")]
    pub grpc_timeout_ms: u64,
    /// Regular expressions; an external command is written only when one
    /// matches its name entirely. Empty: every command is refused.
    #[serde(default, deserialize_with = "lenient::list")]
    pub allowed_commands: Vec<String>,
    /// Command line checking an engine configuration when `grpc_url` is not
    /// set; `{config}` stands for the main file to check. It exits with 0
    /// when the configuration is valid.
    #[serde(default, deserialize_with = "lenient::option")]
    pub verify_command: Option<String>,
    /// Time given to the engine API to check a configuration, in milliseconds.
    #[serde(default = "default_verify_timeout_ms")]
    pub verify_timeout_ms: u64,
}

impl Default for EngineConfig {
    fn default() -> Self {
        Self {
            command_file: default_command_file(),
            grpc_url: None,
            grpc_timeout_ms: default_grpc_timeout_ms(),
            allowed_commands: Vec::new(),
            verify_command: None,
            verify_timeout_ms: default_verify_timeout_ms(),
        }
    }
}

impl EngineConfig {
    /// Placeholder of `verify_command` replaced by the file to check.
    pub const VERIFY_PLACEHOLDER: &'static str = "{config}";

    /// True when `grpc_url` names this host: `localhost` or a loopback address.
    #[must_use]
    pub fn grpc_is_loopback(&self) -> bool {
        let host = self.grpc_url.as_deref().and_then(|url| {
            let uri = url.parse::<Uri>().ok()?;
            Some(uri.host()?.to_owned())
        });
        host.is_some_and(|host| {
            let address = host.trim_start_matches('[').trim_end_matches(']');
            host.eq_ignore_ascii_case("localhost")
                || address.parse::<IpAddr>().is_ok_and(|ip| ip.is_loopback())
        })
    }

    /// Socket path when `grpc_url` is a `unix:` URL.
    #[must_use]
    pub fn grpc_socket(&self) -> Option<&Path> {
        let url = self.grpc_url.as_deref()?;
        // Same reading as tonic, which dials the URL.
        let path = url
            .strip_prefix("unix://")
            .or_else(|| url.strip_prefix("unix:"))?;
        Some(Path::new(path))
    }

    pub(crate) fn grpc_timeout(&self) -> Duration {
        Duration::from_millis(self.grpc_timeout_ms)
    }

    pub(crate) fn verify_timeout(&self) -> Duration {
        Duration::from_millis(self.verify_timeout_ms)
    }

    /// Arguments of `verify_command`, placeholder included, if it is set.
    pub(crate) fn verify_argv(&self) -> Result<Option<Vec<String>>, ConfigError> {
        let Some(line) = &self.verify_command else {
            return Ok(None);
        };
        let invalid = |reason: &str| {
            Err(ConfigError::Invalid(format!(
                "engine.verify_command: {reason}"
            )))
        };
        let Ok(argv) = shell_words::split(line) else {
            return invalid("unbalanced quotes");
        };
        // The program itself is never taken from the placeholder.
        let Some((program, arguments)) = argv.split_first() else {
            return invalid("must start with the program to run");
        };
        if program.is_empty() || program.contains(Self::VERIFY_PLACEHOLDER) {
            return invalid("must start with the program to run");
        }
        if !arguments
            .iter()
            .any(|argument| argument.contains(Self::VERIFY_PLACEHOLDER))
        {
            return invalid(
                "must hold {config}, replaced by the file to check, as in \
                 /usr/sbin/centengine -v {config}",
            );
        }
        Ok(Some(argv))
    }

    fn validate(&self) -> Result<(), ConfigError> {
        let invalid = |message: &str| Err(ConfigError::Invalid(message.to_owned()));
        if self.grpc_timeout_ms == 0 {
            return invalid("engine.grpc_timeout_ms must be at least 1");
        }
        if self.verify_timeout_ms == 0 {
            return invalid("engine.verify_timeout_ms must be at least 1");
        }
        self.verify_argv()?;
        let Some(url) = &self.grpc_url else {
            return Ok(());
        };
        if let Some(socket) = self.grpc_socket() {
            // A relative path would depend on the directory the agent starts in.
            if socket.is_absolute() && !url.ends_with('/') {
                return Ok(());
            }
            return invalid(
                "engine.grpc_url: unix: must be followed by the absolute path of the engine \
                 socket, as in unix:/var/lib/centreon-engine/rw/engine.sock",
            );
        }
        match url.parse::<Uri>() {
            Ok(uri) if uri.scheme_str() == Some("http") && uri.host().is_some() => Ok(()),
            Ok(uri) if uri.scheme_str() == Some("https") => {
                invalid("engine.grpc_url: TLS is not supported on this channel, use http://")
            }
            _ => invalid("engine.grpc_url must be unix:/absolute/path or http://host:port"),
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
    /// Main file of an engine configuration, relative to `path`. When set, a
    /// bundle is checked by the engine before it is applied.
    #[serde(default, deserialize_with = "lenient::option_path")]
    pub engine_main_file: Option<PathBuf>,
}

/// Call of the engine gRPC API a service operation can be mapped to.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum EngineRpc {
    /// `grpc:reload`: `SignalProcess(RESTART)`, the engine reloads its
    /// configuration without stopping, as on SIGHUP.
    Reload,
    /// `grpc:shutdown`: `ShutdownProgram`, the engine process ends.
    Shutdown,
}

impl EngineRpc {
    /// Prefix telling a call of the engine API from a command line.
    pub const PREFIX: &'static str = "grpc:";

    fn parse(name: &str) -> Option<Self> {
        match name {
            "reload" => Some(Self::Reload),
            "shutdown" => Some(Self::Shutdown),
            _ => None,
        }
    }
}

/// What the agent does for one operation of a service.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ServiceAction<'a> {
    /// Runs this command line, without a shell.
    Command(&'a str),
    /// Calls the engine gRPC API (`engine.grpc_url`).
    Engine(EngineRpc),
}

/// Actions controlling one service; an unset operation is refused. A value is
/// a command line, or `grpc:reload` / `grpc:shutdown` (see [`EngineRpc`]).
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
    /// Value mapped to `operation` as written, if the administrator set one;
    /// [`ServiceConfig::action`] tells a command line from an engine call.
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

    /// What `operation` does, if the administrator mapped it. `None` as well
    /// for an unknown `grpc:` name, which the configuration check refuses.
    #[must_use]
    pub fn action(&self, operation: Operation) -> Option<ServiceAction<'_>> {
        let value = self.command(operation)?;
        match value.strip_prefix(EngineRpc::PREFIX) {
            Some(name) => EngineRpc::parse(name).map(ServiceAction::Engine),
            None => Some(ServiceAction::Command(value)),
        }
    }

    /// Names of the engine API calls the four operations are mapped to.
    fn engine_calls(&self) -> impl Iterator<Item = &str> {
        [&self.start, &self.stop, &self.reload, &self.restart]
            .into_iter()
            .filter_map(|value| value.as_deref()?.strip_prefix(EngineRpc::PREFIX))
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
fn default_grpc_timeout_ms() -> u64 {
    5000
}
fn default_verify_timeout_ms() -> u64 {
    70_000
}
fn default_command_file() -> PathBuf {
    PathBuf::from("/var/lib/centreon-engine/rw/centengine.cmd")
}

/// Reads an environment value: canonical TOML integers, booleans and floats
/// are typed, as are quoted strings, arrays and inline tables; anything else
/// is text, as written.
fn env_value(raw: &str) -> Value {
    match raw.parse::<Value>() {
        Ok(Value::Datetime(_)) | Err(_) => Value::String(raw.to_owned()),
        // `1.50` or `+7` would not survive a round trip through a number.
        Ok(Value::Integer(number)) if number.to_string() != raw => Value::String(raw.to_owned()),
        Ok(Value::Float(number)) if number.to_string() != raw => Value::String(raw.to_owned()),
        Ok(value) => value,
    }
}

/// Sets the key `a.b` of `table` for every `<prefix>A__B=value` of `vars`.
fn overlay_env(
    table: &mut Table,
    prefix: &str,
    vars: impl IntoIterator<Item = (String, String)>,
) -> Result<(), ConfigError> {
    // Sorted, so that `A` and `A__B` always combine the same way.
    let vars: BTreeMap<String, String> = vars
        .into_iter()
        .filter(|(name, _)| name.starts_with(prefix))
        .collect();
    for (name, raw) in vars {
        let path = name[prefix.len()..].to_lowercase();
        let mut keys: Vec<&str> = path.split("__").collect();
        let last = keys.pop().unwrap_or_default();
        if last.is_empty() || keys.iter().any(|key| key.is_empty()) {
            return Err(ConfigError::Invalid(format!("{name}: empty key")));
        }
        let mut current = &mut *table;
        for key in keys {
            let entry = current
                .entry(key)
                .or_insert_with(|| Value::Table(Table::new()));
            // A scalar gives way to the table the variable asks for.
            if !entry.is_table() {
                *entry = Value::Table(Table::new());
            }
            let Value::Table(next) = entry else {
                return Err(ConfigError::Invalid(format!("{name}: not a table")));
            };
            current = next;
        }
        current.insert(last.to_owned(), env_value(&raw));
    }
    Ok(())
}

impl AgentConfig {
    /// Loads the configuration from `file` (skipped when absent or `None`)
    /// and from the `GORGONE_AGENT__` environment variables, which win.
    pub fn load(file: Option<&Path>) -> Result<Self, ConfigError> {
        // A variable that is not Unicode is ignored.
        let vars = std::env::vars_os().filter_map(|(name, value)| {
            Some((name.into_string().ok()?, value.into_string().ok()?))
        });
        Self::load_with(file, vars)
    }

    fn load_with(
        file: Option<&Path>,
        vars: impl IntoIterator<Item = (String, String)>,
    ) -> Result<Self, ConfigError> {
        let mut table = Table::new();
        if let Some(path) = file {
            match std::fs::read_to_string(path) {
                Ok(text) => table = text.parse().map_err(Box::new)?,
                Err(error) if error.kind() == std::io::ErrorKind::NotFound => {}
                Err(source) => {
                    return Err(ConfigError::Io {
                        path: path.to_path_buf(),
                        source,
                    });
                }
            }
        }
        overlay_env(&mut table, ENV_PREFIX, vars)?;
        Self::finish(table)
    }

    /// Parses a TOML document, ignoring the environment.
    pub fn from_toml(toml: &str) -> Result<Self, ConfigError> {
        Self::finish(toml.parse().map_err(Box::new)?)
    }

    fn finish(table: Table) -> Result<Self, ConfigError> {
        let mut config: Self = table.try_into().map_err(Box::new)?;
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
        if self.uses_tls() && self.tls.ca_file.is_none() && !cfg!(feature = "native-roots") {
            return invalid(
                "tls.ca_file is required with an https server_url: this build does not \
                 read the system trust store (cargo feature `native-roots`)",
            );
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
        self.engine.validate()?;
        for (name, target) in &self.targets {
            let Some(main_file) = &target.engine_main_file else {
                continue;
            };
            let plain = main_file
                .components()
                .all(|component| matches!(component, Component::Normal(_)));
            if !plain || main_file.as_os_str().is_empty() {
                return Err(ConfigError::Invalid(format!(
                    "targets.{name}.engine_main_file must be a path relative to the target \
                     path, without `.` or `..`"
                )));
            }
            if self.engine.grpc_url.is_none() && self.engine.verify_command.is_none() {
                return Err(ConfigError::Invalid(format!(
                    "targets.{name}.engine_main_file requires engine.grpc_url or \
                     engine.verify_command to check the configuration"
                )));
            }
        }
        for (service, config) in &self.services {
            for name in config.engine_calls() {
                if EngineRpc::parse(name).is_none() {
                    return Err(ConfigError::Invalid(format!(
                        "services.{service}: unknown engine call grpc:{name}, \
                         expected grpc:reload or grpc:shutdown"
                    )));
                }
                if self.engine.grpc_url.is_none() {
                    return Err(ConfigError::Invalid(format!(
                        "services.{service}: grpc:{name} requires engine.grpc_url"
                    )));
                }
            }
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
        tls.ca_file = "/etc/centreon-gorgone/ca.pem"
    "#;

    fn vars(vars: &[(&str, &str)]) -> Vec<(String, String)> {
        vars.iter()
            .map(|(name, value)| (format!("{ENV_PREFIX}{name}"), (*value).to_owned()))
            .collect()
    }

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
        assert_eq!(
            config.targets["engine"].engine_main_file,
            Some(PathBuf::from("centengine.cfg"))
        );
        assert!(config.targets["broker"].engine_main_file.is_none());
        assert_eq!(
            config.engine.verify_argv().unwrap().unwrap(),
            ["/usr/sbin/centengine", "-v", "{config}"]
        );
        assert_eq!(config.engine.verify_timeout_ms, 70_000);
        assert!(!config.targets["broker"].prune);
        assert_eq!(
            config.services["engine"].command(Operation::Reload),
            Some("sudo systemctl reload centengine")
        );
        assert_eq!(
            config.services["broker"].command(Operation::Unspecified),
            None
        );

        // The example policies say what their comments promise.
        use gorgone_core::allowlist::Allowlist;
        let commands = Allowlist::new(&config.commands.allowlist).unwrap();
        for allowed in [
            "/usr/lib/centreon/plugins/centreon_linux_local.pl --plugin=os::linux::local::plugin --mode=cpu --warning=80 --critical=90",
            "/usr/lib64/nagios/plugins/check_ping",
            "/usr/sbin/centengine -v /etc/centreon-engine/centengine.cfg",
        ] {
            assert!(commands.authorize(allowed).is_ok(), "{allowed}");
        }
        for refused in [
            "/usr/lib/centreon/plugins/centreon_linux_local.pl --plugin=os::linux::local::plugin --mode=cpu --warning=80 --critical=90 --extra",
            "/usr/lib64/nagios/plugins/check_ping -H host",
            "/usr/lib64/nagios/plugins/../../../bin/sh",
            "/usr/sbin/centengine -v /tmp/other.cfg",
        ] {
            assert!(commands.authorize(refused).is_err(), "{refused}");
        }
        let engine = Allowlist::new(&config.engine.allowed_commands).unwrap();
        for allowed in [
            "ACKNOWLEDGE_SVC_PROBLEM",
            "SCHEDULE_HOST_DOWNTIME",
            "SCHEDULE_FORCED_SVC_CHECK",
            "PROCESS_SERVICE_CHECK_RESULT",
            "DISABLE_HOST_NOTIFICATIONS",
            "ADD_SVC_COMMENT",
        ] {
            assert!(engine.is_allowed(allowed), "{allowed}");
        }
        for refused in [
            "PROCESS_FILE",
            "CHANGE_HOST_CHECK_COMMAND",
            "CHANGE_SVC_EVENT_HANDLER",
            "CHANGE_GLOBAL_HOST_EVENT_HANDLER",
            "CHANGE_CUSTOM_HOST_VAR",
        ] {
            assert!(!engine.is_allowed(refused), "{refused}");
        }
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
    fn engine_api_settings_are_checked() {
        let config = AgentConfig::from_toml(MINIMAL).unwrap();
        assert!(config.engine.grpc_url.is_none());
        assert_eq!(config.engine.grpc_timeout_ms, 5000);
        assert!(!config.engine.grpc_is_loopback());

        let with = |tail: &str| AgentConfig::from_toml(&format!("{MINIMAL}{tail}"));
        for (url, loopback) in [
            ("http://127.0.0.1:50155", true),
            ("http://localhost:50155", true),
            ("http://[::1]:50155", true),
            ("http://engine:50155", false),
            ("http://10.0.0.7:50155", false),
        ] {
            let config = with(&format!("[engine]\ngrpc_url = \"{url}\"")).unwrap();
            assert_eq!(config.engine.grpc_is_loopback(), loopback, "{url}");
        }
        for url in ["unix:/run/engine.sock", "unix:///run/engine.sock"] {
            let config = with(&format!("[engine]\ngrpc_url = \"{url}\"")).unwrap();
            assert_eq!(
                config.engine.grpc_socket(),
                Some(Path::new("/run/engine.sock"))
            );
            assert!(!config.engine.grpc_is_loopback());
        }
        for (url, needle) in [
            ("unix:", "absolute path"),
            ("unix:engine.sock", "absolute path"),
            ("unix:./engine.sock", "absolute path"),
            ("unix://run/engine.sock", "absolute path"),
            ("unix:/run/", "absolute path"),
            ("unix:///", "absolute path"),
            (
                "UNIX:/run/engine.sock",
                "unix:/absolute/path or http://host:port",
            ),
        ] {
            let message = with(&format!("[engine]\ngrpc_url = \"{url}\""))
                .unwrap_err()
                .to_string();
            assert!(message.contains(needle), "{url}: {message}");
        }
        for (tail, needle) in [
            ("[engine]\ngrpc_url = \"https://engine:50155\"", "TLS"),
            ("[engine]\ngrpc_url = \"engine:50155\"", "http://host:port"),
            ("[engine]\ngrpc_url = \"\"", "http://host:port"),
            ("[engine]\ngrpc_timeout_ms = 0", "grpc_timeout_ms"),
            (
                "[services.engine]\nrestart = \"grpc:shutdown\"",
                "requires engine.grpc_url",
            ),
            (
                "[engine]\ngrpc_url = \"http://e:1\"\n[services.engine]\nstart = \"grpc:start\"",
                "unknown engine call grpc:start",
            ),
        ] {
            let message = with(tail).unwrap_err().to_string();
            assert!(message.contains(needle), "{message}");
        }
    }

    #[test]
    fn configuration_check_settings_are_checked() {
        let with = |tail: &str| AgentConfig::from_toml(&format!("{MINIMAL}{tail}"));
        let target = "[targets.engine]\npath = \"/etc/centreon-engine\"\n";
        let config = with(target).unwrap();
        assert!(config.targets["engine"].engine_main_file.is_none());
        assert!(config.engine.verify_command.is_none());
        assert_eq!(config.engine.verify_timeout_ms, 70_000);

        let grpc = "[engine]\ngrpc_url = \"http://e:1\"\n";
        let main_file = |value: &str| format!("{target}engine_main_file = \"{value}\"\n");
        for engine in [
            grpc,
            "[engine]\nverify_command = \"/usr/sbin/centengine -v {config}\"\n",
            "[engine]\nverify_command = \"sudo -u centreon-engine 'centengine' --check={config}\"\n",
        ] {
            let config = with(&format!("{engine}{}", main_file("conf/centengine.cfg"))).unwrap();
            assert_eq!(
                config.targets["engine"].engine_main_file,
                Some(PathBuf::from("conf/centengine.cfg"))
            );
        }
        let command = |line: &str| format!("[engine]\nverify_command = \"{line}\"");
        for (tail, needle) in [
            (main_file("centengine.cfg"), "requires engine.grpc_url or"),
            (
                format!("{grpc}{}", main_file("/etc/centengine.cfg")),
                "relative to the target",
            ),
            (
                format!("{grpc}{}", main_file("../centengine.cfg")),
                "relative to the target",
            ),
            (format!("{grpc}{}", main_file("")), "relative to the target"),
            (command("/usr/sbin/centengine -v"), "must hold {config}"),
            (
                command("{config} -v {config}"),
                "must start with the program",
            ),
            (command(""), "must start with the program"),
            (command("centengine -v '{config}"), "unbalanced quotes"),
            (
                "[engine]\nverify_timeout_ms = 0".to_owned(),
                "verify_timeout_ms",
            ),
        ] {
            let message = with(&tail).unwrap_err().to_string();
            assert!(message.contains(needle), "{message}");
        }
    }

    #[test]
    fn service_operation_is_a_command_or_an_engine_call() {
        let config = AgentConfig::from_toml(&format!(
            "{MINIMAL}[engine]\ngrpc_url = \"http://engine:50155\"\n\
             [services.engine]\nreload = \"grpc:reload\"\nrestart = \"grpc:shutdown\"\n\
             stop = \"sudo systemctl stop centengine\""
        ))
        .unwrap();
        let engine = &config.services["engine"];
        assert_eq!(
            engine.action(Operation::Reload),
            Some(ServiceAction::Engine(EngineRpc::Reload))
        );
        assert_eq!(
            engine.action(Operation::Restart),
            Some(ServiceAction::Engine(EngineRpc::Shutdown))
        );
        assert_eq!(
            engine.action(Operation::Stop),
            Some(ServiceAction::Command("sudo systemctl stop centengine"))
        );
        assert_eq!(engine.action(Operation::Start), None);
        assert_eq!(engine.command(Operation::Reload), Some("grpc:reload"));
    }

    #[test]
    fn secret_is_redacted() {
        let config = AgentConfig::from_toml(MINIMAL).unwrap();
        assert!(!format!("{config:?}").contains("\"t\""));
    }

    #[test]
    #[cfg(not(feature = "native-roots"))]
    fn https_requires_an_authority_file() {
        let toml = MINIMAL.replace("tls.ca_file", "tls.server_name");
        let message = AgentConfig::from_toml(&toml).unwrap_err().to_string();
        assert!(message.contains("tls.ca_file is required"), "{message}");
        let clear = toml.replace("https://", "http://");
        assert!(!AgentConfig::from_toml(&clear).unwrap().uses_tls());
    }

    #[test]
    fn environment_values_are_typed_only_when_canonical() {
        assert_eq!(env_value("7"), Value::Integer(7));
        assert_eq!(env_value("true"), Value::Boolean(true));
        assert_eq!(env_value("1.5"), Value::Float(1.5));
        assert_eq!(env_value(r#"["a", 1]"#).as_array().unwrap().len(), 2);
        assert_eq!(env_value(r#""quoted""#), Value::String("quoted".into()));
        for text in [
            "1.50",
            "+7",
            "0x10",
            "1_000",
            "",
            " 7",
            "[x]",
            "{x}",
            "a b",
            "2026-01-01",
        ] {
            assert_eq!(env_value(text), Value::String(text.into()), "{text:?}");
        }
    }

    #[test]
    fn overlay_ignores_other_variables_and_replaces_scalars() {
        let mut table: Table = "a = 1\n[b]\nc = 2".parse().unwrap();
        let vars = [("OTHER__A", "9"), ("P__A__X", "text"), ("P__B__D", "3")];
        let vars = vars.map(|(name, value)| (name.to_owned(), value.to_owned()));
        overlay_env(&mut table, "P__", vars).unwrap();
        let expected: Table = "[a]\nx = 'text'\n[b]\nc = 2\nd = 3".parse().unwrap();
        assert_eq!(table, expected);

        for bad in ["P__", "P__A__", "P____A"] {
            let vars = [(bad.to_owned(), "1".to_owned())];
            assert!(overlay_env(&mut table, "P__", vars).is_err(), "{bad}");
        }
    }

    #[test]
    fn typed_looking_environment_values_stay_text() {
        let vars = vars(&[
            ("NODE_ID", "7"),
            ("SERVER_URL", "http://central:5556"),
            ("TOKEN", "123456"),
            ("STATE_DIR", "1"),
            ("KILL_GRACE_MS", "250"),
            ("TLS__SERVER_NAME", "10"),
            ("TLS__CA_FILE", "2"),
            ("ENGINE__COMMAND_FILE", "3"),
            ("COMMANDS__ALLOWLIST", r#"[true, 42, "echo .*"]"#),
            ("SERVICES__ENGINE__RELOAD", "true"),
            ("SERVICES__ENGINE__STOP", "1.50"),
            ("SERVICES__ENGINE__RESTART", "[x]"),
            // Inner double quotes are removed.
            ("SERVICES__ENGINE__START", r#""[1]""#),
            ("TARGETS__7__PATH", "8"),
        ]);
        let config = AgentConfig::load_with(None, vars).unwrap();
        assert_eq!(config.node_id, "7");
        assert_eq!(config.token.unwrap().expose(), "123456");
        assert_eq!(config.state_dir, Some(PathBuf::from("1")));
        assert_eq!(config.kill_grace_ms, 250);
        assert_eq!(config.tls.server_name.as_deref(), Some("10"));
        assert_eq!(config.tls.ca_file, Some(PathBuf::from("2")));
        assert_eq!(config.engine.command_file, PathBuf::from("3"));
        assert_eq!(config.commands.allowlist, ["true", "42", "echo .*"]);
        assert_eq!(config.services["engine"].reload.as_deref(), Some("true"));
        assert_eq!(config.services["engine"].stop.as_deref(), Some("1.50"));
        assert_eq!(config.services["engine"].restart.as_deref(), Some("[x]"));
        assert_eq!(config.services["engine"].start.as_deref(), Some("[1]"));
        assert_eq!(config.targets["7"].path, PathBuf::from("8"));
    }

    #[test]
    fn environment_overrides_file_and_reads_token_file() {
        let dir = tempfile::tempdir().unwrap();
        let file = dir.path().join("agent.toml");
        let token = dir.path().join("token");
        std::fs::write(&file, MINIMAL.replace("token = \"t\"", "")).unwrap();
        std::fs::write(&token, "from-file\n").unwrap();
        let vars = vars(&[
            ("NODE_ID", "42"),
            ("TOKEN_FILE", token.to_str().unwrap()),
            ("COMMANDS__SHELL", "true"),
            ("SERVICES__ENGINE__RELOAD", "sudo reload"),
            ("TARGETS__ENGINE__PATH", "/tmp/engine"),
        ]);
        let config = AgentConfig::load_with(Some(&file), vars).unwrap();
        assert_eq!(config.node_id, "42");
        assert_eq!(config.token.unwrap().expose(), "from-file");
        assert!(config.commands.shell);
        assert_eq!(
            config.services["engine"].reload.as_deref(),
            Some("sudo reload")
        );
        assert_eq!(config.targets["engine"].path, PathBuf::from("/tmp/engine"));
    }

    #[test]
    fn missing_file_is_skipped_and_broken_file_is_reported() {
        let dir = tempfile::tempdir().unwrap();
        let vars = vars(&[("NODE_ID", "1"), ("SERVER_URL", "http://c"), ("TOKEN", "t")]);
        let absent = dir.path().join("absent.toml");
        assert!(AgentConfig::load_with(Some(&absent), vars.clone()).is_ok());
        let broken = dir.path().join("broken.toml");
        std::fs::write(&broken, "node_id = ").unwrap();
        let error = AgentConfig::load_with(Some(&broken), vars).unwrap_err();
        assert!(matches!(error, ConfigError::Parse(_)), "{error}");
    }
}
