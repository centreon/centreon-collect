//! Rotation of the credential chosen at enrolment: the agent replaces it
//! with a new random one on a schedule and when the central asks.
//!
//! Steps, each one safe to interrupt and repeat:
//!
//! 1. the new credential is written next to the current one in the
//!    identity file (`next_credential`);
//! 2. `RotateCredential(new)` presented with the current credential: the
//!    server accepts both from now on;
//! 3. the calls present the new credential, and `RotateCredential(new)` is
//!    sent again with it: the server makes it the only one (and closes the
//!    sessions opened with the former one, the session reopens with it);
//! 4. the identity file keeps the new credential only.
//!
//! An agent restarted with a `next_credential` in its file runs steps 2 to 4
//! again: step 2 fails harmlessly once the server dropped the former
//! credential, and step 3 succeeds whether the server had recorded the new
//! one, promoted it, or not heard of it yet.

use std::path::PathBuf;
use std::sync::{Arc, Mutex, PoisonError};
use std::time::{Duration, SystemTime};

use gorgone_proto::agent::RotateCredentialRequest;
use tokio::sync::Notify;
use tokio_util::sync::CancellationToken;

use crate::client::{Bearer, GatewayClient};
use crate::enrolment::{IDENTITY_FILE, Identity, new_credential};
use crate::session::Backoff;

/// Retries of an interrupted rotation (server away, store failure).
const RETRY_MIN: Duration = Duration::from_secs(1);
const RETRY_MAX: Duration = Duration::from_secs(60);

/// Outcome of one attempt to finish a rotation.
#[derive(Debug, PartialEq, Eq)]
enum Attempt {
    Done,
    /// To try again later; the reason is logged.
    Retry,
}

/// Rotates the credential kept in the state directory.
pub(crate) struct Rotation {
    dir: PathBuf,
    identity: Mutex<Identity>,
    bearer: Arc<Bearer>,
    client: GatewayClient,
    /// Time between two scheduled rotations; `None`: on request only.
    period: Option<Duration>,
    requested: Notify,
}

impl Rotation {
    pub(crate) fn new(
        dir: PathBuf,
        identity: Identity,
        bearer: Arc<Bearer>,
        client: GatewayClient,
        period: Option<Duration>,
    ) -> Self {
        Self {
            dir,
            identity: Mutex::new(identity),
            bearer,
            client,
            period,
            requested: Notify::new(),
        }
    }

    /// Asks for a rotation now (the central asked for it).
    pub(crate) fn request(&self) {
        self.requested.notify_one();
    }

    fn identity(&self) -> Identity {
        self.identity
            .lock()
            .unwrap_or_else(PoisonError::into_inner)
            .clone()
    }

    fn keep(&self, identity: Identity) -> std::io::Result<()> {
        identity.store(&self.dir)?;
        *self.identity.lock().unwrap_or_else(PoisonError::into_inner) = identity;
        Ok(())
    }

    /// Time left before the scheduled rotation: the credential changed when
    /// the identity file was last written.
    fn until_due(&self) -> Option<Duration> {
        let period = self.period?;
        let changed = std::fs::metadata(self.dir.join(IDENTITY_FILE))
            .and_then(|meta| meta.modified())
            .unwrap_or_else(|_| SystemTime::now());
        let age = SystemTime::now()
            .duration_since(changed)
            .unwrap_or_default();
        Some(period.saturating_sub(age))
    }

    /// Finishes a rotation left by a former run, then rotates on schedule
    /// and on request until `stop`.
    pub(crate) async fn run(self: Arc<Self>, stop: CancellationToken) {
        loop {
            if self.identity().next_credential.is_some() {
                let mut retries = Backoff::new(RETRY_MIN, RETRY_MAX);
                while self.finish().await == Attempt::Retry {
                    tokio::select! {
                        () = tokio::time::sleep(retries.next_delay()) => {}
                        () = stop.cancelled() => return,
                    }
                }
            }
            let due = async {
                match self.until_due() {
                    Some(left) => tokio::time::sleep(left).await,
                    None => std::future::pending().await,
                }
            };
            tokio::select! {
                () = stop.cancelled() => return,
                () = self.requested.notified() => {
                    tracing::info!("credential rotation requested by the central");
                }
                () = due => tracing::info!("credential rotation due"),
            }
            if let Err(error) = self.begin() {
                tracing::error!(%error, "credential rotation not started: state directory unwritable");
                tokio::select! {
                    () = tokio::time::sleep(RETRY_MAX) => {}
                    () = stop.cancelled() => return,
                }
            }
        }
    }

    /// Step 1: the new credential is kept before anyone hears of it.
    fn begin(&self) -> std::io::Result<()> {
        let mut identity = self.identity();
        if identity.next_credential.is_none() {
            identity.next_credential = Some(new_credential()?);
            self.keep(identity)?;
        }
        Ok(())
    }

    /// Steps 2 to 4.
    async fn finish(&self) -> Attempt {
        let identity = self.identity();
        let Some(next) = identity.next_credential.clone() else {
            return Attempt::Done;
        };
        let request = RotateCredentialRequest {
            new_credential: next.clone(),
        };
        let mut client = self.client.clone();
        // Step 2, with the current credential. Refused when the server
        // already made the new one the only one: step 3 tells.
        if let Err(status) = client.rotate_credential(request.clone()).await
            && status.code() != tonic::Code::Unauthenticated
        {
            tracing::warn!(code = ?status.code(), error = %status.message(),
                "credential rotation: new credential not recorded, retried later");
            return Attempt::Retry;
        }
        // Step 3: from now on every call, the sessions included, presents
        // the new credential.
        if let Err(error) = self.bearer.set(&next) {
            tracing::error!(%error, "credential rotation: new credential unusable");
            return Attempt::Retry;
        }
        match client.rotate_credential(request).await {
            Ok(answer) if answer.get_ref().done => {}
            Ok(_) => {
                tracing::warn!("credential rotation: the server did not switch, retried later");
                return self.back_to(&identity);
            }
            Err(status) if status.code() == tonic::Code::Unauthenticated => {
                tracing::error!(error = %status.message(),
                    "credential rotation: neither credential accepted (node deleted?), retried later");
                return self.back_to(&identity);
            }
            Err(status) => {
                // The server may have switched: the next session, which
                // presents the new credential, finishes it.
                tracing::warn!(code = ?status.code(), error = %status.message(),
                    "credential rotation: no confirmation, retried later");
                return Attempt::Retry;
            }
        }
        // Step 4.
        let rotated = Identity {
            credential: next,
            next_credential: None,
            ..identity
        };
        if let Err(error) = self.keep(rotated) {
            // The file still holds both: the next start finishes it.
            tracing::error!(%error, "credential rotation: identity file not updated");
            return Attempt::Retry;
        }
        tracing::info!(node_id = %self.identity().node_id, "credential rotated");
        Attempt::Done
    }

    /// Presents the current credential again.
    fn back_to(&self, identity: &Identity) -> Attempt {
        if let Err(error) = self.bearer.set(&identity.credential) {
            tracing::error!(%error, "credential rotation: current credential unusable");
        }
        Attempt::Retry
    }
}
