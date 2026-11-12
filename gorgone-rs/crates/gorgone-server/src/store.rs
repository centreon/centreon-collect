//! SQL store: the source of truth shared by every replica.
//!
//! One code path serves SQLite and MariaDB through the sqlx `Any` driver, so
//! statements stay portable and only use integers, strings and byte strings.

use std::time::Duration;

use sha2::{Digest, Sha256};
use sqlx::any::{AnyPoolOptions, AnyRow};
use sqlx::{AnyPool, Row};

use crate::error::Error;

/// Size of one stored blob chunk, also the size of a `FetchBlob` frame.
pub(crate) const CHUNK_SIZE: usize = 1 << 20;

/// Sequence number of the event written by the server when a job expires.
pub(crate) const EXPIRED_SEQ: i64 = i64::MAX;

/// Sequence number of the event written by the server when a job is cancelled
/// before reaching an agent. Agent sequence numbers stay below it.
pub(crate) const CANCELLED_SEQ: i64 = i64::MAX - 1;

/// Values of `jobs.cancel_requested`.
const CANCEL_REQUESTED: i64 = 1;
const CANCEL_SENT: i64 = 2;

/// Job states of the state machine.
pub(crate) mod state {
    pub(crate) const QUEUED: &str = "queued";
    pub(crate) const DISPATCHED: &str = "dispatched";
    pub(crate) const RUNNING: &str = "running";
    pub(crate) const SUCCEEDED: &str = "succeeded";
    pub(crate) const FAILED: &str = "failed";
    pub(crate) const EXPIRED: &str = "expired";
    pub(crate) const CANCELLED: &str = "cancelled";

    /// True when no further transition is possible.
    pub(crate) fn is_terminal(state: &str) -> bool {
        matches!(state, SUCCEEDED | FAILED | EXPIRED | CANCELLED)
    }
}

/// Unix time in milliseconds.
pub(crate) fn now_ms() -> i64 {
    std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .map_or(0, |d| i64::try_from(d.as_millis()).unwrap_or(i64::MAX))
}

