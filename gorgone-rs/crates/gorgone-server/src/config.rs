//! Server configuration: one TOML file, every key overridable by environment.

use std::net::SocketAddr;
use std::path::{Path, PathBuf};
use std::time::Duration;

use figment::Figment;
use figment::providers::{Env, Format, Serialized, Toml};
use serde::{Deserialize, Serialize};

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

/// REST API access control.
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
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
    pub allowed_cidrs: Vec<ipnet::IpNet>,
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
        }
    }
}

/// Settings of one `gorgone-server` replica.
#[derive(Debug, Clone, Serialize, Deserialize)]
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
}

impl Default for Config {
    fn default() -> Self {
        Self {
            rest_listen: SocketAddr::from(([0, 0, 0, 0], 8085)),
            agent_listen: SocketAddr::from(([0, 0, 0, 0], 5556)),
            peer_listen: SocketAddr::from(([0, 0, 0, 0], 5557)),
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
        }
    }
}

impl Config {
    /// Loads defaults, then the TOML `file` if given, then `GORGONE_SERVER__…`
    /// environment variables.
    pub fn load(file: Option<&Path>) -> Result<Self, Error> {
        let mut figment = Figment::from(Serialized::defaults(Self::default()));
        if let Some(file) = file {
            figment = figment.merge(Toml::file_exact(file));
        }
        let config: Self = figment
            .merge(Env::prefixed(ENV_PREFIX).split("__"))
            .extract()
            .map_err(Box::new)?;
        config.validate()?;
        Ok(config)
    }

    /// Checks the constraints the type system does not express.
    pub fn validate(&self) -> Result<(), Error> {
        if self.heartbeat_interval_s == 0 || self.sweep_interval_ms == 0 {
            return Err(Error::Invalid(
                "heartbeat_interval_s and sweep_interval_ms must be positive".into(),
            ));
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
        let loaded: Config = Figment::from(Serialized::defaults(Config::default()))
            .merge(Toml::file_exact(&example))
            .extract()
            .unwrap();
        loaded.validate().unwrap();
        // Every uncommented key of the example is the default value.
        let as_json = |config: &Config| serde_json::to_value(config).unwrap();
        assert_eq!(as_json(&loaded), as_json(&Config::default()));

        // And every key of the configuration appears in the example.
        let text = std::fs::read_to_string(&example).unwrap();
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
}
