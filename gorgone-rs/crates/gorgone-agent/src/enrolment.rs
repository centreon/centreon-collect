//! Enrolment of a new node: trades the enrolment token given by the central
//! for a node id, bound to a credential this agent generates and keeps in
//! its state directory. Later starts read the identity back and ignore the
//! token.

use std::fs::{self, File, OpenOptions};
use std::io::{self, Read, Write};
use std::os::unix::fs::OpenOptionsExt;
use std::path::{Path, PathBuf};
use std::time::Duration;

use gorgone_proto::agent::EnrollRequest;
use gorgone_proto::agent::enroll_response::State;

use crate::config::{AgentConfig, Secret};
use crate::session::Backoff;
use crate::{AgentError, client};

/// File of `state_dir` holding the node identity: `node_id=` and
/// `credential=` lines, mode 0600. `node_id` is empty while a fleet
/// enrolment waits for the central.
pub(crate) const IDENTITY_FILE: &str = "identity";
/// Random bytes of a credential, sent in hexadecimal.
const CREDENTIAL_BYTES: usize = 32;
/// Period of the questions while the central has not assigned a node.
const PENDING_MIN: Duration = Duration::from_secs(2);
const PENDING_MAX: Duration = Duration::from_secs(10);

/// Identity of the node, as kept on disk.
#[derive(Debug, Clone, PartialEq, Eq)]
pub(crate) struct Identity {
    pub(crate) node_id: String,
    pub(crate) credential: String,
}

impl Identity {
    fn parse(text: &str) -> Option<Self> {
        let mut node_id = None;
        let mut credential = None;
        for line in text.lines() {
            match line.split_once('=') {
                Some(("node_id", value)) => node_id = Some(value.trim().to_owned()),
                Some(("credential", value)) => credential = Some(value.trim().to_owned()),
                _ => {}
            }
        }
        let credential = credential.filter(|c| !c.is_empty())?;
        Some(Self {
            node_id: node_id.unwrap_or_default(),
            credential,
        })
    }

    /// Reads the identity of `dir`; `None` when there is none yet.
    pub(crate) fn load(dir: &Path) -> io::Result<Option<Self>> {
        let path = dir.join(IDENTITY_FILE);
        match fs::read_to_string(&path) {
            Ok(text) => Self::parse(&text).map(Some).ok_or_else(|| {
                io::Error::other(format!("{}: unreadable identity", path.display()))
            }),
            Err(error) if error.kind() == io::ErrorKind::NotFound => Ok(None),
            Err(error) => Err(error),
        }
    }

    /// Writes the identity atomically, readable by the agent user only.
    pub(crate) fn store(&self, dir: &Path) -> io::Result<()> {
        fs::create_dir_all(dir)?;
        let path = dir.join(IDENTITY_FILE);
        let staged = dir.join(format!(".{IDENTITY_FILE}.tmp"));
        let mut file = OpenOptions::new()
            .write(true)
            .create(true)
            .truncate(true)
            .mode(0o600)
            .open(&staged)?;
        write!(
            file,
            "node_id={}\ncredential={}\n",
            self.node_id, self.credential
        )?;
        file.sync_all()?;
        drop(file);
        fs::rename(&staged, &path)?;
        // The rename itself survives a power loss once the directory is synced.
        File::open(dir)?.sync_all()
    }
}

/// A new random credential, from the kernel generator.
fn new_credential() -> io::Result<String> {
    let mut bytes = [0u8; CREDENTIAL_BYTES];
    File::open("/dev/urandom")?.read_exact(&mut bytes)?;
    Ok(bytes.iter().map(|byte| format!("{byte:02x}")).collect())
}

/// Host name of the machine, for the name and address reported at enrolment.
fn host_name() -> String {
    fs::read_to_string("/proc/sys/kernel/hostname")
        .ok()
        .map(|name| name.trim().to_owned())
        .filter(|name| !name.is_empty())
        .or_else(|| {
            std::env::var("HOSTNAME")
                .ok()
                .filter(|name| !name.is_empty())
        })
        .unwrap_or_else(|| "unnamed-node".to_owned())
}

fn io_error(path: PathBuf) -> impl FnOnce(io::Error) -> AgentError {
    move |source| AgentError::Io { path, source }
}

