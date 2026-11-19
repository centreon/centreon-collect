//! Enrolment of new nodes: tokens given by the central, and the enrolments
//! made with them; rotation of the credential of an enrolled node. Only
//! hashes of the tokens and of the node credentials are stored.

use sqlx::Row;
use sqlx::any::AnyRow;

use super::{Store, hash_token, opt_text, with_retry};
use crate::error::Error;

/// States of an enrolment.
pub(crate) mod enrolment_state {
    /// Fleet token: waits for the central to assign a node (and, when the
    /// token asks for it, for an administrator to approve it first).
    pub(crate) const PENDING: &str = "pending";
    /// Approved by an administrator: the central may assign a node.
    pub(crate) const APPROVED: &str = "approved";
    /// Bound to a node, whose sessions present the credential.
    pub(crate) const ENROLLED: &str = "enrolled";
    /// The central finished setting the node up.
    pub(crate) const COMPLETED: &str = "completed";
    /// Refused by the central before any node was assigned: the agent is
    /// told so, and asks again rarely.
    pub(crate) const REJECTED: &str = "rejected";
    /// Its node was deleted: the credential is refused.
    pub(crate) const REVOKED: &str = "revoked";
}

/// Shortest credential accepted from an agent.
pub(crate) const MIN_CREDENTIAL_LEN: usize = 32;
/// Longest credential accepted from an agent.
pub(crate) const MAX_CREDENTIAL_LEN: usize = 255;

/// Enrolment token, without its hash.
#[derive(Debug, Clone)]
pub(crate) struct EnrolmentTokenRow {
    pub(crate) id: String,
    /// Node the token enrols; `None` for a fleet token.
    pub(crate) node_id: Option<String>,
    pub(crate) name: Option<String>,
    pub(crate) expires_at: i64,
    pub(crate) max_uses: Option<i64>,
    pub(crate) uses: i64,
    pub(crate) created_at: i64,
    /// Fleet token whose enrolments wait for an administrator's approval.
    pub(crate) approval_required: bool,
    /// Last enrolment counted against the token.
    pub(crate) last_used_at: Option<i64>,
}

/// A token to record.
#[derive(Debug, Clone, Copy)]
pub(crate) struct NewEnrolmentToken<'a> {
    pub(crate) id: &'a str,
    /// Lowercase hex SHA-256 of the token.
    pub(crate) token_hash: &'a str,
    pub(crate) node_id: Option<&'a str>,
    pub(crate) name: Option<&'a str>,
    pub(crate) expires_at: i64,
    pub(crate) max_uses: Option<i64>,
    /// Fleet tokens only: enrolments wait for an approval.
    pub(crate) approval_required: bool,
}

/// One enrolment of a node with a token.
#[derive(Debug, Clone)]
pub(crate) struct EnrolmentRow {
    pub(crate) id: String,
    pub(crate) token_id: String,
    pub(crate) state: String,
    pub(crate) node_id: Option<String>,
    pub(crate) name: String,
    pub(crate) address: String,
    pub(crate) agent_version: String,
    pub(crate) component: String,
    pub(crate) created_at: i64,
    pub(crate) updated_at: i64,
    /// Made with a fleet token (the central chooses the node).
    pub(crate) fleet: bool,
    /// Waits for an approval before the central may assign a node.
    pub(crate) approval_required: bool,
}

/// What an agent sends to enrol, secrets in clear (never stored).
#[derive(Debug, Clone, Copy)]
pub(crate) struct EnrolRequest<'a> {
    pub(crate) token: &'a str,
    pub(crate) credential: &'a str,
    pub(crate) name: &'a str,
    pub(crate) address: &'a str,
    pub(crate) agent_version: &'a str,
    pub(crate) component: &'a str,
}