/// Lowercase hex SHA-256 of an enrolment token.
pub(crate) fn hash_token(token: &str) -> String {
    hex::encode(Sha256::digest(token.as_bytes()))
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Dialect {
    Sqlite,
    MySql,
}

/// Enrolled node.
#[derive(Debug, Clone)]
pub(crate) struct NodeRow {
    pub(crate) id: String,
    pub(crate) name: String,
    pub(crate) created_at: i64,
}

/// Agent session held by a replica.
#[derive(Debug, Clone)]
pub(crate) struct SessionRow {
    pub(crate) node_id: String,
    pub(crate) session_id: String,
    pub(crate) replica_id: String,
    pub(crate) agent_version: String,
    pub(crate) connected_at: i64,
    pub(crate) last_seen: i64,
}

/// Stored job.
#[derive(Debug, Clone)]
pub(crate) struct JobRow {
    pub(crate) id: String,
    pub(crate) node_id: String,
    pub(crate) kind: String,
    /// Protobuf-encoded `Action`.
    pub(crate) action: Vec<u8>,
    pub(crate) state: String,
    pub(crate) attempts: i64,
    pub(crate) created_at: i64,
    pub(crate) updated_at: i64,
    pub(crate) deadline_at: i64,
    /// A cancellation was asked for while the job was on an agent.
    pub(crate) cancel_requested: bool,
}

/// Stored job event.
#[derive(Debug, Clone)]
pub(crate) struct EventRow {
    pub(crate) seq: i64,
    pub(crate) time_ms: i64,
    pub(crate) code: i64,
    /// JSON document.
    pub(crate) data: String,
}

/// Handle on the shared database.
#[derive(Debug, Clone)]
pub(crate) struct Store {
    pool: AnyPool,
    dialect: Dialect,
}

const JOB_COLUMNS: &str = "id, node_id, kind, action, state, attempts, created_at, updated_at, deadline_at, cancel_requested";

/// Reads a text column. The `Any` driver reports MariaDB `TEXT` types as
/// byte strings, SQLite ones as strings.
fn text(row: &AnyRow, column: &str) -> Result<String, sqlx::Error> {
    match row.try_get::<String, _>(column) {
        Ok(value) => Ok(value),
        Err(_) => {
            let bytes: Vec<u8> = row.try_get(column)?;
            String::from_utf8(bytes).map_err(|err| sqlx::Error::ColumnDecode {
                index: column.to_owned(),
                source: Box::new(err),
            })
        }
    }
}

fn job_from_row(row: &AnyRow) -> Result<JobRow, sqlx::Error> {
    Ok(JobRow {
        id: row.try_get("id")?,
        node_id: row.try_get("node_id")?,
        kind: row.try_get("kind")?,
        action: row.try_get("action")?,
        state: row.try_get("state")?,
        attempts: row.try_get("attempts")?,
        created_at: row.try_get("created_at")?,
        updated_at: row.try_get("updated_at")?,
        deadline_at: row.try_get("deadline_at")?,
        cancel_requested: row.try_get::<i64, _>("cancel_requested")? != 0,
    })
}

fn session_from_row(row: &AnyRow) -> Result<SessionRow, sqlx::Error> {
    Ok(SessionRow {
        node_id: row.try_get("node_id")?,
        session_id: row.try_get("session_id")?,
        replica_id: row.try_get("replica_id")?,
        agent_version: row.try_get("agent_version")?,
        connected_at: row.try_get("connected_at")?,
        last_seen: row.try_get("last_seen")?,
    })
}

impl Store {
    /// Connects to `url` (`sqlite:` or `mysql:`/`mariadb:`) and creates the schema.
    pub(crate) async fn connect(url: &str) -> Result<Self, Error> {
        sqlx::any::install_default_drivers();
        let dialect = if url.starts_with("sqlite:") {
            Dialect::Sqlite
        } else if url.starts_with("mysql:") || url.starts_with("mariadb:") {
            Dialect::MySql
        } else {
            return Err(Error::Invalid(
                "database_url must start with sqlite:, mysql: or mariadb:".into(),
            ));
        };
        let mut options = AnyPoolOptions::new().acquire_timeout(Duration::from_secs(15));
        options = match dialect {
            // Few connections: SQLite has a single writer anyway.
            Dialect::Sqlite => options.max_connections(4).after_connect(|conn, _| {
                Box::pin(async move {
                    // WAL lets readers proceed while another replica writes.
                    for pragma in [
                        "PRAGMA busy_timeout = 15000",
                        "PRAGMA journal_mode = WAL",
                        "PRAGMA synchronous = NORMAL",
                    ] {
                        sqlx::query(pragma).execute(&mut *conn).await?;
                    }
                    Ok(())
                })
            }),
            Dialect::MySql => options.max_connections(16),
        };
        let store = Self {
            pool: options.connect(url).await?,
            dialect,
        };
        store.migrate().await?;
        Ok(store)
    }

    async fn migrate(&self) -> Result<(), Error> {
        let (text, blob) = match self.dialect {
            Dialect::Sqlite => ("TEXT", "BLOB"),
            Dialect::MySql => ("LONGTEXT", "MEDIUMBLOB"),
        };
        // MySQL has no portable `CREATE INDEX IF NOT EXISTS`: declare keys inline.
        let job_keys = match self.dialect {
            Dialect::Sqlite => "",
            Dialect::MySql => {
                ", KEY idx_jobs_node_state (node_id, state), KEY idx_jobs_state (state, deadline_at)"
            }
        };
        let mut statements = vec![
            "CREATE TABLE IF NOT EXISTS nodes (
                id VARCHAR(191) NOT NULL PRIMARY KEY,
                name VARCHAR(255) NOT NULL,
                token_hash VARCHAR(64) NOT NULL,
                created_at BIGINT NOT NULL)"
                .to_owned(),
            "CREATE TABLE IF NOT EXISTS replicas (
                id VARCHAR(191) NOT NULL PRIMARY KEY,
                peer_addr VARCHAR(255) NOT NULL,
                last_seen BIGINT NOT NULL)"
                .to_owned(),
            "CREATE TABLE IF NOT EXISTS sessions (
                node_id VARCHAR(191) NOT NULL PRIMARY KEY,
                session_id VARCHAR(191) NOT NULL,
                replica_id VARCHAR(191) NOT NULL,
                agent_version VARCHAR(191) NOT NULL,
                connected_at BIGINT NOT NULL,
                last_seen BIGINT NOT NULL)"
                .to_owned(),
            format!(
                "CREATE TABLE IF NOT EXISTS jobs (
                id VARCHAR(191) NOT NULL PRIMARY KEY,
                node_id VARCHAR(191) NOT NULL,
                kind VARCHAR(32) NOT NULL,
                action {blob} NOT NULL,
                state VARCHAR(16) NOT NULL,
                attempts BIGINT NOT NULL,
                session_id VARCHAR(191) NULL,
                created_at BIGINT NOT NULL,
                updated_at BIGINT NOT NULL,
                deadline_at BIGINT NOT NULL,
                cancel_requested BIGINT NOT NULL DEFAULT 0{job_keys})"
            ),
            format!(
                "CREATE TABLE IF NOT EXISTS job_events (
                job_id VARCHAR(191) NOT NULL,
                seq BIGINT NOT NULL,
                time_ms BIGINT NOT NULL,
                code BIGINT NOT NULL,
                data {text} NOT NULL,
                PRIMARY KEY (job_id, seq))"
            ),
            "CREATE TABLE IF NOT EXISTS blobs (
                digest VARCHAR(64) NOT NULL PRIMARY KEY,
                size BIGINT NOT NULL,
                created_at BIGINT NOT NULL)"
                .to_owned(),
            format!(
                "CREATE TABLE IF NOT EXISTS blob_chunks (
                digest VARCHAR(64) NOT NULL,
                idx BIGINT NOT NULL,
                data {blob} NOT NULL,
                PRIMARY KEY (digest, idx))"
            ),
            "CREATE TABLE IF NOT EXISTS leases (
                name VARCHAR(191) NOT NULL PRIMARY KEY,
                holder VARCHAR(191) NOT NULL,
                expires_at BIGINT NOT NULL)"
                .to_owned(),
            "CREATE TABLE IF NOT EXISTS meta (
                name VARCHAR(191) NOT NULL PRIMARY KEY,
                value VARCHAR(255) NOT NULL)"
                .to_owned(),
        ];
        if self.dialect == Dialect::Sqlite {
            statements.push(
                "CREATE INDEX IF NOT EXISTS idx_jobs_node_state ON jobs (node_id, state)".into(),
            );
            statements.push(
                "CREATE INDEX IF NOT EXISTS idx_jobs_state ON jobs (state, deadline_at)".into(),
            );
        }
        for statement in &statements {
            sqlx::query(statement).execute(&self.pool).await?;
        }
        // Stores created before cancellation existed lack the column.
        if sqlx::query("SELECT cancel_requested FROM jobs LIMIT 1")
            .fetch_optional(&self.pool)
            .await
            .is_err()
        {
            sqlx::query("ALTER TABLE jobs ADD COLUMN cancel_requested BIGINT NOT NULL DEFAULT 0")
                .execute(&self.pool)
                .await?;
        }
        Ok(())
    }

    /// Identifier shared by every replica of this store, created once.
    pub(crate) async fn server_id(&self) -> Result<String, Error> {
        let sql = format!(
            "{} INTO meta (name, value) VALUES (?, ?)",
            self.insert_ignore()
        );
        // Concurrent replicas: the first insert wins, all read the same row.
        sqlx::query(&sql)
            .bind("server_id")
            .bind(uuid::Uuid::new_v4().simple().to_string())
            .execute(&self.pool)
            .await?;
        let row = sqlx::query("SELECT value FROM meta WHERE name = ?")
            .bind("server_id")
            .fetch_one(&self.pool)
            .await?;
        Ok(row.try_get("value")?)
    }

    fn insert_ignore(&self) -> &'static str {
        match self.dialect {
            Dialect::Sqlite => "INSERT OR IGNORE",
            Dialect::MySql => "INSERT IGNORE",
        }
    }

    /// Cheap round trip used by the readiness probe.
    pub(crate) async fn ping(&self) -> Result<(), Error> {
        sqlx::query("SELECT COUNT(*) FROM leases")
            .fetch_one(&self.pool)
            .await?;
        Ok(())
    }

    /// Closes the pool.
    pub(crate) async fn close(&self) {
        self.pool.close().await;
    }

    // ---- nodes ----

    /// Enrols (or re-enrols) a node and returns its new token, shown once.
    pub(crate) async fn enrol_node(&self, id: &str, name: &str) -> Result<String, Error> {
        let token = hex::encode(rand::random::<[u8; 32]>());
        sqlx::query("REPLACE INTO nodes (id, name, token_hash, created_at) VALUES (?, ?, ?, ?)")
            .bind(id)
            .bind(name)
            .bind(hash_token(&token))
            .bind(now_ms())
            .execute(&self.pool)
            .await?;
        Ok(token)
    }

    /// Records a node without token (development `auto_enroll` only).
    pub(crate) async fn ensure_node(&self, id: &str) -> Result<(), Error> {
        let sql = format!(
            "{} INTO nodes (id, name, token_hash, created_at) VALUES (?, ?, '', ?)",
            self.insert_ignore()
        );
        sqlx::query(&sql)
            .bind(id)
            .bind(id)
            .bind(now_ms())
            .execute(&self.pool)
            .await?;
        Ok(())
    }

    /// Token hash of a node; empty for an auto-enrolled node.
    pub(crate) async fn node_token_hash(&self, id: &str) -> Result<Option<String>, Error> {
        let row = sqlx::query("SELECT token_hash FROM nodes WHERE id = ?")
            .bind(id)
            .fetch_optional(&self.pool)
            .await?;
        Ok(row.map(|r| r.try_get("token_hash")).transpose()?)
    }

    /// Removes a node and its session. False when it did not exist.
    pub(crate) async fn delete_node(&self, id: &str) -> Result<bool, Error> {
        sqlx::query("DELETE FROM sessions WHERE node_id = ?")
            .bind(id)
            .execute(&self.pool)
            .await?;
        let done = sqlx::query("DELETE FROM nodes WHERE id = ?")
            .bind(id)
            .execute(&self.pool)
            .await?;
        Ok(done.rows_affected() > 0)
    }

    /// All enrolled nodes.
    pub(crate) async fn list_nodes(&self) -> Result<Vec<NodeRow>, Error> {
        let rows = sqlx::query("SELECT id, name, created_at FROM nodes ORDER BY id")
            .fetch_all(&self.pool)
            .await?;
        rows.iter()
            .map(|row| {
                Ok(NodeRow {
                    id: row.try_get("id")?,
                    name: row.try_get("name")?,
                    created_at: row.try_get("created_at")?,
                })
            })
            .collect::<Result<_, sqlx::Error>>()
            .map_err(Error::from)
    }

    // ---- replicas ----

    /// Records that `id` is alive and reachable by its peers at `peer_addr`.
    pub(crate) async fn heartbeat_replica(&self, id: &str, peer_addr: &str) -> Result<(), Error> {
        sqlx::query("REPLACE INTO replicas (id, peer_addr, last_seen) VALUES (?, ?, ?)")
            .bind(id)
            .bind(peer_addr)
            .bind(now_ms())
            .execute(&self.pool)
            .await?;
        Ok(())
    }

    /// Peer address of a replica.
    pub(crate) async fn replica_addr(&self, id: &str) -> Result<Option<String>, Error> {
        let row = sqlx::query("SELECT peer_addr FROM replicas WHERE id = ?")
            .bind(id)
            .fetch_optional(&self.pool)
            .await?;
        Ok(row.map(|r| r.try_get("peer_addr")).transpose()?)
    }

    /// Removes a replica and the sessions it held (graceful shutdown).
    pub(crate) async fn remove_replica(&self, id: &str) -> Result<(), Error> {
        sqlx::query("DELETE FROM sessions WHERE replica_id = ?")
            .bind(id)
            .execute(&self.pool)
            .await?;
        sqlx::query("DELETE FROM replicas WHERE id = ?")
            .bind(id)
            .execute(&self.pool)
            .await?;
        Ok(())
    }

    /// Drops replicas silent since `cutoff_ms` and every session of an unknown
    /// replica. Returns the number of sessions dropped.
    pub(crate) async fn drop_dead_replicas(&self, cutoff_ms: i64) -> Result<u64, Error> {
        sqlx::query("DELETE FROM replicas WHERE last_seen < ?")
            .bind(cutoff_ms)
            .execute(&self.pool)
            .await?;
        let done =
            sqlx::query("DELETE FROM sessions WHERE replica_id NOT IN (SELECT id FROM replicas)")
                .execute(&self.pool)
                .await?;
        Ok(done.rows_affected())
    }

    // ---- sessions ----

    /// Registers the session of a node, replacing any older one.
    pub(crate) async fn upsert_session(
        &self,
        node_id: &str,
        session_id: &str,
        replica_id: &str,
        agent_version: &str,
    ) -> Result<(), Error> {
        let now = now_ms();
        sqlx::query(
            "REPLACE INTO sessions
             (node_id, session_id, replica_id, agent_version, connected_at, last_seen)
             VALUES (?, ?, ?, ?, ?, ?)",
        )
        .bind(node_id)
        .bind(session_id)
        .bind(replica_id)
        .bind(agent_version)
        .bind(now)
        .bind(now)
        .execute(&self.pool)
        .await?;
        Ok(())
    }

    /// Updates `last_seen`. False when the session is no longer the current one.
    pub(crate) async fn touch_session(
        &self,
        node_id: &str,
        session_id: &str,
    ) -> Result<bool, Error> {
        let done =
            sqlx::query("UPDATE sessions SET last_seen = ? WHERE node_id = ? AND session_id = ?")
                .bind(now_ms())
                .bind(node_id)
                .bind(session_id)
                .execute(&self.pool)
                .await?;
        Ok(done.rows_affected() > 0)
    }

    /// Removes a session if it is still the current one of its node.
    pub(crate) async fn remove_session(
        &self,
        node_id: &str,
        session_id: &str,
    ) -> Result<(), Error> {
        sqlx::query("DELETE FROM sessions WHERE node_id = ? AND session_id = ?")
            .bind(node_id)
            .bind(session_id)
            .execute(&self.pool)
            .await?;
        Ok(())
    }

    /// Current session of a node.
    pub(crate) async fn session(&self, node_id: &str) -> Result<Option<SessionRow>, Error> {
        let row = sqlx::query(
            "SELECT node_id, session_id, replica_id, agent_version, connected_at, last_seen
             FROM sessions WHERE node_id = ?",
        )
        .bind(node_id)
        .fetch_optional(&self.pool)
        .await?;
        Ok(row.as_ref().map(session_from_row).transpose()?)
    }

    /// Every session, all replicas included.
    pub(crate) async fn list_sessions(&self) -> Result<Vec<SessionRow>, Error> {
        let rows = sqlx::query(
            "SELECT node_id, session_id, replica_id, agent_version, connected_at, last_seen
             FROM sessions ORDER BY node_id",
        )
        .fetch_all(&self.pool)
        .await?;
        Ok(rows
            .iter()
            .map(session_from_row)
            .collect::<Result<_, _>>()?)
    }

    // ---- jobs ----

    /// Commits a new `queued` job together with its "accepted" event (seq 0).
    pub(crate) async fn insert_job(
        &self,
        id: &str,
        node_id: &str,
        kind: &str,
        action: &[u8],
        deadline_at: i64,
        accepted_data: &str,
    ) -> Result<(), Error> {
        let now = now_ms();
        // One transaction: a job never exists without its "accepted" event.
        let mut tx = self.pool.begin().await?;
        sqlx::query(
            "INSERT INTO jobs
             (id, node_id, kind, action, state, attempts, session_id, created_at, updated_at, deadline_at)
             VALUES (?, ?, ?, ?, ?, 0, NULL, ?, ?, ?)",
        )
        .bind(id)
        .bind(node_id)
        .bind(kind)
        .bind(action)
        .bind(state::QUEUED)
        .bind(now)
        .bind(now)
        .bind(deadline_at)
        .execute(&mut *tx)
        .await?;
        sqlx::query(
            "INSERT INTO job_events (job_id, seq, time_ms, code, data) VALUES (?, 0, ?, ?, ?)",
        )
        .bind(id)
        .bind(now)
        .bind(i64::from(gorgone_core::legacy::ACTION_BEGIN))
        .bind(accepted_data)
        .execute(&mut *tx)
        .await?;
        tx.commit().await?;
        Ok(())
    }

    /// One job.
    pub(crate) async fn job(&self, id: &str) -> Result<Option<JobRow>, Error> {
        let sql = format!("SELECT {JOB_COLUMNS} FROM jobs WHERE id = ?");
        let row = sqlx::query(&sql)
            .bind(id)
            .fetch_optional(&self.pool)
            .await?;
        Ok(row.as_ref().map(job_from_row).transpose()?)
    }

    /// Oldest `queued` jobs of a node that are not past their deadline.
    pub(crate) async fn queued_jobs(
        &self,
        node_id: &str,
        limit: i64,
    ) -> Result<Vec<JobRow>, Error> {
        let sql = format!(
            "SELECT {JOB_COLUMNS} FROM jobs
             WHERE node_id = ? AND state = ? AND deadline_at > ? AND cancel_requested = 0
             ORDER BY created_at, id LIMIT ?"
        );
        let rows = sqlx::query(&sql)
            .bind(node_id)
            .bind(state::QUEUED)
            .bind(now_ms())
            .bind(limit)
            .fetch_all(&self.pool)
            .await?;
        Ok(rows.iter().map(job_from_row).collect::<Result<_, _>>()?)
    }

    /// Compare-and-set `queued → dispatched`. True when this caller won the job.
    pub(crate) async fn claim_job(&self, id: &str, session_id: &str) -> Result<bool, Error> {
        let done = sqlx::query(
            "UPDATE jobs SET state = ?, session_id = ?, attempts = attempts + 1, updated_at = ?
             WHERE id = ? AND state = ?",
        )
        .bind(state::DISPATCHED)
        .bind(session_id)
        .bind(now_ms())
        .bind(id)
        .bind(state::QUEUED)
        .execute(&self.pool)
        .await?;
        Ok(done.rows_affected() == 1)
    }

    /// Gives back a claimed job that could not be pushed to its session.
    pub(crate) async fn unclaim_job(&self, id: &str, session_id: &str) -> Result<(), Error> {
        sqlx::query(
            "UPDATE jobs SET state = ?, session_id = NULL, updated_at = ?
             WHERE id = ? AND state = ? AND session_id = ?",
        )
        .bind(state::QUEUED)
        .bind(now_ms())
        .bind(id)
        .bind(state::DISPATCHED)
        .bind(session_id)
        .execute(&self.pool)
        .await?;
        Ok(())
    }

    /// `queued | dispatched → running`, on the first agent event.
    pub(crate) async fn mark_running(&self, id: &str) -> Result<(), Error> {
        sqlx::query("UPDATE jobs SET state = ?, updated_at = ? WHERE id = ? AND state IN (?, ?)")
            .bind(state::RUNNING)
            .bind(now_ms())
            .bind(id)
            .bind(state::QUEUED)
            .bind(state::DISPATCHED)
            .execute(&self.pool)
            .await?;
        Ok(())
    }

    /// Sets the final state of a job that is not terminal yet.
    pub(crate) async fn finish_job(&self, id: &str, final_state: &str) -> Result<bool, Error> {
        let done = sqlx::query(
            "UPDATE jobs SET state = ?, updated_at = ? WHERE id = ? AND state IN (?, ?, ?)",
        )
        .bind(final_state)
        .bind(now_ms())
        .bind(id)
        .bind(state::QUEUED)
        .bind(state::DISPATCHED)
        .bind(state::RUNNING)
        .execute(&self.pool)
        .await?;
        Ok(done.rows_affected() == 1)
    }

    /// Puts back in the queue the jobs dispatched to a session that no longer
    /// exists and that never produced an agent event.
    pub(crate) async fn requeue_orphans(&self) -> Result<u64, Error> {
        let done = sqlx::query(
            "UPDATE jobs SET state = ?, session_id = NULL, updated_at = ?
             WHERE state = ?
               AND session_id NOT IN (SELECT session_id FROM sessions)
               AND NOT EXISTS (SELECT 1 FROM job_events e WHERE e.job_id = jobs.id AND e.seq > 0)",
        )
        .bind(state::QUEUED)
        .bind(now_ms())
        .bind(state::DISPATCHED)
        .execute(&self.pool)
        .await?;
        Ok(done.rows_affected())
    }

    /// Expires queued jobs past their deadline, and jobs handed to an agent
    /// that stayed silent `grace_ms` after it. Returns the expired job ids.
    pub(crate) async fn expire_jobs(
        &self,
        grace_ms: i64,
        data: &str,
    ) -> Result<Vec<String>, Error> {
        let now = now_ms();
        let rows = sqlx::query(
            "SELECT id FROM jobs
             WHERE (state = ? AND deadline_at <= ?) OR (state IN (?, ?) AND deadline_at <= ?)
             LIMIT 500",
        )
        .bind(state::QUEUED)
        .bind(now)
        .bind(state::DISPATCHED)
        .bind(state::RUNNING)
        .bind(now.saturating_sub(grace_ms))
        .fetch_all(&self.pool)
        .await?;
        let mut expired = Vec::new();
        for row in &rows {
            let id: String = row.try_get("id")?;
            // Another replica may run the same sweep: only the winner logs.
            if self.finish_job(&id, state::EXPIRED).await? {
                let code = i64::from(gorgone_core::legacy::ACTION_FINISH_KO);
                self.insert_event(&id, EXPIRED_SEQ, now, code, data).await?;
                expired.push(id);
            }
        }
        Ok(expired)
    }

    /// Cancels a job still in the queue. True when this caller cancelled it.
    pub(crate) async fn cancel_queued(&self, id: &str, data: &str) -> Result<bool, Error> {
        let now = now_ms();
        let done =
            sqlx::query("UPDATE jobs SET state = ?, updated_at = ? WHERE id = ? AND state = ?")
                .bind(state::CANCELLED)
                .bind(now)
                .bind(id)
                .bind(state::QUEUED)
                .execute(&self.pool)
                .await?;
        if done.rows_affected() != 1 {
            return Ok(false);
        }
        let code = i64::from(gorgone_core::legacy::ACTION_FINISH_KO);
        self.insert_event(id, CANCELLED_SEQ, now, code, data)
            .await?;
        Ok(true)
    }

    /// Flags a job that already reached an agent; its owner replica sends the
    /// `Cancel` frame. True when the job is still dispatched or running.
    pub(crate) async fn request_cancel(&self, id: &str) -> Result<bool, Error> {
        // Only an unflagged row is written, so that a sent frame is not resent.
        sqlx::query(
            "UPDATE jobs SET cancel_requested = ?
             WHERE id = ? AND state IN (?, ?) AND cancel_requested = 0",
        )
        .bind(CANCEL_REQUESTED)
        .bind(id)
        .bind(state::DISPATCHED)
        .bind(state::RUNNING)
        .execute(&self.pool)
        .await?;
        let row = sqlx::query("SELECT COUNT(*) AS n FROM jobs WHERE id = ? AND state IN (?, ?)")
            .bind(id)
            .bind(state::DISPATCHED)
            .bind(state::RUNNING)
            .fetch_one(&self.pool)
            .await?;
        Ok(row.try_get::<i64, _>("n")? > 0)
    }

    /// Jobs of a node whose `Cancel` frame has not been sent yet.
    pub(crate) async fn pending_cancels(&self, node_id: &str) -> Result<Vec<String>, Error> {
        let rows = sqlx::query(
            "SELECT id FROM jobs
             WHERE node_id = ? AND cancel_requested = ? AND state IN (?, ?)",
        )
        .bind(node_id)
        .bind(CANCEL_REQUESTED)
        .bind(state::DISPATCHED)
        .bind(state::RUNNING)
        .fetch_all(&self.pool)
        .await?;
        Ok(rows
            .iter()
            .map(|row| row.try_get("id"))
            .collect::<Result<_, _>>()?)
    }

    /// Records that the `Cancel` frame of a job was handed to its session.
    pub(crate) async fn mark_cancel_sent(&self, id: &str) -> Result<(), Error> {
        sqlx::query("UPDATE jobs SET cancel_requested = ? WHERE id = ? AND cancel_requested = ?")
            .bind(CANCEL_SENT)
            .bind(id)
            .bind(CANCEL_REQUESTED)
            .execute(&self.pool)
            .await?;
        Ok(())
    }

    /// Cancels the flagged jobs that went back to the queue (session lost
    /// before the agent produced anything). Returns their ids.
    pub(crate) async fn cancel_requeued(&self, data: &str) -> Result<Vec<String>, Error> {
        let rows =
            sqlx::query("SELECT id FROM jobs WHERE state = ? AND cancel_requested <> 0 LIMIT 500")
                .bind(state::QUEUED)
                .fetch_all(&self.pool)
                .await?;
        let mut cancelled = Vec::new();
        for row in &rows {
            let id: String = row.try_get("id")?;
            if self.cancel_queued(&id, data).await? {
                cancelled.push(id);
            }
        }
        Ok(cancelled)
    }

    /// Jobs of a node, oldest first.
    #[cfg(test)]
    pub(crate) async fn jobs_of_node(&self, node_id: &str) -> Result<Vec<JobRow>, Error> {
        let sql =
            format!("SELECT {JOB_COLUMNS} FROM jobs WHERE node_id = ? ORDER BY created_at, id");
        let rows = sqlx::query(&sql)
            .bind(node_id)
            .fetch_all(&self.pool)
            .await?;
        Ok(rows.iter().map(job_from_row).collect::<Result<_, _>>()?)
    }

    /// Deletes terminal jobs (with their events) and blobs older than `cutoff_ms`.
    pub(crate) async fn purge(&self, cutoff_ms: i64) -> Result<u64, Error> {
        let terminal = [
            state::SUCCEEDED,
            state::FAILED,
            state::EXPIRED,
            state::CANCELLED,
        ];
        sqlx::query(
            "DELETE FROM job_events WHERE job_id IN
             (SELECT id FROM jobs WHERE state IN (?, ?, ?, ?) AND updated_at < ?)",
        )
        .bind(terminal[0])
        .bind(terminal[1])
        .bind(terminal[2])
        .bind(terminal[3])
        .bind(cutoff_ms)
        .execute(&self.pool)
        .await?;
        let done = sqlx::query("DELETE FROM jobs WHERE state IN (?, ?, ?, ?) AND updated_at < ?")
            .bind(terminal[0])
            .bind(terminal[1])
            .bind(terminal[2])
            .bind(terminal[3])
            .bind(cutoff_ms)
            .execute(&self.pool)
            .await?;
        sqlx::query(
            "DELETE FROM blob_chunks WHERE digest IN
             (SELECT digest FROM blobs WHERE created_at < ?)",
        )
        .bind(cutoff_ms)
        .execute(&self.pool)
        .await?;
        sqlx::query("DELETE FROM blobs WHERE created_at < ?")
            .bind(cutoff_ms)
            .execute(&self.pool)
            .await?;
        Ok(done.rows_affected())
    }

    // ---- job events ----

    /// Appends an event; a repeated `(job_id, seq)` is ignored. True when new.
    pub(crate) async fn insert_event(
        &self,
        job_id: &str,
        seq: i64,
        time_ms: i64,
        code: i64,
        data: &str,
    ) -> Result<bool, Error> {
        let sql = format!(
            "{} INTO job_events (job_id, seq, time_ms, code, data) VALUES (?, ?, ?, ?, ?)",
            self.insert_ignore()
        );
        let done = sqlx::query(&sql)
            .bind(job_id)
            .bind(seq)
            .bind(time_ms)
            .bind(code)
            .bind(data)
            .execute(&self.pool)
            .await?;
        Ok(done.rows_affected() == 1)
    }

    /// Events of a job with `seq > after_seq`, in sequence order.
    pub(crate) async fn events(
        &self,
        job_id: &str,
        after_seq: i64,
    ) -> Result<Vec<EventRow>, Error> {
        let rows = sqlx::query(
            "SELECT seq, time_ms, code, data FROM job_events
             WHERE job_id = ? AND seq > ? ORDER BY seq",
        )
        .bind(job_id)
        .bind(after_seq)
        .fetch_all(&self.pool)
        .await?;
        rows.iter()
            .map(|row| {
                Ok(EventRow {
                    seq: row.try_get("seq")?,
                    time_ms: row.try_get("time_ms")?,
                    code: row.try_get("code")?,
                    data: text(row, "data")?,
                })
            })
            .collect::<Result<_, sqlx::Error>>()
            .map_err(Error::from)
    }

    // ---- blobs ----

    /// Stores a blob under its SHA-256. Idempotent by digest.
    pub(crate) async fn put_blob(&self, bytes: &[u8]) -> Result<(String, i64), Error> {
        let digest = gorgone_core::bundle::digest(bytes);
        let size =
            i64::try_from(bytes.len()).map_err(|_| Error::Invalid("blob too large".into()))?;
        // Refreshing the date keeps a re-uploaded blob away from the purge.
        let touched = sqlx::query("UPDATE blobs SET created_at = ? WHERE digest = ?")
            .bind(now_ms())
            .bind(&digest)
            .execute(&self.pool)
            .await?;
        if touched.rows_affected() > 0 {
            return Ok((digest, size));
        }
        // One transaction: readers never see a blob without all its chunks.
        let mut tx = self.pool.begin().await?;
        for (idx, chunk) in bytes.chunks(CHUNK_SIZE).enumerate() {
            sqlx::query("REPLACE INTO blob_chunks (digest, idx, data) VALUES (?, ?, ?)")
                .bind(&digest)
                .bind(i64::try_from(idx).unwrap_or(i64::MAX))
                .bind(chunk)
                .execute(&mut *tx)
                .await?;
        }
        let sql = format!(
            "{} INTO blobs (digest, size, created_at) VALUES (?, ?, ?)",
            self.insert_ignore()
        );
        sqlx::query(&sql)
            .bind(&digest)
            .bind(size)
            .bind(now_ms())
            .execute(&mut *tx)
            .await?;
        tx.commit().await?;
        Ok((digest, size))
    }

    /// Size of a complete blob.
    pub(crate) async fn blob_size(&self, digest: &str) -> Result<Option<i64>, Error> {
        let row = sqlx::query("SELECT size FROM blobs WHERE digest = ?")
            .bind(digest)
            .fetch_optional(&self.pool)
            .await?;
        Ok(row.map(|r| r.try_get("size")).transpose()?)
    }

    /// One chunk of a blob; `None` past the last one.
    pub(crate) async fn blob_chunk(
        &self,
        digest: &str,
        idx: i64,
    ) -> Result<Option<Vec<u8>>, Error> {
        let row = sqlx::query("SELECT data FROM blob_chunks WHERE digest = ? AND idx = ?")
            .bind(digest)
            .bind(idx)
            .fetch_optional(&self.pool)
            .await?;
        Ok(row.map(|r| r.try_get("data")).transpose()?)
    }

    // ---- leases ----

    /// Acquires or renews the lease `name` for `ttl_ms`. True when `holder`
    /// holds it. Atomic: a single conditional statement decides the winner.
    pub(crate) async fn acquire_lease(
        &self,
        name: &str,
        holder: &str,
        ttl_ms: i64,
    ) -> Result<bool, Error> {
        let now = now_ms();
        let expires = now.saturating_add(ttl_ms);
        let renewed = sqlx::query(
            "UPDATE leases SET holder = ?, expires_at = ?
             WHERE name = ? AND (holder = ? OR expires_at < ?)",
        )
        .bind(holder)
        .bind(expires)
        .bind(name)
        .bind(holder)
        .bind(now)
        .execute(&self.pool)
        .await?;
        if renewed.rows_affected() == 1 {
            return Ok(true);
        }
        let sql = format!(
            "{} INTO leases (name, holder, expires_at) VALUES (?, ?, ?)",
            self.insert_ignore()
        );
        let created = sqlx::query(&sql)
            .bind(name)
            .bind(holder)
            .bind(expires)
            .execute(&self.pool)
            .await?;
        Ok(created.rows_affected() == 1)
    }
}