/// Gives `config` its node identity: the one kept in `state_dir`, else the
/// one obtained with `enrolment_token` (retried until it succeeds). Without
/// `state_dir` or token, the configuration is used as it is.
pub(crate) async fn resolve(mut config: AgentConfig) -> Result<AgentConfig, AgentError> {
    let Some(dir) = config.state_dir.clone() else {
        return Ok(config);
    };
    let kept = Identity::load(&dir).map_err(io_error(dir.join(IDENTITY_FILE)))?;
    if let Some(identity) = &kept
        && !identity.node_id.is_empty()
    {
        if !config.node_id.is_empty() && config.node_id != identity.node_id {
            return Err(AgentError::Config(crate::ConfigError::Invalid(format!(
                "node_id {} differs from the node {} this agent enrolled as ({})",
                config.node_id,
                identity.node_id,
                dir.join(IDENTITY_FILE).display()
            ))));
        }
        tracing::info!(node_id = %identity.node_id, "using the identity kept in the state directory");
        config.node_id.clone_from(&identity.node_id);
        config.token = Some(Secret::new(identity.credential.clone()));
        return Ok(config);
    }
    let Some(token) = config.enrolment_token.clone() else {
        return Ok(config);
    };
    // Kept before the first attempt: a restart while the central has not
    // assigned a node asks again with the same credential.
    let credential = match kept {
        Some(identity) => identity.credential,
        None => {
            let identity = Identity {
                node_id: String::new(),
                credential: new_credential().map_err(io_error(PathBuf::from("/dev/urandom")))?,
            };
            identity
                .store(&dir)
                .map_err(io_error(dir.join(IDENTITY_FILE)))?;
            identity.credential
        }
    };
    let node_id = enrol(&config, token.expose(), &credential).await?;
    Identity {
        node_id: node_id.clone(),
        credential: credential.clone(),
    }
    .store(&dir)
    .map_err(io_error(dir.join(IDENTITY_FILE)))?;
    tracing::info!(%node_id, "enrolled: node identity kept in the state directory");
    config.node_id = node_id;
    config.token = Some(Secret::new(credential));
    Ok(config)
}

/// Asks the server until it gives a node id. Never gives up: the token may
/// not be pushed yet, the server may be down, the central may not have
/// assigned a node yet.
async fn enrol(config: &AgentConfig, token: &str, credential: &str) -> Result<String, AgentError> {
    let mut client = client::connect_anonymous(config)?;
    let host = host_name();
    let request = EnrollRequest {
        token: token.to_owned(),
        credential: credential.to_owned(),
        name: config.name.clone().unwrap_or_else(|| host.clone()),
        address: config.address.clone().unwrap_or(host),
        agent_version: env!("CARGO_PKG_VERSION").to_owned(),
        component: config.component.clone(),
    };
    let mut failures = Backoff::new(
        Duration::from_millis(config.reconnect.min_ms),
        Duration::from_millis(config.reconnect.max_ms),
    );
    let mut pending = Backoff::new(PENDING_MIN, PENDING_MAX);
    tracing::info!(server_url = %config.server_url, name = %request.name, "enrolling with the enrolment token");
    loop {
        let delay = match client.enroll(request.clone()).await {
            Ok(answer) => {
                let answer = answer.into_inner();
                match answer.state() {
                    State::Enrolled if !answer.node_id.is_empty() => return Ok(answer.node_id),
                    State::Pending => {
                        failures.reset();
                        let delay = pending.next_delay();
                        tracing::info!(enrolment_id = %answer.enrolment_id, retry_in = ?delay,
                            "enrolment accepted, waiting for the central to assign a node");
                        delay
                    }
                    _ => {
                        let delay = failures.next_delay();
                        tracing::warn!(retry_in = ?delay, "unexpected enrolment answer");
                        delay
                    }
                }
            }
            Err(status) => {
                let delay = failures.next_delay();
                tracing::error!(code = ?status.code(), error = %status.message(), retry_in = ?delay,
                    "enrolment failed: check server_url and the enrolment token (unknown, expired \
                     or already used; a new token may not be pushed yet)");
                delay
            }
        };
        tokio::time::sleep(delay).await;
    }
}

#[cfg(test)]
mod tests {
    use std::os::unix::fs::PermissionsExt;

    use super::*;

    #[test]
    fn identity_round_trip_is_private() {
        let dir = tempfile::tempdir().unwrap();
        assert_eq!(Identity::load(dir.path()).unwrap(), None);
        let identity = Identity {
            node_id: "12".into(),
            credential: new_credential().unwrap(),
        };
        assert_eq!(identity.credential.len(), 2 * CREDENTIAL_BYTES);
        identity.store(dir.path()).unwrap();
        assert_eq!(Identity::load(dir.path()).unwrap(), Some(identity));
        let mode = fs::metadata(dir.path().join(IDENTITY_FILE))
            .unwrap()
            .permissions()
            .mode();
        assert_eq!(mode & 0o777, 0o600);
        assert!(!dir.path().join(".identity.tmp").exists());
        // Pending: no node id yet.
        let pending = Identity::parse("node_id=\ncredential=abc\n").unwrap();
        assert!(pending.node_id.is_empty());
        assert_eq!(Identity::parse("node_id=1\n"), None);
    }

    #[test]
    fn credentials_differ() {
        assert_ne!(new_credential().unwrap(), new_credential().unwrap());
    }
}