/// Answer to an enrolment.
#[derive(Debug, Clone, PartialEq, Eq)]
pub(crate) enum EnrolOutcome {
    /// The node identity to open sessions with.
    Enrolled {
        enrolment_id: String,
        node_id: String,
    },
    /// Waits for the central to assign a node.
    Pending { enrolment_id: String },
    /// Rejected by an administrator. Only the holder of the token and of
    /// the credential of this enrolment learns it.
    Rejected { enrolment_id: String },
    /// Refused, with the reason kept for the server logs.
    Refused(&'static str),
}

/// Answer to the assignment of a node to a pending enrolment.
#[derive(Debug, Clone, PartialEq, Eq)]
pub(crate) enum AssignOutcome {
    Assigned,
    NotFound,
    Conflict(&'static str),
}

/// Answer to a new credential an agent proposes, or to a rotation request.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub(crate) enum NextCredential {
    /// Recorded: both credentials are accepted until the new one is used.
    Recorded,
    /// It is already the credential of the node (rotation finished).
    Current,
    /// The node has no credential (auto-enrolled): nothing to rotate.
    NoCredential,
    UnknownNode,
}

/// Credentials of a node as stored (hashes).
#[derive(Debug, Clone, PartialEq, Eq)]
pub(crate) struct NodeCredentials {
    /// Empty for a node accepted by `auto_enroll`.
    pub(crate) token_hash: String,
    /// Credential proposed by the agent, not presented yet.
    pub(crate) next_token_hash: Option<String>,
    /// The agent is asked to rotate at its next session.
    pub(crate) rotation_requested: bool,
}

const TOKEN_COLUMNS: &str =
    "id, node_id, name, expires_at, max_uses, uses, created_at, approval_required, last_used_at";
const ENROLMENT_COLUMNS: &str = "id, token_id, state, node_id, name, address, agent_version, \
     component, created_at, updated_at, fleet, approval_required";

/// States in which an enrolment still waits for its node: its token is kept.
const WAITING: [&str; 2] = [enrolment_state::PENDING, enrolment_state::APPROVED];

fn token_from_row(row: &AnyRow) -> Result<EnrolmentTokenRow, sqlx::Error> {
    Ok(EnrolmentTokenRow {
        id: row.try_get("id")?,
        node_id: opt_text(row, "node_id")?,
        name: opt_text(row, "name")?,
        expires_at: row.try_get("expires_at")?,
        max_uses: row.try_get("max_uses")?,
        uses: row.try_get("uses")?,
        created_at: row.try_get("created_at")?,
        approval_required: row.try_get::<i64, _>("approval_required")? != 0,
        last_used_at: row.try_get("last_used_at")?,
    })
}

fn enrolment_from_row(row: &AnyRow) -> Result<EnrolmentRow, sqlx::Error> {
    Ok(EnrolmentRow {
        id: row.try_get("id")?,
        token_id: row.try_get("token_id")?,
        state: row.try_get("state")?,
        node_id: opt_text(row, "node_id")?,
        name: row.try_get("name")?,
        address: row.try_get("address")?,
        agent_version: row.try_get("agent_version")?,
        component: row.try_get("component")?,
        created_at: row.try_get("created_at")?,
        updated_at: row.try_get("updated_at")?,
        fleet: row.try_get::<i64, _>("fleet")? != 0,
        approval_required: row.try_get::<i64, _>("approval_required")? != 0,
    })
}

/// Outcome of an enrolment already recorded for the same token and credential.
fn replay(enrolment: &EnrolmentRow) -> EnrolOutcome {
    match (enrolment.state.as_str(), &enrolment.node_id) {
        (enrolment_state::PENDING | enrolment_state::APPROVED, _) => EnrolOutcome::Pending {
            enrolment_id: enrolment.id.clone(),
        },
        (enrolment_state::REJECTED, _) => EnrolOutcome::Rejected {
            enrolment_id: enrolment.id.clone(),
        },
        (enrolment_state::ENROLLED | enrolment_state::COMPLETED, Some(node_id)) => {
            EnrolOutcome::Enrolled {
                enrolment_id: enrolment.id.clone(),
                node_id: node_id.clone(),
            }
        }
        _ => EnrolOutcome::Refused("enrolment revoked"),
    }
}

/// Keeps at most `max` bytes of a text an agent reported.
fn cut(text: &str, max: usize) -> &str {
    let mut end = text.len().min(max);
    while !text.is_char_boundary(end) {
        end -= 1;
    }
    &text[..end]
}

impl Store {
    /// Records an enrolment token. Recording the same id and hash again
    /// changes nothing; another hash under a known id, or a hash known under
    /// another id, is refused. A token bound to a node is single use.
    pub(crate) async fn put_enrolment_token(
        &self,
        token: NewEnrolmentToken<'_>,
    ) -> Result<bool, Error> {
        let max_uses = if token.node_id.is_some() {
            Some(1)
        } else {
            token.max_uses
        };
        // A bound token names its node: there is nothing to approve.
        let approval_required = token.approval_required && token.node_id.is_none();
        let sql = format!(
            "{} INTO enrolment_tokens
             (id, token_hash, node_id, name, expires_at, max_uses, uses, created_at,
              approval_required)
             VALUES (?, ?, ?, ?, ?, ?, 0, ?, ?)",
            self.insert_ignore()
        );
        let inserted = sqlx::query(&sql)
            .bind(token.id)
            .bind(token.token_hash)
            .bind(token.node_id)
            .bind(token.name)
            .bind(token.expires_at)
            .bind(max_uses)
            .bind(self.now_ms())
            .bind(i64::from(approval_required))
            .execute(&self.pool)
            .await?
            .rows_affected();
        if inserted > 0 {
            return Ok(true);
        }
        let stored = sqlx::query("SELECT token_hash FROM enrolment_tokens WHERE id = ?")
            .bind(token.id)
            .fetch_optional(&self.pool)
            .await?;
        match stored {
            Some(row) if row.try_get::<String, _>("token_hash")? == token.token_hash => Ok(false),
            Some(_) => Err(Error::Invalid(
                "an enrolment token with this id already exists".into(),
            )),
            None => Err(Error::Invalid("this token is already registered".into())),
        }
    }

    /// Enrolment tokens, without their hash.
    pub(crate) async fn list_enrolment_tokens(&self) -> Result<Vec<EnrolmentTokenRow>, Error> {
        let sql = format!("SELECT {TOKEN_COLUMNS} FROM enrolment_tokens ORDER BY created_at, id");
        let rows = sqlx::query(&sql).fetch_all(&self.pool).await?;
        Ok(rows.iter().map(token_from_row).collect::<Result<_, _>>()?)
    }

