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

/// File of `state_dir` holding the node identity: `node_id=`,
/// `credential=` and, during a rotation, `next_credential=` lines, mode
/// 0600. `node_id` is empty while a fleet enrolment waits for the central.
/// Its modification time is when the credential last changed.
pub(crate) const IDENTITY_FILE: &str = "identity";
/// Random bytes of a credential, sent in hexadecimal.
const CREDENTIAL_BYTES: usize = 32;
/// Period of the questions while the central has not assigned a node.
const PENDING_MIN: Duration = Duration::from_secs(2);
const PENDING_MAX: Duration = Duration::from_secs(10);
/// Period of the questions once the enrolment was rejected: nothing will
/// change for this token, but asking rarely keeps the agent alive (no
/// restart loop under a supervisor) and shows in its log.
const REJECTED_RETRY: Duration = Duration::from_secs(3600);

/// Identity of the node, as kept on disk.
#[derive(Debug, Clone, PartialEq, Eq)]
pub(crate) struct Identity {
    pub(crate) node_id: String,
    pub(crate) credential: String,
    /// Credential being rotated to: written before the server hears of it,
    /// promoted to `credential` once the server made it the only one.
    pub(crate) next_credential: Option<String>,
}

impl Identity {
    fn parse(text: &str) -> Option<Self> {
        let mut node_id = None;
        let mut credential = None;
        let mut next_credential = None;
        for line in text.lines() {
            match line.split_once('=') {
                Some(("node_id", value)) => node_id = Some(value.trim().to_owned()),
                Some(("credential", value)) => credential = Some(value.trim().to_owned()),
                Some(("next_credential", value)) => {
                    next_credential = Some(value.trim().to_owned());
                }
                _ => {}
            }
        }
        let credential = credential.filter(|c| !c.is_empty())?;
        Some(Self {
            node_id: node_id.unwrap_or_default(),
            credential,
            next_credential: next_credential.filter(|c| !c.is_empty()),
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
        if let Some(next) = &self.next_credential {
            writeln!(file, "next_credential={next}")?;
        }
        file.sync_all()?;
        drop(file);
        fs::rename(&staged, &path)?;
        // The rename itself survives a power loss once the directory is synced.
        File::open(dir)?.sync_all()
    }
}

/// A new random credential, from the kernel generator.
pub(crate) fn new_credential() -> io::Result<String> {
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
/// `state_dir` or token, the configuration is used as it is. Also returns
/// the identity kept in `state_dir`, whose credential this agent rotates.
pub(crate) async fn resolve(
    mut config: AgentConfig,
) -> Result<(AgentConfig, Option<Identity>), AgentError> {
    let Some(dir) = config.state_dir.clone() else {
        return Ok((config, None));
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
        return Ok((config, kept));
    }
    let Some(token) = config.enrolment_token.clone() else {
        return Ok((config, None));
    };
    // Kept before the first attempt: a restart while the central has not
    // assigned a node asks again with the same credential.
    let credential = match kept {
        Some(identity) => identity.credential,
        None => {
            let identity = Identity {
                node_id: String::new(),
                credential: new_credential().map_err(io_error(PathBuf::from("/dev/urandom")))?,
                next_credential: None,
            };
            identity
                .store(&dir)
                .map_err(io_error(dir.join(IDENTITY_FILE)))?;
            identity.credential
        }
    };
    let node_id = enrol(&config, token.expose(), &credential).await?;
    let identity = Identity {
        node_id: node_id.clone(),
        credential: credential.clone(),
        next_credential: None,
    };
    identity
        .store(&dir)
        .map_err(io_error(dir.join(IDENTITY_FILE)))?;
    tracing::info!(%node_id, "enrolled: node identity kept in the state directory");
    config.node_id = node_id;
    config.token = Some(Secret::new(credential));
    Ok((config, Some(identity)))
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
                    State::Rejected => {
                        tracing::error!(enrolment_id = %answer.enrolment_id, retry_in = ?REJECTED_RETRY,
                            "enrolment rejected by the central: no node will be assigned with \
                             this token; ask its administrator for another one");
                        REJECTED_RETRY
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
                tracing::error!(server_url = %config.server_url, code = ?status.code(),
                    error = %status.message(), retry_in = ?delay,
                    "enrolment failed: {}", failure_hint(status.code()));
                delay
            }
        };
        tokio::time::sleep(delay).await;
    }
}

/// What to check after a failed `Enroll` call: an unreachable server and a
/// refused token need different actions from the admin.
fn failure_hint(code: tonic::Code) -> &'static str {
    match code {
        tonic::Code::Unavailable | tonic::Code::DeadlineExceeded => {
            "gorgone-server cannot be reached: check server_url (name resolution, network, port, \
             TLS)"
        }
        tonic::Code::PermissionDenied | tonic::Code::Unauthenticated => {
            "the enrolment token is refused: unknown, expired or already used (a token just \
             created may not be pushed to gorgone-server yet)"
        }
        tonic::Code::ResourceExhausted => {
            "too many refused attempts from this address: gorgone-server slows them down"
        }
        _ => "unexpected answer from gorgone-server",
    }
}

#[cfg(test)]
mod tests {
    use std::os::unix::fs::PermissionsExt;

    use super::*;

    #[test]
    fn failure_hint_tells_network_from_token() {
        assert!(failure_hint(tonic::Code::Unavailable).contains("cannot be reached"));
        assert!(failure_hint(tonic::Code::DeadlineExceeded).contains("cannot be reached"));
        assert!(failure_hint(tonic::Code::PermissionDenied).contains("token is refused"));
        assert!(!failure_hint(tonic::Code::Unavailable).contains("token"));
        assert!(failure_hint(tonic::Code::ResourceExhausted).contains("too many"));
    }

    #[test]
    fn identity_round_trip_is_private() {
        let dir = tempfile::tempdir().unwrap();
        assert_eq!(Identity::load(dir.path()).unwrap(), None);
        let identity = Identity {
            node_id: "12".into(),
            credential: new_credential().unwrap(),
            next_credential: None,
        };
        assert_eq!(identity.credential.len(), 2 * CREDENTIAL_BYTES);
        identity.store(dir.path()).unwrap();
        assert_eq!(Identity::load(dir.path()).unwrap(), Some(identity.clone()));
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
        // During a rotation both credentials are kept.
        let rotating = Identity {
            next_credential: Some(new_credential().unwrap()),
            ..identity
        };
        rotating.store(dir.path()).unwrap();
        assert_eq!(Identity::load(dir.path()).unwrap(), Some(rotating));
    }

    #[test]
    fn credentials_differ() {
        assert_ne!(new_credential().unwrap(), new_credential().unwrap());
    }
}