impl Store {
    /// Gives up a lease so that another replica can take it at once.
    pub(crate) async fn release_lease(&self, name: &str, holder: &str) -> Result<(), Error> {
        sqlx::query("DELETE FROM leases WHERE name = ? AND holder = ?")
            .bind(name)
            .bind(holder)
            .execute(&self.pool)
            .await?;
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Store on a fresh SQLite file, or on `GORGONE_TEST_DATABASE_URL`.
    async fn store() -> (Store, Option<tempfile::TempDir>) {
        if let Ok(url) = std::env::var("GORGONE_TEST_DATABASE_URL") {
            return (Store::connect(&url).await.unwrap(), None);
        }
        let dir = tempfile::tempdir().unwrap();
        let url = format!("sqlite://{}/gorgone.db?mode=rwc", dir.path().display());
        (Store::connect(&url).await.unwrap(), Some(dir))
    }

    fn unique(prefix: &str) -> String {
        format!("{prefix}-{}", uuid::Uuid::now_v7())
    }

    #[tokio::test]
    async fn claim_is_a_compare_and_set() {
        let (store, _dir) = store().await;
        let (job, node) = (unique("job"), unique("node"));
        store
            .insert_job(&job, &node, "run_commands", b"x", now_ms() + 60_000, "{}")
            .await
            .unwrap();

        // Concurrent claims: exactly one winner.
        let claims = (0..8).map(|i| {
            let (store, job) = (store.clone(), job.clone());
            tokio::spawn(async move { store.claim_job(&job, &format!("s{i}")).await.unwrap() })
        });
        let mut won = 0;
        for claim in claims {
            won += usize::from(claim.await.unwrap());
        }
        assert_eq!(won, 1);

        let row = store.job(&job).await.unwrap().unwrap();
        assert_eq!((row.state.as_str(), row.attempts), (state::DISPATCHED, 1));
        assert!(store.queued_jobs(&node, 10).await.unwrap().is_empty());
    }

    #[tokio::test]
    async fn orphans_are_requeued_unless_they_have_agent_events() {
        let (store, _dir) = store().await;
        let node = unique("node");
        let (silent, started) = (unique("job"), unique("job"));
        for job in [&silent, &started] {
            store
                .insert_job(job, &node, "run_commands", b"x", now_ms() + 60_000, "{}")
                .await
                .unwrap();
            assert!(store.claim_job(job, "gone-session").await.unwrap());
        }
        assert!(
            store
                .insert_event(&started, 1, now_ms(), 4, "{}")
                .await
                .unwrap()
        );

        assert!(store.requeue_orphans().await.unwrap() >= 1);
        assert_eq!(
            store.job(&silent).await.unwrap().unwrap().state,
            state::QUEUED
        );
        assert_eq!(
            store.job(&started).await.unwrap().unwrap().state,
            state::DISPATCHED
        );
    }

    #[tokio::test]
    async fn events_are_idempotent_on_job_and_seq() {
        let (store, _dir) = store().await;
        let job = unique("job");
        assert!(
            store
                .insert_event(&job, 1, 10, 100, "{\"a\":1}")
                .await
                .unwrap()
        );
        assert!(
            !store
                .insert_event(&job, 1, 20, 100, "{\"a\":2}")
                .await
                .unwrap()
        );
        let events = store.events(&job, -1).await.unwrap();
        assert_eq!(events.len(), 1);
        assert_eq!(events[0].data, "{\"a\":1}");
    }

    #[tokio::test]
    async fn lease_is_exclusive_until_it_expires() {
        let (store, _dir) = store().await;
        let name = unique("lease");
        assert!(store.acquire_lease(&name, "a", 60_000).await.unwrap());
        assert!(!store.acquire_lease(&name, "b", 60_000).await.unwrap());
        // The holder renews.
        assert!(store.acquire_lease(&name, "a", 1).await.unwrap());
        tokio::time::sleep(Duration::from_millis(20)).await;
        // Expired: another replica takes over, the former holder is out.
        assert!(store.acquire_lease(&name, "b", 60_000).await.unwrap());
        assert!(!store.acquire_lease(&name, "a", 60_000).await.unwrap());
    }

    #[tokio::test]
    async fn blob_round_trip_in_chunks() {
        let (store, _dir) = store().await;
        let bytes: Vec<u8> = (0..(CHUNK_SIZE * 2 + 17))
            .map(|i| (i % 251) as u8)
            .collect();
        let (digest, size) = store.put_blob(&bytes).await.unwrap();
        assert_eq!(size as usize, bytes.len());
        assert_eq!(store.put_blob(&bytes).await.unwrap().0, digest);
        assert_eq!(store.blob_size(&digest).await.unwrap(), Some(size));

        let mut read = Vec::new();
        let mut idx = 0;
        while let Some(chunk) = store.blob_chunk(&digest, idx).await.unwrap() {
            assert!(chunk.len() <= CHUNK_SIZE);
            read.extend_from_slice(&chunk);
            idx += 1;
        }
        assert_eq!(idx, 3);
        assert_eq!(read, bytes);
    }

    #[tokio::test]
    async fn node_token_is_stored_hashed() {
        let (store, _dir) = store().await;
        let node = unique("node");
        let token = store.enrol_node(&node, "poller").await.unwrap();
        assert_eq!(token.len(), 64);
        let hash = store.node_token_hash(&node).await.unwrap().unwrap();
        assert_eq!(hash, hash_token(&token));
        assert_ne!(hash, token);
        assert!(store.delete_node(&node).await.unwrap());
        assert!(store.node_token_hash(&node).await.unwrap().is_none());
    }
}