    /// Removes a token: it enrols nothing more. The enrolments made with it stay.
    pub(crate) async fn delete_enrolment_token(&self, id: &str) -> Result<bool, Error> {
        let done = sqlx::query("DELETE FROM enrolment_tokens WHERE id = ?")
            .bind(id)
            .execute(&self.pool)
            .await?;
        Ok(done.rows_affected() > 0)
    }

    /// Every enrolment, oldest first.
    pub(crate) async fn list_enrolments(&self) -> Result<Vec<EnrolmentRow>, Error> {
        let sql = format!("SELECT {ENROLMENT_COLUMNS} FROM enrolments ORDER BY created_at, id");
        let rows = sqlx::query(&sql).fetch_all(&self.pool).await?;
        Ok(rows
            .iter()
            .map(enrolment_from_row)
            .collect::<Result<_, _>>()?)
    }

    /// One enrolment.
    pub(crate) async fn enrolment(&self, id: &str) -> Result<Option<EnrolmentRow>, Error> {
        let sql = format!("SELECT {ENROLMENT_COLUMNS} FROM enrolments WHERE id = ?");
        let row = sqlx::query(&sql)
            .bind(id)
            .fetch_optional(&self.pool)
            .await?;
        Ok(row.as_ref().map(enrolment_from_row).transpose()?)
    }

    async fn enrolment_of(
        &self,
        token_id: &str,
        credential_hash: &str,
    ) -> Result<Option<EnrolmentRow>, sqlx::Error> {
        let sql = format!(
            "SELECT {ENROLMENT_COLUMNS} FROM enrolments WHERE token_id = ? AND credential_hash = ?"
        );
        let row = sqlx::query(&sql)
            .bind(token_id)
            .bind(credential_hash)
            .fetch_optional(&self.pool)
            .await?;
        row.as_ref().map(enrolment_from_row).transpose()
    }

    /// Trades a token for a node identity bound to the agent's credential.
    /// The same token and credential again give the same answer, so an
    /// agent retries safely. Uses are counted once per credential.
    pub(crate) async fn enrol(&self, request: EnrolRequest<'_>) -> Result<EnrolOutcome, Error> {
        let token_hash = hash_token(request.token);
        let credential_hash = hash_token(request.credential);
        let sql = format!("SELECT {TOKEN_COLUMNS} FROM enrolment_tokens WHERE token_hash = ?");
        let Some(row) = sqlx::query(&sql)
            .bind(&token_hash)
            .fetch_optional(&self.pool)
            .await?
        else {
            return Ok(EnrolOutcome::Refused("unknown token"));
        };
        let token = token_from_row(&row)?;
        if let Some(known) = self.enrolment_of(&token.id, &credential_hash).await? {
            return Ok(replay(&known));
        }
        if token.expires_at <= self.now_ms() {
            return Ok(EnrolOutcome::Refused("token expired"));
        }
        let enrolment_id = crate::random_id()?;
        let recorded =
            with_retry(|| self.enrol_tx(&token, &credential_hash, &enrolment_id, request)).await;
        match recorded {
            Ok(outcome) => Ok(outcome),
            // The same request committed concurrently (unique token and
            // credential): its outcome is the answer.
            Err(err) => match self.enrolment_of(&token.id, &credential_hash).await? {
                Some(known) => Ok(replay(&known)),
                None => Err(err.into()),
            },
        }
    }

    async fn enrol_tx(
        &self,
        token: &EnrolmentTokenRow,
        credential_hash: &str,
        enrolment_id: &str,
        request: EnrolRequest<'_>,
    ) -> Result<EnrolOutcome, sqlx::Error> {
        let now = self.now_ms();
        let mut tx = self.pool.begin().await?;
        // Compare-and-set: concurrent enrolments never exceed the uses.
        let counted = sqlx::query(
            "UPDATE enrolment_tokens SET uses = uses + 1, last_used_at = ?
             WHERE id = ? AND expires_at > ? AND (max_uses IS NULL OR uses < max_uses)",
        )
        .bind(now)
        .bind(&token.id)
        .bind(now)
        .execute(&mut *tx)
        .await?
        .rows_affected();
        if counted == 0 {
            return Ok(EnrolOutcome::Refused("token used up or expired"));
        }
        let name = match (&token.node_id, &token.name) {
            (Some(_), Some(name)) => name.as_str(),
            _ => cut(request.name, 255),
        };
        let state = if token.node_id.is_some() {
            enrolment_state::ENROLLED
        } else {
            enrolment_state::PENDING
        };
        sqlx::query(
            "INSERT INTO enrolments (id, token_id, credential_hash, state, node_id, name, address,
             agent_version, component, created_at, updated_at, fleet, approval_required)
             VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
        )
        .bind(enrolment_id)
        .bind(&token.id)
        .bind(credential_hash)
        .bind(state)
        .bind(token.node_id.as_deref())
        .bind(name)
        .bind(cut(request.address, 255))
        .bind(cut(request.agent_version, 191))
        .bind(cut(request.component, 32))
        .bind(now)
        .bind(now)
        .bind(i64::from(token.node_id.is_none()))
        .bind(i64::from(token.approval_required))
        .execute(&mut *tx)
        .await?;
        let outcome = match &token.node_id {
            Some(node_id) => {
                bind_node(&mut tx, node_id, name, credential_hash, now).await?;
                EnrolOutcome::Enrolled {
                    enrolment_id: enrolment_id.to_owned(),
                    node_id: node_id.clone(),
                }
            }
            None => EnrolOutcome::Pending {
                enrolment_id: enrolment_id.to_owned(),
            },
        };
        tx.commit().await?;
        Ok(outcome)
    }

