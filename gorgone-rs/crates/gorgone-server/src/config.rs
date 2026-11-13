//! Server configuration: one TOML file, every key overridable by environment.

use std::fmt;
use std::net::SocketAddr;
use std::path::{Path, PathBuf};
use std::time::Duration;

use serde::{Deserialize, Serialize};
use toml::{Table, Value};

pub use crate::cidr::Cidr;
use crate::error::Error;

/// Prefix of the environment overrides; `__` separates nested keys.
pub const ENV_PREFIX: &str = "GORGONE_SERVER__";

/// TLS material of one listener.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TlsConfig {
    /// PEM certificate chain.
    pub cert: PathBuf,
    /// PEM private key.
    pub key: PathBuf,
    /// PEM CA bundle; when set, clients must present a certificate it signed.
    #[serde(default)]
    pub client_ca: Option<PathBuf>,
}

/// What `Debug` shows in place of a secret.
fn redacted<T>(secret: &Option<T>) -> &'static str {
    if secret.is_some() { "***" } else { "None" }
}

/// REST API access control. `Debug` never shows the secrets.
#[derive(Clone, Default, Serialize, Deserialize)]
#[serde(default)]
pub struct RestAuthConfig {
    /// Bearer token; also accepted as the basic-auth password of any user
    /// when no basic credentials are set.
    pub token: Option<String>,
    /// Basic-auth user name.
    pub basic_username: Option<String>,
    /// Basic-auth password.
    pub basic_password: Option<String>,
    /// Client networks allowed to call the API; empty allows every address.
    pub allowed_cidrs: Vec<Cidr>,
}

impl fmt::Debug for RestAuthConfig {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("RestAuthConfig")
            .field("token", &redacted(&self.token))
            .field("basic_username", &self.basic_username)
            .field("basic_password", &redacted(&self.basic_password))
            .field("allowed_cidrs", &self.allowed_cidrs)
            .finish()
    }
}

impl RestAuthConfig {
    pub(crate) fn requires_credentials(&self) -> bool {
        self.token.is_some() || self.basic_password.is_some()
    }
}

/// Legacy `centcore.cmd` intake, for writers that do not call the API yet.
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(default)]
pub struct CentcoreConfig {
    /// Run the intake (only the replica holding the `centcore` lease reads).
    pub enabled: bool,
    /// Command file written by centreon-web.
    pub cmd_file: PathBuf,
    /// Directory of additional `*.cmd` files.
    pub cmd_dir: PathBuf,
    /// Centreon cache directory holding `config/{engine,broker,vmware}/<id>`.
    pub cache_dir: PathBuf,
    /// Directory holding `<id>/centreontrapd.sdb`, source of `SYNCTRAP`.
    pub cache_dir_trap: PathBuf,
    /// Period between two reads, in milliseconds.
    pub poll_interval_ms: u64,
    /// Characters removed from external commands.
    pub illegal_characters: String,
    /// External commands sent per `EngineCommands` job.
    pub bulk_external_cmd: usize,
    /// Owners a command file must have to be read; empty means the user
    /// running the server, or root.
    pub allowed_uids: Vec<u32>,
}

impl Default for CentcoreConfig {
    fn default() -> Self {
        Self {
            enabled: false,
            cmd_file: PathBuf::from("/var/lib/centreon/centcore.cmd"),
            cmd_dir: PathBuf::from("/var/lib/centreon/centcore"),
            cache_dir: PathBuf::from("/var/cache/centreon"),
            cache_dir_trap: PathBuf::from("/etc/snmp/centreon_traps"),
            poll_interval_ms: 5000,
            illegal_characters: String::new(),
            bulk_external_cmd: 50,
            allowed_uids: Vec::new(),
        }
    }
}

/// SQLite `synchronous` setting (the journal is always WAL).
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum SqliteSynchronous {
    /// The log is synced at every commit: an acknowledged job or event
    /// survives a power loss.
    #[default]
    Full,
    /// The log is synced at checkpoints only: several times faster, but the
    /// last commits can be lost on a power loss or an OS crash (not on a
    /// crash of the server alone).
    Normal,
}