    /// Gives a waiting enrolment its node: the node is created with the
    /// credential of the enrolment. An enrolment whose token asks for an
    /// approval must be approved first. Assigning the same node again is a
    /// no-op.
    pub(crate) async fn assign_enrolment(
        &self,
        id: &str,
        node_id: &str,
        name: Option<&str>,
    ) -> Result<AssignOutcome, Error> {
        Ok(with_retry(|| self.assign_tx(id, node_id, name)).await?)
    }

    async fn assign_tx(
        &self,
        id: &str,
        node_id: &str,
        name: Option<&str>,
    ) -> Result<AssignOutcome, sqlx::Error> {
        let now = self.now_ms();
        let mut tx = self.pool.begin().await?;
        let row = sqlx::query(
            "SELECT state, node_id, name, credential_hash, approval_required
             FROM enrolments WHERE id = ?",
        )
        .bind(id)
        .fetch_optional(&mut *tx)
        .await?;
        let Some(row) = row else {
            return Ok(AssignOutcome::NotFound);
        };
        let state: String = row.try_get("state")?;
        if state == enrolment_state::PENDING && row.try_get::<i64, _>("approval_required")? != 0 {
            return Ok(AssignOutcome::Conflict("enrolment awaits approval"));
        }
        if !WAITING.contains(&state.as_str()) {
            let same = opt_text(&row, "node_id")?.as_deref() == Some(node_id)
                && [enrolment_state::ENROLLED, enrolment_state::COMPLETED]
                    .contains(&state.as_str());
            return Ok(if same {
                AssignOutcome::Assigned
            } else {
                AssignOutcome::Conflict("enrolment is not waiting for a node")
            });
        }
        let taken = sqlx::query("SELECT token_hash FROM nodes WHERE id = ?")
            .bind(node_id)
            .fetch_optional(&mut *tx)
            .await?;
        if let Some(taken) = taken
            && !taken.try_get::<String, _>("token_hash")?.is_empty()
        {
            return Ok(AssignOutcome::Conflict("node already enrolled"));
        }
        let reported: String = row.try_get("name")?;
        let name = name.unwrap_or(&reported);
        let credential_hash: String = row.try_get("credential_hash")?;
        let updated = sqlx::query(
            "UPDATE enrolments SET state = ?, node_id = ?, name = ?, updated_at = ?
             WHERE id = ? AND state = ?",
        )
        .bind(enrolment_state::ENROLLED)
        .bind(node_id)
        .bind(name)
        .bind(now)
        .bind(id)
        .bind(&state)
        .execute(&mut *tx)
        .await?
        .rows_affected();
        if updated == 0 {
            return Ok(AssignOutcome::Conflict(
                "enrolment is not waiting for a node",
            ));
        }
        bind_node(&mut tx, node_id, name, &credential_hash, now).await?;
        tx.commit().await?;
        Ok(AssignOutcome::Assigned)
    }

    /// Moves an enrolment from one of `from` to `to`; false when it was in
    /// none of them (or does not exist).
    pub(crate) async fn set_enrolment_state(
        &self,
        id: &str,
        from: &[&str],
        to: &str,
    ) -> Result<bool, Error> {
        let sql = format!(
            "UPDATE enrolments SET state = ?, updated_at = ? WHERE id = ? AND state IN ({})",
            super::placeholders(from.len())
        );
        let mut update = sqlx::query(&sql).bind(to).bind(self.now_ms()).bind(id);
        for state in from {
            update = update.bind(*state);
        }
        Ok(update.execute(&self.pool).await?.rows_affected() > 0)
    }

    /// Marks the enrolments of a deleted node revoked: its credential is
    /// refused from now on, also by `auto_enroll`.
    pub(crate) async fn revoke_node_enrolments(&self, node_id: &str) -> Result<u64, Error> {
        let done = sqlx::query(
            "UPDATE enrolments SET state = ?, updated_at = ? WHERE node_id = ? AND state IN (?, ?)",
        )
        .bind(enrolment_state::REVOKED)
        .bind(self.now_ms())
        .bind(node_id)
        .bind(enrolment_state::ENROLLED)
        .bind(enrolment_state::COMPLETED)
        .execute(&self.pool)
        .await?;
        Ok(done.rows_affected())
    }

    /// True when the node was enrolled with a token and then deleted.
    pub(crate) async fn node_revoked(&self, node_id: &str) -> Result<bool, Error> {
        let row = sqlx::query("SELECT id FROM enrolments WHERE node_id = ? AND state = ? LIMIT 1")
            .bind(node_id)
            .bind(enrolment_state::REVOKED)
            .fetch_optional(&self.pool)
            .await?;
        Ok(row.is_some())
    }