/// Settings of one `gorgone-server` replica. `Debug` never shows the secrets
/// (`database_url`, `peer_token`, REST credentials).
#[derive(Clone, Serialize, Deserialize)]
#[serde(default)]
pub struct Config {
    /// REST listener.
    pub rest_listen: SocketAddr,
    /// Agent gateway (gRPC) listener.
    pub agent_listen: SocketAddr,
    /// Replica peer service (gRPC) listener.
    pub peer_listen: SocketAddr,
    /// `sqlite://…` for a single replica, `mysql://…` (MariaDB) otherwise.
    pub database_url: String,
    /// Identifier of this replica; random when unset.
    pub replica_id: Option<String>,
    /// URL the other replicas use to reach the peer listener; derived from
    /// the bound address when unset.
    pub peer_advertise_addr: Option<String>,
    /// Heartbeat interval announced to agents, in seconds.
    pub heartbeat_interval_s: u32,
    /// Deadline of a job submitted without one, in seconds.
    pub job_default_deadline_s: u64,
    /// How long terminal jobs and blobs are kept, in seconds.
    pub retention_s: u64,
    /// Period of the background sweep, in milliseconds.
    pub sweep_interval_ms: u64,
    /// Largest accepted REST body (configuration bundles), in bytes.
    pub body_limit_bytes: usize,
    /// Development only: accept unknown nodes without a token.
    pub auto_enroll: bool,
    /// Stable identifier returned by the legacy thumbprint route; derived
    /// from the database location when unset.
    pub thumbprint: Option<String>,
    /// REST API access control.
    pub rest: RestAuthConfig,
    /// TLS of the REST listener.
    pub rest_tls: Option<TlsConfig>,
    /// TLS of the agent gateway.
    pub agent_tls: Option<TlsConfig>,
    /// Emit logs as JSON.
    pub log_json: bool,
    /// Shared secret the replicas present to each other on the peer service;
    /// unset leaves it open (trusted network only).
    pub peer_token: Option<String>,
    /// TLS of the peer listener; its certificate is also the client identity
    /// when `client_ca` asks for mTLS.
    pub peer_tls: Option<TlsConfig>,
    /// PEM CA bundle used to verify the other replicas' peer certificates.
    pub peer_ca: Option<PathBuf>,
    /// Legacy `centcore.cmd` intake.
    pub centcore: CentcoreConfig,
    /// Accept running the REST API without any credential.
    pub insecure_no_auth: bool,
    /// Accept a listener without TLS on a non-loopback address.
    pub allow_plaintext: bool,
    /// Serve `/metrics` without credentials.
    pub metrics_public: bool,
    /// How long a frame may wait for an agent that does not read its
    /// session before the session is closed, in milliseconds.
    pub session_send_timeout_ms: u64,
    /// Size of the database connection pool; 4 for SQLite and 16 for
    /// MariaDB when unset.
    pub db_max_connections: Option<u32>,
    /// SQLite durability; ignored with MariaDB.
    pub sqlite_synchronous: SqliteSynchronous,
}

impl fmt::Debug for Config {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Config")
            .field("rest_listen", &self.rest_listen)
            .field("agent_listen", &self.agent_listen)
            .field("peer_listen", &self.peer_listen)
            .field("database_url", &"***")
            .field("replica_id", &self.replica_id)
            .field("peer_advertise_addr", &self.peer_advertise_addr)
            .field("heartbeat_interval_s", &self.heartbeat_interval_s)
            .field("job_default_deadline_s", &self.job_default_deadline_s)
            .field("retention_s", &self.retention_s)
            .field("sweep_interval_ms", &self.sweep_interval_ms)
            .field("body_limit_bytes", &self.body_limit_bytes)
            .field("auto_enroll", &self.auto_enroll)
            .field("thumbprint", &self.thumbprint)
            .field("rest", &self.rest)
            .field("rest_tls", &self.rest_tls)
            .field("agent_tls", &self.agent_tls)
            .field("log_json", &self.log_json)
            .field("peer_token", &redacted(&self.peer_token))
            .field("peer_tls", &self.peer_tls)
            .field("peer_ca", &self.peer_ca)
            .field("centcore", &self.centcore)
            .field("insecure_no_auth", &self.insecure_no_auth)
            .field("allow_plaintext", &self.allow_plaintext)
            .field("metrics_public", &self.metrics_public)
            .field("session_send_timeout_ms", &self.session_send_timeout_ms)
            .field("db_max_connections", &self.db_max_connections)
            .field("sqlite_synchronous", &self.sqlite_synchronous)
            .finish()
    }
}

impl Default for Config {
    fn default() -> Self {
        Self {
            // Loopback by default: exposing a listener is a decision.
            rest_listen: SocketAddr::from(([127, 0, 0, 1], 8085)),
            agent_listen: SocketAddr::from(([127, 0, 0, 1], 5556)),
            peer_listen: SocketAddr::from(([127, 0, 0, 1], 5557)),
            database_url: "sqlite:///var/lib/centreon-gorgone/gorgone.db?mode=rwc".to_owned(),
            replica_id: None,
            peer_advertise_addr: None,
            heartbeat_interval_s: 10,
            job_default_deadline_s: 300,
            retention_s: 7 * 24 * 3600,
            sweep_interval_ms: 2000,
            body_limit_bytes: 256 << 20,
            auto_enroll: false,
            thumbprint: None,
            rest: RestAuthConfig::default(),
            rest_tls: None,
            agent_tls: None,
            log_json: false,
            peer_token: None,
            peer_tls: None,
            peer_ca: None,
            centcore: CentcoreConfig::default(),
            insecure_no_auth: false,
            allow_plaintext: false,
            metrics_public: false,
            session_send_timeout_ms: 5000,
            db_max_connections: None,
            sqlite_synchronous: SqliteSynchronous::Full,
        }
    }
}

/// Value of an environment override: any TOML value (`true`, `42`,
/// `["a", "b"]`, `"quoted"`), else the text itself.
fn env_value(raw: &str) -> Value {
    format!("value = {raw}")
        .parse::<Table>()
        .ok()
        // One value and nothing else; a date stays the text it was.
        .filter(|table| table.len() == 1)
        .and_then(|mut table| table.remove("value"))
        .filter(|value| !value.is_datetime())
        .unwrap_or_else(|| Value::String(raw.to_owned()))
}

/// Sets the key `a.b` of `table` for every `<prefix>A__B=value` of `vars`.
/// The prefix and the keys are matched whatever their case.
fn overlay_env(table: &mut Table, prefix: &str, vars: impl IntoIterator<Item = (String, String)>) {
    // Sorted, so that `A` and `A__B` always combine the same way.
    let mut vars: Vec<(String, String)> = vars
        .into_iter()
        .filter_map(|(name, value)| {
            let head = name.get(..prefix.len())?;
            head.eq_ignore_ascii_case(prefix)
                .then(|| (name[prefix.len()..].to_lowercase(), value))
        })
        .collect();
    vars.sort();
    for (path, raw) in vars {
        let mut keys: Vec<&str> = path.split("__").collect();
        let Some(last) = keys.pop().filter(|last| !last.is_empty()) else {
            continue;
        };
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
                return;
            };
            current = next;
        }
        current.insert(last.to_owned(), env_value(&raw));
    }
}

impl Config {
    /// Loads defaults, then the TOML `file` if given, then `GORGONE_SERVER__…`
    /// environment variables.
    pub fn load(file: Option<&Path>) -> Result<Self, Error> {
        // A variable that is not Unicode is ignored.
        let vars = std::env::vars_os().filter_map(|(name, value)| {
            Some((name.into_string().ok()?, value.into_string().ok()?))
        });
        Self::load_with(file, vars)
    }

    fn load_with(
        file: Option<&Path>,
        vars: impl IntoIterator<Item = (String, String)>,
    ) -> Result<Self, Error> {
        let mut table = Table::new();
        if let Some(file) = file {
            let text =
                std::fs::read_to_string(file).map_err(Error::io(file.display().to_string()))?;
            table = text
                .parse()
                .map_err(|err| Error::Config(format!("{}: {err}", file.display())))?;
        }
        overlay_env(&mut table, ENV_PREFIX, vars);
        // Keys left out take the defaults (`serde(default)`).
        let config: Self = table
            .try_into()
            .map_err(|err: toml::de::Error| Error::Config(err.to_string()))?;
        config.validate_deployment()?;
        Ok(config)
    }

    /// [`Self::validate`], plus the rule applied to a configuration loaded
    /// from a file or the environment: the REST API needs a credential,
    /// unless `insecure_no_auth` says otherwise.
    pub fn validate_deployment(&self) -> Result<(), Error> {
        self.validate()?;
        if !self.rest.requires_credentials() && !self.insecure_no_auth {
            return Err(Error::Invalid(
                "no REST credential: set rest.token (or rest.basic_username and \
                 rest.basic_password), or insecure_no_auth = true to run without"
                    .into(),
            ));
        }
        Ok(())
    }