    /// Deletes the tokens that enrol nothing more since `retention_ms`:
    /// expired, or used up. A token one of whose enrolments still waits
    /// for its node is kept, since its agent asks again with it. The
    /// enrolments stay (history, revocation). Returns the tokens deleted.
    pub(crate) async fn purge_enrolment_tokens(&self, retention_ms: i64) -> Result<u64, Error> {
        let cutoff = self.now_ms().saturating_sub(retention_ms);
        let done = sqlx::query(
            "DELETE FROM enrolment_tokens
             WHERE (expires_at < ?
                    OR (max_uses IS NOT NULL AND uses >= max_uses AND last_used_at < ?))
               AND id NOT IN (SELECT token_id FROM enrolments WHERE state IN (?, ?))",
        )
        .bind(cutoff)
        .bind(cutoff)
        .bind(WAITING[0])
        .bind(WAITING[1])
        .execute(&self.pool)
        .await?;
        Ok(done.rows_affected())
    }

    // ---- credential rotation ----

    /// Stored credentials of a node; `None` when it does not exist.
    pub(crate) async fn node_credentials(
        &self,
        id: &str,
    ) -> Result<Option<NodeCredentials>, Error> {
        let row = sqlx::query(
            "SELECT token_hash, next_token_hash, rotate_requested FROM nodes WHERE id = ?",
        )
        .bind(id)
        .fetch_optional(&self.pool)
        .await?;
        let Some(row) = row else {
            return Ok(None);
        };
        Ok(Some(NodeCredentials {
            token_hash: row.try_get("token_hash")?,
            next_token_hash: opt_text(&row, "next_token_hash")?,
            rotation_requested: row.try_get::<i64, _>("rotate_requested")? != 0,
        }))
    }

    /// Records the credential an agent proposes (hash), next to its
    /// current one: both are accepted until the new one is presented.
    /// Proposing the current credential again is a no-op, so that an agent
    /// retrying after a lost answer learns the rotation is done.
    pub(crate) async fn propose_credential(
        &self,
        id: &str,
        next_hash: &str,
    ) -> Result<NextCredential, Error> {
        let updated = sqlx::query(
            "UPDATE nodes SET next_token_hash = ?
             WHERE id = ? AND token_hash <> '' AND token_hash <> ?",
        )
        .bind(next_hash)
        .bind(id)
        .bind(next_hash)
        .execute(&self.pool)
        .await?
        .rows_affected();
        if updated > 0 {
            return Ok(NextCredential::Recorded);
        }
        Ok(match self.node_credentials(id).await? {
            None => NextCredential::UnknownNode,
            Some(stored) if stored.token_hash.is_empty() => NextCredential::NoCredential,
            Some(_) => NextCredential::Current,
        })
    }

    /// Makes the proposed credential the only one of the node, once its
    /// agent presented it: the former one is refused from now on, and the
    /// sessions opened with it are dropped. False when `next_hash` is not
    /// (or no longer) the proposed credential.
    pub(crate) async fn promote_credential(
        &self,
        id: &str,
        next_hash: &str,
    ) -> Result<bool, Error> {
        Ok(with_retry(|| self.promote_tx(id, next_hash)).await?)
    }

    async fn promote_tx(&self, id: &str, next_hash: &str) -> Result<bool, sqlx::Error> {
        let mut tx = self.pool.begin().await?;
        let promoted = sqlx::query(
            "UPDATE nodes SET token_hash = next_token_hash, next_token_hash = NULL,
             rotate_requested = 0, credential_rotated_at = ?
             WHERE id = ? AND next_token_hash = ?",
        )
        .bind(self.now_ms())
        .bind(id)
        .bind(next_hash)
        .execute(&mut *tx)
        .await?
        .rows_affected();
        if promoted == 0 {
            return Ok(false);
        }
        sqlx::query("DELETE FROM agent_sessions WHERE node_id = ?")
            .bind(id)
            .execute(&mut *tx)
            .await?;
        tx.commit().await?;
        Ok(true)
    }

    /// Asks the agent of a node to rotate its credential at its next
    /// session, and drops its sessions so that the next one starts now.
    pub(crate) async fn request_rotation(&self, id: &str) -> Result<NextCredential, Error> {
        let updated =
            sqlx::query("UPDATE nodes SET rotate_requested = 1 WHERE id = ? AND token_hash <> ''")
                .bind(id)
                .execute(&self.pool)
                .await?
                .rows_affected();
        if updated == 0 {
            return Ok(if self.node_exists(id).await? {
                NextCredential::NoCredential
            } else {
                NextCredential::UnknownNode
            });
        }
        sqlx::query("DELETE FROM agent_sessions WHERE node_id = ?")
            .bind(id)
            .execute(&self.pool)
            .await?;
        Ok(NextCredential::Recorded)
    }
}