    /// Checks the constraints the type system does not express, and that no
    /// listener is exposed without protection.
    pub fn validate(&self) -> Result<(), Error> {
        if self.heartbeat_interval_s == 0
            || self.sweep_interval_ms == 0
            || self.session_send_timeout_ms == 0
        {
            return Err(Error::Invalid(
                "heartbeat_interval_s, sweep_interval_ms and session_send_timeout_ms must be \
                 positive"
                    .into(),
            ));
        }
        for (name, listen, tls) in [
            ("rest", self.rest_listen, &self.rest_tls),
            ("agent", self.agent_listen, &self.agent_tls),
            ("peer", self.peer_listen, &self.peer_tls),
        ] {
            if !listen.ip().is_loopback() && tls.is_none() && !self.allow_plaintext {
                return Err(Error::Invalid(format!(
                    "{name}_listen {listen} is not a loopback address: configure {name}_tls, \
                     or allow_plaintext = true"
                )));
            }
        }
        if self.db_max_connections == Some(0) {
            return Err(Error::Invalid("db_max_connections must be positive".into()));
        }
        if !self.peer_listen.ip().is_loopback() && self.peer_token.is_none() {
            return Err(Error::Invalid(format!(
                "peer_listen {} is not a loopback address: peer_token is required",
                self.peer_listen
            )));
        }
        if self.centcore.enabled
            && (self.centcore.poll_interval_ms == 0 || self.centcore.bulk_external_cmd == 0)
        {
            return Err(Error::Invalid(
                "centcore.poll_interval_ms and centcore.bulk_external_cmd must be positive".into(),
            ));
        }
        if self.peer_tls.is_some() && self.peer_ca.is_none() {
            return Err(Error::Invalid(
                "peer_tls needs peer_ca to verify the other replicas".into(),
            ));
        }
        if self.rest.basic_username.is_some() != self.rest.basic_password.is_some() {
            return Err(Error::Invalid(
                "rest.basic_username and rest.basic_password go together".into(),
            ));
        }
        Ok(())
    }

    pub(crate) fn heartbeat_interval(&self) -> Duration {
        Duration::from_secs(u64::from(self.heartbeat_interval_s))
    }

    pub(crate) fn sweep_interval(&self) -> Duration {
        Duration::from_millis(self.sweep_interval_ms)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn example_file_documents_the_defaults() {
        let example = Path::new(env!("CARGO_MANIFEST_DIR")).join("config.example.toml");
        let text = std::fs::read_to_string(&example).unwrap();
        let loaded: Config = toml::from_str(&text).unwrap();
        loaded.validate().unwrap();
        // Every uncommented key of the example is the default value.
        let as_json = |config: &Config| serde_json::to_value(config).unwrap();
        assert_eq!(as_json(&loaded), as_json(&Config::default()));

        // And every key of the configuration appears in the example.
        let defaults = as_json(&Config::default());
        let mut keys: Vec<String> = defaults.as_object().unwrap().keys().cloned().collect();
        for section in ["rest", "centcore"] {
            keys.extend(defaults[section].as_object().unwrap().keys().cloned());
        }
        keys.extend(["cert", "key", "client_ca"].map(str::to_owned));
        for key in keys {
            assert!(
                text.contains(&format!("{key} = ")) || text.contains(&format!("[{key}]")),
                "{key} is not documented in config.example.toml"
            );
        }
    }

    #[test]
    fn inconsistent_settings_are_refused() {
        let mut config = Config::default();
        config.rest.basic_username = Some("web".into());
        assert!(config.validate().is_err());

        let mut config = Config {
            peer_tls: Some(TlsConfig {
                cert: "c".into(),
                key: "k".into(),
                client_ca: None,
            }),
            ..Config::default()
        };
        assert!(config.validate().is_err());
        config.peer_ca = Some("ca".into());
        assert!(config.validate().is_ok());

        let mut config = Config::default();
        config.centcore.enabled = true;
        config.centcore.bulk_external_cmd = 0;
        assert!(config.validate().is_err());
    }

    fn tls() -> Option<TlsConfig> {
        Some(TlsConfig {
            cert: "c".into(),
            key: "k".into(),
            client_ca: None,
        })
    }

    #[test]
    fn deployment_without_rest_credential_is_refused() {
        let config = Config::default();
        // Usable in process (loopback), but not as a deployed configuration.
        config.validate().unwrap();
        let refused = config.validate_deployment().unwrap_err().to_string();
        assert!(refused.contains("insecure_no_auth"), "{refused}");

        let explicit = Config {
            insecure_no_auth: true,
            ..Config::default()
        };
        explicit.validate_deployment().unwrap();
        let mut with_token = Config::default();
        with_token.rest.token = Some("t".into());
        with_token.validate_deployment().unwrap();

        // Same rule through the loader: file only, then file plus a token.
        let dir = tempfile::tempdir().unwrap();
        let file = dir.path().join("gorgone.toml");
        std::fs::write(&file, "sweep_interval_ms = 1000\n").unwrap();
        assert!(Config::load_with(Some(&file), env(&[])).is_err());
        std::fs::write(&file, "sweep_interval_ms = 1000\n[rest]\ntoken = \"t\"\n").unwrap();
        let loaded = Config::load_with(Some(&file), env(&[])).unwrap();
        assert_eq!(loaded.sweep_interval_ms, 1000);
        // What the file leaves out keeps its default.
        assert_eq!(loaded.retention_s, Config::default().retention_s);
        assert_eq!(loaded.centcore.bulk_external_cmd, 50);

        // A missing or malformed file is an error, not an empty configuration.
        let missing = dir.path().join("missing.toml");
        assert!(matches!(
            Config::load_with(Some(&missing), env(&[])),
            Err(Error::Io { .. })
        ));
        std::fs::write(&file, "sweep_interval_ms = \n").unwrap();
        assert!(matches!(
            Config::load_with(Some(&file), env(&[])),
            Err(Error::Config(_))
        ));
    }

    fn env(vars: &[(&str, &str)]) -> Vec<(String, String)> {
        vars.iter()
            .map(|(name, value)| ((*name).to_owned(), (*value).to_owned()))
            .collect()
    }

    #[test]
    fn environment_overrides_the_file() {
        let dir = tempfile::tempdir().unwrap();
        let file = dir.path().join("gorgone.toml");
        std::fs::write(
            &file,
            "sweep_interval_ms = 1000\nretention_s = 60\n[rest]\ntoken = \"file\"\n\
             [centcore]\nenabled = false\nbulk_external_cmd = 7\n",
        )
        .unwrap();
        let vars = env(&[
            ("GORGONE_SERVER__SWEEP_INTERVAL_MS", "250"),
            ("GORGONE_SERVER__REST_LISTEN", "0.0.0.0:8085"),
            ("GORGONE_SERVER__ALLOW_PLAINTEXT", "true"),
            ("GORGONE_SERVER__DATABASE_URL", "mysql://u:p@db/gorgone"),
            ("GORGONE_SERVER__REST__TOKEN", "from-env"),
            (
                "GORGONE_SERVER__REST__ALLOWED_CIDRS",
                r#"["10.0.0.0/8", "::1/128"]"#,
            ),
            ("GORGONE_SERVER__CENTCORE__ENABLED", "true"),
            ("GORGONE_SERVER__SQLITE_SYNCHRONOUS", "normal"),
            ("GORGONE_SERVER__PEER_TOKEN", "\"quoted\""),
            ("gorgone_server__thumbprint", "abc"),
            // Not ours: another prefix, and the variable naming the file.
            ("GORGONE_SERVER_CONFIG", "/nowhere"),
            ("GORGONE_AGENT__RETENTION_S", "1"),
            ("OTHER", "x"),
        ]);
        let config = Config::load_with(Some(&file), vars.clone()).unwrap();
        assert_eq!(config.sweep_interval_ms, 250);
        assert_eq!(config.rest_listen, "0.0.0.0:8085".parse().unwrap());
        assert!(config.allow_plaintext);
        assert_eq!(config.database_url, "mysql://u:p@db/gorgone");
        assert_eq!(config.rest.token.as_deref(), Some("from-env"));
        assert_eq!(config.rest.allowed_cidrs.len(), 2);
        assert!(config.centcore.enabled);
        assert_eq!(config.sqlite_synchronous, SqliteSynchronous::Normal);
        assert_eq!(config.peer_token.as_deref(), Some("quoted"));
        assert_eq!(config.thumbprint.as_deref(), Some("abc"));
        // Untouched by the environment: the file, then the defaults.
        assert_eq!(config.retention_s, 60);
        assert_eq!(config.centcore.bulk_external_cmd, 7);
        assert_eq!(config.heartbeat_interval_s, 10);

        // Without a file the environment is enough.
        let config = Config::load_with(None, vars).unwrap();
        assert_eq!(config.rest.token.as_deref(), Some("from-env"));
        assert_eq!(config.retention_s, Config::default().retention_s);

        // A value of the wrong type is refused, with the key in the message.
        let refused = Config::load_with(None, env(&[("GORGONE_SERVER__RETENTION_S", "soon")]))
            .unwrap_err()
            .to_string();
        assert!(refused.contains("retention_s"), "{refused}");
    }

    #[test]
    fn environment_values_are_toml_or_text() {
        assert_eq!(env_value("true"), Value::Boolean(true));
        assert_eq!(env_value("42"), Value::Integer(42));
        assert_eq!(env_value("1.5"), Value::Float(1.5));
        assert_eq!(env_value("\"42\""), Value::String("42".into()));
        assert_eq!(env_value("plain text"), Value::String("plain text".into()));
        assert_eq!(env_value(""), Value::String(String::new()));
        assert_eq!(
            env_value("0.0.0.0:8085"),
            Value::String("0.0.0.0:8085".into())
        );
        assert_eq!(
            env_value("mysql://u:p@db/gorgone # not a comment"),
            Value::String("mysql://u:p@db/gorgone # not a comment".into())
        );
        assert_eq!(
            env_value("[1, 2]"),
            Value::Array(vec![Value::Integer(1), Value::Integer(2)])
        );
        // Nothing but one value: no way to smuggle another key.
        assert_eq!(
            env_value("1\nother = 2"),
            Value::String("1\nother = 2".into())
        );

        // Nested keys, and a scalar giving way to a table.
        let mut table: Table = "a = 1\n[b]\nc = 1\n".parse().unwrap();
        overlay_env(
            &mut table,
            "P__",
            env(&[
                ("P__A__X", "2"),
                ("P__B__D", "3"),
                ("P__", "9"),
                ("Q__B__C", "5"),
            ]),
        );
        let expected: Table = "[a]\nx = 2\n[b]\nc = 1\nd = 3\n".parse().unwrap();
        assert_eq!(table, expected);
    }

    #[test]
    fn exposed_listeners_need_tls_and_the_peer_a_token() {
        let exposed: SocketAddr = "0.0.0.0:0".parse().unwrap();
        for listener in ["rest", "agent", "peer"] {
            let mut config = Config {
                peer_token: Some("p".into()),
                peer_ca: Some("ca".into()),
                ..Config::default()
            };
            match listener {
                "rest" => config.rest_listen = exposed,
                "agent" => config.agent_listen = exposed,
                _ => config.peer_listen = exposed,
            }
            let refused = config.validate().unwrap_err().to_string();
            assert!(refused.contains(&format!("{listener}_tls")), "{refused}");

            let mut plaintext = config.clone();
            plaintext.allow_plaintext = true;
            plaintext.validate().unwrap();

            match listener {
                "rest" => config.rest_tls = tls(),
                "agent" => config.agent_tls = tls(),
                _ => config.peer_tls = tls(),
            }
            config.validate().unwrap();
        }

        // TLS or not, an exposed peer listener needs the shared secret.
        let config = Config {
            peer_listen: exposed,
            allow_plaintext: true,
            ..Config::default()
        };
        let refused = config.validate().unwrap_err().to_string();
        assert!(refused.contains("peer_token"), "{refused}");
    }

    #[test]
    fn debug_never_shows_secrets() {
        let mut config = Config {
            database_url: "mysql://gorgone:db-secret@db/gorgone".into(),
            peer_token: Some("peer-secret".into()),
            ..Config::default()
        };
        config.rest.token = Some("rest-secret".into());
        config.rest.basic_username = Some("web".into());
        config.rest.basic_password = Some("basic-secret".into());
        let shown = format!("{config:?} {:?} {:#?}", config.rest, config);
        for secret in ["db-secret", "peer-secret", "rest-secret", "basic-secret"] {
            assert!(!shown.contains(secret), "{secret} leaked in {shown}");
        }
        assert!(shown.contains("***"));
        assert!(shown.contains("web"));
    }
}