/// Creates (or replaces) the node with the credential hash as its token
/// hash, and drops its sessions: one opened with a former token dies.
async fn bind_node(
    tx: &mut sqlx::Transaction<'_, sqlx::Any>,
    node_id: &str,
    name: &str,
    credential_hash: &str,
    now: i64,
) -> Result<(), sqlx::Error> {
    sqlx::query("REPLACE INTO nodes (id, name, token_hash, created_at) VALUES (?, ?, ?, ?)")
        .bind(node_id)
        .bind(name)
        .bind(credential_hash)
        .bind(now)
        .execute(&mut **tx)
        .await?;
    sqlx::query("DELETE FROM agent_sessions WHERE node_id = ?")
        .bind(node_id)
        .execute(&mut **tx)
        .await?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::super::testing::{ENGINE, store, unique};
    use super::*;

    const CREDENTIAL: &str = "c0ffee00c0ffee00c0ffee00c0ffee00c0ffee00";

    fn request<'a>(token: &'a str, credential: &'a str) -> EnrolRequest<'a> {
        EnrolRequest {
            token,
            credential,
            name: "collector-host",
            address: "10.0.0.7",
            agent_version: "0.1.0",
            component: "engine",
        }
    }

    async fn token(
        store: &Store,
        node_id: Option<&str>,
        ttl_ms: i64,
        max_uses: Option<i64>,
    ) -> String {
        token_with(store, node_id, ttl_ms, max_uses, false).await.0
    }

    /// A token and its id.
    async fn token_with(
        store: &Store,
        node_id: Option<&str>,
        ttl_ms: i64,
        max_uses: Option<i64>,
        approval_required: bool,
    ) -> (String, String) {
        let (token, id) = (unique("tok"), unique("tid"));
        let created = store
            .put_enrolment_token(NewEnrolmentToken {
                id: &id,
                token_hash: &hash_token(&token),
                node_id,
                name: node_id.map(|_| "named-poller"),
                expires_at: store.now_ms() + ttl_ms,
                max_uses,
                approval_required,
            })
            .await
            .unwrap();
        assert!(created);
        (token, id)
    }

    fn pending_id(outcome: EnrolOutcome) -> String {
        match outcome {
            EnrolOutcome::Pending { enrolment_id } => enrolment_id,
            other => panic!("not pending: {other:?}"),
        }
    }

    #[tokio::test]
    async fn bound_token_enrols_its_node_once() {
        let (store, _dir) = store().await;
        let node = unique("node");
        let token = token(&store, Some(&node), 60_000, None).await;

        let outcome = store.enrol(request(&token, CREDENTIAL)).await.unwrap();
        let EnrolOutcome::Enrolled {
            node_id,
            enrolment_id,
        } = outcome.clone()
        else {
            panic!("{outcome:?}");
        };
        assert_eq!(node_id, node);
        // The node now authenticates with the credential hash.
        assert_eq!(
            store.node_token_hash(&node).await.unwrap(),
            Some(hash_token(CREDENTIAL))
        );
        assert_eq!(
            store.node(&node).await.unwrap().unwrap().name,
            "named-poller"
        );
        // A retry with the same credential gives the same answer...
        assert_eq!(
            store.enrol(request(&token, CREDENTIAL)).await.unwrap(),
            outcome
        );
        // ...another credential is refused: single use.
        let other = "another-credential-of-32-characters!";
        assert_eq!(
            store.enrol(request(&token, other)).await.unwrap(),
            EnrolOutcome::Refused("token used up or expired")
        );
        let enrolment = store.enrolment(&enrolment_id).await.unwrap().unwrap();
        assert_eq!(enrolment.state, enrolment_state::ENROLLED);
        assert_eq!(enrolment.address, "10.0.0.7");
    }

    #[tokio::test]
    async fn unknown_and_expired_tokens_are_refused() {
        let (store, _dir) = store().await;
        assert_eq!(
            store
                .enrol(request(&unique("nope"), CREDENTIAL))
                .await
                .unwrap(),
            EnrolOutcome::Refused("unknown token")
        );
        let expired = token(&store, Some(&unique("node")), -1, None).await;
        assert_eq!(
            store.enrol(request(&expired, CREDENTIAL)).await.unwrap(),
            EnrolOutcome::Refused("token expired")
        );
    }

    #[tokio::test]
    async fn fleet_token_counts_uses_and_waits_for_assignment() {
        let (store, _dir) = store().await;
        let token = token(&store, None, 60_000, Some(2)).await;
        let credentials = [
            "first-credential-of-at-least-32-chars",
            "second-credential-of-at-least-32-chars",
            "third-credential-of-at-least-32-chars",
        ];
        let EnrolOutcome::Pending { enrolment_id } =
            store.enrol(request(&token, credentials[0])).await.unwrap()
        else {
            panic!("not pending");
        };
        assert!(matches!(
            store.enrol(request(&token, credentials[1])).await.unwrap(),
            EnrolOutcome::Pending { .. }
        ));
        assert_eq!(
            store.enrol(request(&token, credentials[2])).await.unwrap(),
            EnrolOutcome::Refused("token used up or expired")
        );
        // Asking again does not use the token again.
        assert_eq!(
            store.enrol(request(&token, credentials[0])).await.unwrap(),
            EnrolOutcome::Pending {
                enrolment_id: enrolment_id.clone()
            }
        );

        let node = unique("node");
        assert_eq!(
            store
                .assign_enrolment(&enrolment_id, &node, None)
                .await
                .unwrap(),
            AssignOutcome::Assigned
        );
        // Idempotent; another node is a conflict.
        assert_eq!(
            store
                .assign_enrolment(&enrolment_id, &node, None)
                .await
                .unwrap(),
            AssignOutcome::Assigned
        );
        assert!(matches!(
            store
                .assign_enrolment(&enrolment_id, "other", None)
                .await
                .unwrap(),
            AssignOutcome::Conflict(_)
        ));
        assert_eq!(
            store.enrol(request(&token, credentials[0])).await.unwrap(),
            EnrolOutcome::Enrolled {
                enrolment_id: enrolment_id.clone(),
                node_id: node.clone()
            }
        );
        assert_eq!(
            store.node(&node).await.unwrap().unwrap().name,
            "collector-host"
        );
        assert_eq!(
            store
                .assign_enrolment("missing", &node, None)
                .await
                .unwrap(),
            AssignOutcome::NotFound
        );

        // Completed, then the node deleted: revoked.
        assert!(
            store
                .set_enrolment_state(
                    &enrolment_id,
                    &[enrolment_state::ENROLLED],
                    enrolment_state::COMPLETED
                )
                .await
                .unwrap()
        );
        assert!(!store.node_revoked(&node).await.unwrap());
        assert!(store.delete_node(&node).await.unwrap());
        assert!(store.node_revoked(&node).await.unwrap());
        assert_eq!(
            store.enrol(request(&token, credentials[0])).await.unwrap(),
            EnrolOutcome::Refused("enrolment revoked")
        );
    }

    #[tokio::test]
    async fn concurrent_enrolments_never_exceed_the_uses() {
        let (store, _dir) = store().await;
        let token = token(&store, None, 60_000, Some(3)).await;
        let attempts = (0..8).map(|i| {
            let (store, token) = (store.clone(), token.clone());
            tokio::spawn(async move {
                let credential = format!("concurrent-credential-number-{i:04}-xxxx");
                store.enrol(request(&token, &credential)).await.unwrap()
            })
        });
        let mut pending = 0;
        for attempt in attempts {
            pending += usize::from(matches!(
                attempt.await.unwrap(),
                EnrolOutcome::Pending { .. }
            ));
        }
        assert_eq!(pending, 3);
    }

    #[tokio::test]
    async fn token_records_are_idempotent() {
        let (store, _dir) = store().await;
        let (id, hash) = (unique("tid"), hash_token(&unique("tok")));
        let new = NewEnrolmentToken {
            id: &id,
            token_hash: &hash,
            node_id: None,
            name: None,
            expires_at: store.now_ms() + 1000,
            max_uses: None,
            approval_required: false,
        };
        assert!(store.put_enrolment_token(new).await.unwrap());
        assert!(!store.put_enrolment_token(new).await.unwrap());
        let other_hash = hash_token(&unique("tok"));
        assert!(
            store
                .put_enrolment_token(NewEnrolmentToken {
                    token_hash: &other_hash,
                    ..new
                })
                .await
                .is_err()
        );
        assert!(
            store
                .put_enrolment_token(NewEnrolmentToken {
                    id: &unique("tid"),
                    ..new
                })
                .await
                .is_err()
        );
        assert!(
            store
                .list_enrolment_tokens()
                .await
                .unwrap()
                .iter()
                .any(|t| t.id == id)
        );
        assert!(store.delete_enrolment_token(&id).await.unwrap());
        assert!(!store.delete_enrolment_token(&id).await.unwrap());
    }

    #[tokio::test]
    async fn approval_is_needed_before_assignment_and_rejection_is_told() {
        let (store, _dir) = store().await;
        let (token, token_id) = token_with(&store, None, 60_000, Some(2), true).await;
        let listed = store.list_enrolment_tokens().await.unwrap();
        let listed = listed.iter().find(|t| t.id == token_id).unwrap();
        assert!(listed.approval_required);
        assert_eq!(listed.last_used_at, None);

        let first = "approved-credential-of-at-least-32-chars";
        let id = pending_id(store.enrol(request(&token, first)).await.unwrap());
        let enrolment = store.enrolment(&id).await.unwrap().unwrap();
        assert!(enrolment.fleet && enrolment.approval_required);
        let node = unique("node");
        assert_eq!(
            store.assign_enrolment(&id, &node, None).await.unwrap(),
            AssignOutcome::Conflict("enrolment awaits approval")
        );
        assert!(!store.node_exists(&node).await.unwrap());
        assert!(
            store
                .set_enrolment_state(&id, &[enrolment_state::PENDING], enrolment_state::APPROVED)
                .await
                .unwrap()
        );
        // Approved, the agent still waits for its node.
        assert_eq!(
            store.enrol(request(&token, first)).await.unwrap(),
            EnrolOutcome::Pending {
                enrolment_id: id.clone()
            }
        );
        assert_eq!(
            store.assign_enrolment(&id, &node, None).await.unwrap(),
            AssignOutcome::Assigned
        );
        assert!(matches!(
            store.enrol(request(&token, first)).await.unwrap(),
            EnrolOutcome::Enrolled { .. }
        ));

        // A second machine is rejected: it is told, and gets no node.
        let second = "rejected-credential-of-at-least-32-chars";
        let rejected = pending_id(store.enrol(request(&token, second)).await.unwrap());
        assert!(
            store
                .set_enrolment_state(
                    &rejected,
                    &[enrolment_state::PENDING, enrolment_state::APPROVED],
                    enrolment_state::REJECTED
                )
                .await
                .unwrap()
        );
        assert_eq!(
            store.enrol(request(&token, second)).await.unwrap(),
            EnrolOutcome::Rejected {
                enrolment_id: rejected.clone()
            }
        );
        assert!(matches!(
            store
                .assign_enrolment(&rejected, &unique("node"), None)
                .await
                .unwrap(),
            AssignOutcome::Conflict(_)
        ));
        let listed = store.list_enrolment_tokens().await.unwrap();
        let listed = listed.iter().find(|t| t.id == token_id).unwrap();
        assert_eq!(listed.uses, 2);
        assert!(listed.last_used_at.is_some());

        // A token bound to a node never asks for an approval.
        let node = unique("node");
        let (bound, _) = token_with(&store, Some(&node), 60_000, None, true).await;
        assert!(matches!(
            store.enrol(request(&bound, first)).await.unwrap(),
            EnrolOutcome::Enrolled { .. }
        ));
    }

    #[tokio::test]
    async fn purge_keeps_the_tokens_still_needed() {
        let (store, _dir) = store().await;
        // Expired for longer than the retention, never used: purged.
        let (_, expired) = token_with(&store, None, -120_000, None, false).await;
        // Expired, with an enrolment that waits for its node: kept.
        let (waiting_token, waiting) = token_with(&store, None, 60_000, None, false).await;
        store
            .enrol(request(&waiting_token, CREDENTIAL))
            .await
            .unwrap();
        sqlx::query("UPDATE enrolment_tokens SET expires_at = ? WHERE id = ?")
            .bind(store.now_ms() - 1)
            .bind(&waiting)
            .execute(&store.pool)
            .await
            .unwrap();
        // Used up: purged once the retention passed since its last use.
        let node = unique("node");
        let (used_token, used) = token_with(&store, Some(&node), 60_000, None, false).await;
        store.enrol(request(&used_token, CREDENTIAL)).await.unwrap();
        // Valid and usable: kept.
        let (_, valid) = token_with(&store, None, 60_000, Some(5), false).await;

        let ids = |tokens: Vec<EnrolmentTokenRow>| -> Vec<String> {
            tokens.into_iter().map(|token| token.id).collect()
        };
        store.purge_enrolment_tokens(60_000).await.unwrap();
        let left = ids(store.list_enrolment_tokens().await.unwrap());
        assert!(!left.contains(&expired));
        assert!(left.contains(&waiting) && left.contains(&used) && left.contains(&valid));

        tokio::time::sleep(std::time::Duration::from_millis(5)).await;
        store.purge_enrolment_tokens(0).await.unwrap();
        let left = ids(store.list_enrolment_tokens().await.unwrap());
        assert!(!left.contains(&used));
        assert!(left.contains(&waiting) && left.contains(&valid));
        // The enrolments stay: the node keeps working, and stays revocable.
        assert!(store.list_enrolments().await.unwrap().len() >= 2);
    }

    #[tokio::test]
    async fn credential_rotation_accepts_both_until_the_new_one_is_used() {
        let (store, _dir) = store().await;
        let node = unique("node");
        let token = token(&store, Some(&node), 60_000, None).await;
        store.enrol(request(&token, CREDENTIAL)).await.unwrap();
        let (old, new) = (
            hash_token(CREDENTIAL),
            hash_token("the-new-credential-of-32-chars-or-more"),
        );

        assert_eq!(
            store.request_rotation(&node).await.unwrap(),
            NextCredential::Recorded
        );
        assert!(store.node(&node).await.unwrap().unwrap().rotation_requested);
        assert_eq!(
            store.propose_credential(&node, &new).await.unwrap(),
            NextCredential::Recorded
        );
        // Again: harmless.
        assert_eq!(
            store.propose_credential(&node, &new).await.unwrap(),
            NextCredential::Recorded
        );
        let stored = store.node_credentials(&node).await.unwrap().unwrap();
        assert_eq!(stored.token_hash, old);
        assert_eq!(stored.next_token_hash.as_deref(), Some(new.as_str()));
        assert!(stored.rotation_requested);

        store
            .open_session(&node, "session-of-the-old-credential")
            .await;
        assert!(!store.promote_credential(&node, &old).await.unwrap());
        assert!(store.promote_credential(&node, &new).await.unwrap());
        assert!(!store.promote_credential(&node, &new).await.unwrap());
        let stored = store.node_credentials(&node).await.unwrap().unwrap();
        assert_eq!(stored.token_hash, new);
        assert_eq!(stored.next_token_hash, None);
        assert!(!stored.rotation_requested);
        let row = store.node(&node).await.unwrap().unwrap();
        assert!(row.credential_rotated_at.is_some() && !row.rotation_requested);
        // The session opened with the former credential is gone.
        assert!(store.session(&node, ENGINE).await.unwrap().is_none());
        // Proposing the credential in force tells the rotation is done.
        assert_eq!(
            store.propose_credential(&node, &new).await.unwrap(),
            NextCredential::Current
        );

        assert_eq!(
            store.propose_credential("missing", &new).await.unwrap(),
            NextCredential::UnknownNode
        );
        assert_eq!(
            store.request_rotation("missing").await.unwrap(),
            NextCredential::UnknownNode
        );
        let open = unique("node");
        store.ensure_node(&open).await.unwrap();
        assert_eq!(
            store.propose_credential(&open, &new).await.unwrap(),
            NextCredential::NoCredential
        );
        assert_eq!(
            store.request_rotation(&open).await.unwrap(),
            NextCredential::NoCredential
        );
    }
}
