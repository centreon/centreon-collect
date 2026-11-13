//! SQL store: the source of truth shared by every replica.
//!
//! One code path serves SQLite and MariaDB through the sqlx `Any` driver, so
//! statements stay portable and only use integers, strings and byte strings.
//!
//! Time: every stored timestamp, deadline, lease and liveness cutoff uses the
//! database clock, estimated as `local clock + offset` and refreshed
//! periodically, so replicas never compare their own wall clocks. Residual
//! assumption: between two refreshes the local clock drifts from the
//! database clock by much less than the liveness and lease margins (seconds),
//! and the round trip of the "now" query is short and roughly symmetric.

use std::collections::{BTreeMap, HashMap, HashSet};
use std::future::Future;
use std::sync::atomic::{AtomicI64, Ordering};
use std::sync::{Arc, Mutex, PoisonError};
use std::time::Duration;

use sha2::{Digest, Sha256};
use sqlx::any::{AnyPoolOptions, AnyRow};
use sqlx::{AnyPool, Row};
use tokio::sync::{mpsc, oneshot};
use tracing::{debug, warn};

use crate::error::Error;
use crate::metrics;

/// Size of one stored blob chunk, also the size of a `FetchBlob` frame.
pub(crate) const CHUNK_SIZE: usize = 1 << 20;

/// Sequence number of the event written by the server when a job expires.
pub(crate) const EXPIRED_SEQ: i64 = i64::MAX;
/// Sequence number of the event written when a queued job is cancelled.
pub(crate) const CANCELLED_SEQ: i64 = i64::MAX - 1;
/// Sequence number of the event written when the server fails a job itself.
pub(crate) const FAILED_SEQ: i64 = i64::MAX - 2;
/// Agent sequence numbers stay strictly below this value.
pub(crate) const SERVER_SEQ_FLOOR: i64 = FAILED_SEQ;

/// Bytes of event payload sent in one multi-row insert, well under the
/// default MariaDB `max_allowed_packet`.
const INSERT_BYTES: usize = 2 << 20;
/// Submits committed together, and their cumulated action size.
const SUBMIT_BATCH: usize = 128;
const SUBMIT_BYTES: usize = 4 << 20;
/// Submits that may wait for the group commit before submitters are held.
const SUBMIT_QUEUE: usize = 1024;
/// Rows handled per statement by the batched maintenance work.
const BATCH: i64 = 500;
/// Prerequisites one job may have.
pub(crate) const MAX_REQUIRES: usize = 16;
/// Attempts of a transaction hit by a deadlock or a busy database.
const RETRIES: u32 = 6;

/// Job states of the state machine.
pub(crate) mod state {
    pub(crate) const QUEUED: &str = "queued";
    pub(crate) const DISPATCHED: &str = "dispatched";
    pub(crate) const RUNNING: &str = "running";
    pub(crate) const SUCCEEDED: &str = "succeeded";
    pub(crate) const FAILED: &str = "failed";
    pub(crate) const EXPIRED: &str = "expired";
    pub(crate) const CANCELLED: &str = "cancelled";

    /// Shown for a prerequisite that no longer exists; never stored.
    pub(crate) const PURGED: &str = "purged";

    /// States a job can still leave.
    pub(crate) const ACTIVE: [&str; 3] = [QUEUED, DISPATCHED, RUNNING];
    /// States a job never leaves.
    pub(crate) const TERMINAL: [&str; 4] = [SUCCEEDED, FAILED, EXPIRED, CANCELLED];

    /// True when no further transition is possible.
    pub(crate) fn is_terminal(state: &str) -> bool {
        TERMINAL.contains(&state)
    }
}

fn local_now_ms() -> i64 {
    std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .map_or(0, |d| i64::try_from(d.as_millis()).unwrap_or(i64::MAX))
}

/// Lowercase hex SHA-256 of an enrolment token.
pub(crate) fn hash_token(token: &str) -> String {
    crate::hex(&Sha256::digest(token.as_bytes()))
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
    /// JSON document of what the agent last reported, when it ever did.
    pub(crate) state: Option<String>,
    /// When the reported state last changed.
    pub(crate) state_at: Option<i64>,
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
    /// Submission order within the node, assigned by the store.
    pub(crate) seq: i64,
    /// Jobs of the node that must succeed before this one is dispatched.
    pub(crate) requires: Vec<String>,
}

/// Which configuration pushes of its node a new job also requires.
#[derive(Debug, Clone, Copy, Default)]
pub(crate) enum LatestConfig<'a> {
    /// None.
    #[default]
    None,
    /// The latest push of every target.
    All,
    /// The latest push of each of these targets, when there is one.
    Targets(&'a [String]),
}

/// Job to commit.
#[derive(Debug, Clone, Copy)]
pub(crate) struct NewJob<'a> {
    pub(crate) id: &'a str,
    pub(crate) node_id: &'a str,
    pub(crate) kind: &'a str,
    pub(crate) action: &'a [u8],
    /// How long the job may live, counted on the database clock.
    pub(crate) ttl_ms: i64,
    /// Blob an `ApplyConfig` job needs, kept while the job is active.
    pub(crate) blob_digest: Option<&'a str>,
    /// JSON payload of the "accepted" event.
    pub(crate) accepted_data: &'a str,
    /// The id was chosen by the caller and may already exist: look first.
    pub(crate) may_exist: bool,
    /// Prerequisite job ids, already validated.
    pub(crate) requires: &'a [String],
    /// Latest `ApplyConfig` jobs of the node to require as well, resolved
    /// inside the submit transaction.
    pub(crate) latest_config: LatestConfig<'a>,
    /// Target of an `ApplyConfig` job, which becomes the latest one for it.
    pub(crate) config_target: Option<&'a str>,
}

/// Owned copy of a [`NewJob`], waiting for the group commit.
#[derive(Debug)]
struct QueuedJob {
    id: String,
    node_id: String,
    kind: String,
    action: Vec<u8>,
    ttl_ms: i64,
    blob_digest: Option<String>,
    accepted_data: String,
    requires: Vec<String>,
    config_target: Option<String>,
}

impl QueuedJob {
    fn as_new(&self) -> NewJob<'_> {
        NewJob {
            id: &self.id,
            node_id: &self.node_id,
            kind: &self.kind,
            action: &self.action,
            ttl_ms: self.ttl_ms,
            blob_digest: self.blob_digest.as_deref(),
            accepted_data: &self.accepted_data,
            may_exist: false,
            requires: &self.requires,
            latest_config: LatestConfig::None,
            config_target: self.config_target.as_deref(),
        }
    }
}

/// A submit and where to answer once it is committed.
#[derive(Debug)]
struct QueuedSubmit {
    job: QueuedJob,
    reply: oneshot::Sender<Result<bool, Error>>,
}

/// Queued job that will never run: one of its prerequisites did not succeed.
#[derive(Debug, Clone)]
pub(crate) struct Unmet {
    pub(crate) job_id: String,
    pub(crate) prerequisite: String,
    /// Final state of the prerequisite, or [`state::PURGED`].
    pub(crate) state: String,
}

/// Outcome of one [`Store::claim_jobs`].
#[derive(Debug, Default)]
pub(crate) struct Claim {
    /// Jobs won by the session, in submission order.
    pub(crate) jobs: Vec<JobRow>,
    /// Queued jobs read; a full batch means more may follow.
    pub(crate) listed: usize,
    /// Jobs the claim statement was asked for.
    pub(crate) tried: usize,
    /// Jobs to fail instead of dispatching them.
    pub(crate) unmet: Vec<Unmet>,
    /// The head of the queue waits for a prerequisite still in progress.
    pub(crate) blocked: bool,
}

/// What an agent event needs to know about its job.
#[derive(Debug, Clone)]
pub(crate) struct JobHead {
    pub(crate) node_id: String,
    pub(crate) state: String,
    pub(crate) attempts: i64,
}

/// Agent event to commit.
#[derive(Debug, Clone)]
pub(crate) struct NewEvent<'a> {
    pub(crate) job_id: &'a str,
    pub(crate) seq: i64,
    pub(crate) time_ms: i64,
    pub(crate) code: i64,
    /// JSON document.
    pub(crate) data: String,
}

/// State change committed with a batch of events: `running`, or a final
/// state, applied only from the states that allow it.
#[derive(Debug, Clone, Copy)]
pub(crate) struct Transition<'a> {
    pub(crate) job_id: &'a str,
    pub(crate) to: &'static str,
}

/// How to open the store.
#[derive(Debug, Clone, Copy, Default)]
pub(crate) struct StoreOptions {
    /// Pool size; a default per database kind when unset.
    pub(crate) max_connections: Option<u32>,
    /// SQLite only: `synchronous = NORMAL` instead of `FULL`.
    pub(crate) sqlite_relaxed_sync: bool,
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

/// Terminal transition written by the server, with its event.
#[derive(Debug, Clone, Copy)]
pub(crate) struct Termination<'a> {
    pub(crate) to: &'a str,
    pub(crate) seq: i64,
    pub(crate) code: i32,
    pub(crate) data: &'a str,
}

/// Handle on the shared database.
#[derive(Debug, Clone)]
pub(crate) struct Store {
    pool: AnyPool,
    dialect: Dialect,
    /// Database clock minus local clock, in milliseconds.
    clock_offset: Arc<AtomicI64>,
    /// Nodes whose submission counter row is known to exist.
    counters: Arc<Mutex<HashSet<String>>>,
    /// Queue of the group commit of submits; `None` in the worker's own copy.
    submits: Option<mpsc::Sender<QueuedSubmit>>,
}

const JOB_COLUMNS: &str = "id, node_id, kind, action, state, attempts, created_at, updated_at, \
     deadline_at, cancel_requested, seq, requires";

const NODE_COLUMNS: &str = "id, name, created_at, state, state_at";

const SESSION_COLUMNS: &str =
    "node_id, session_id, replica_id, agent_version, connected_at, last_seen";

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

/// Same as [`text`] for a nullable column.
fn opt_text(row: &AnyRow, column: &str) -> Result<Option<String>, sqlx::Error> {
    match row.try_get::<Option<String>, _>(column) {
        Ok(value) => Ok(value),
        Err(_) => {
            let bytes: Option<Vec<u8>> = row.try_get(column)?;
            bytes
                .map(String::from_utf8)
                .transpose()
                .map_err(|err| sqlx::Error::ColumnDecode {
                    index: column.to_owned(),
                    source: Box::new(err),
                })
        }
    }
}

/// Stored form of a prerequisite list: a JSON array, `NULL` when empty.
fn encode_requires(requires: &[String]) -> Option<String> {
    (!requires.is_empty()).then(|| serde_json::Value::from(requires).to_string())
}

fn node_from_row(row: &AnyRow) -> Result<NodeRow, sqlx::Error> {
    Ok(NodeRow {
        id: row.try_get("id")?,
        name: row.try_get("name")?,
        created_at: row.try_get("created_at")?,
        state: opt_text(row, "state")?,
        state_at: row.try_get("state_at")?,
    })
}

fn job_from_row(row: &AnyRow) -> Result<JobRow, sqlx::Error> {
    let requires = match opt_text(row, "requires")? {
        Some(json) => serde_json::from_str(&json).map_err(|err| sqlx::Error::ColumnDecode {
            index: "requires".to_owned(),
            source: Box::new(err),
        })?,
        None => Vec::new(),
    };
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
        seq: row.try_get("seq")?,
        requires,
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

fn strings(rows: &[AnyRow], column: &str) -> Result<Vec<String>, sqlx::Error> {
    rows.iter().map(|row| row.try_get(column)).collect()
}

/// `?, ?, ?` for `count` bound values.
fn placeholders(count: usize) -> String {
    vec!["?"; count].join(", ")
}

/// Deadlock, lock wait timeout or busy database: the statement did nothing
/// and can be replayed.
fn is_transient(err: &sqlx::Error) -> bool {
    let sqlx::Error::Database(db) = err else {
        return false;
    };
    let message = db.message();
    // 40001/1213: MariaDB deadlock; 1205: lock wait timeout; 5/6: SQLite busy/locked.
    matches!(
        db.code().as_deref(),
        Some("40001" | "1213" | "1205" | "5" | "6" | "517")
    ) || [
        "Deadlock found",
        "Lock wait timeout",
        "database is locked",
        "table is locked",
    ]
    .iter()
    .any(|needle| message.contains(needle))
}

/// Runs `operation`, again when it failed for a transient reason.
async fn with_retry<T, F, Fut>(mut operation: F) -> Result<T, sqlx::Error>
where
    F: FnMut() -> Fut,
    Fut: Future<Output = Result<T, sqlx::Error>>,
{
    let mut attempt = 0;
    loop {
        match operation().await {
            Err(err) if attempt + 1 < RETRIES && is_transient(&err) => {
                attempt += 1;
                debug!(attempt, error = %err, "transient store error, retrying");
                tokio::time::sleep(Duration::from_millis(10 * u64::from(attempt * attempt))).await;
            }
            other => return other,
        }
    }
}

impl Store {
    /// [`Self::connect_with`] and the default options.
    #[cfg(test)]
    pub(crate) async fn connect(url: &str) -> Result<Self, Error> {
        // Small pool: many test stores share one database server.
        let settings = StoreOptions {
            max_connections: Some(4),
            ..StoreOptions::default()
        };
        Self::connect_with(url, settings).await
    }

    /// Connects to `url` (`sqlite:` or `mysql:`/`mariadb:`) and creates the schema.
    pub(crate) async fn connect_with(url: &str, settings: StoreOptions) -> Result<Self, Error> {
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
        let built_in = match dialect {
            Dialect::Sqlite => cfg!(feature = "sqlite"),
            Dialect::MySql => cfg!(feature = "mysql"),
        };
        if !built_in {
            return Err(Error::Invalid(
                "this build has no driver for the database named by database_url".into(),
            ));
        }
        let mut options = AnyPoolOptions::new().acquire_timeout(Duration::from_secs(15));
        options = match dialect {
            // Few connections: SQLite has a single writer anyway.
            Dialect::Sqlite => {
                // FULL syncs the log at every commit: a committed job survives
                // a power loss. NORMAL only syncs at checkpoints.
                let synchronous = if settings.sqlite_relaxed_sync {
                    "PRAGMA synchronous = NORMAL"
                } else {
                    "PRAGMA synchronous = FULL"
                };
                options
                    .max_connections(settings.max_connections.unwrap_or(4))
                    .after_connect(move |conn, _| {
                        Box::pin(async move {
                            // WAL lets readers proceed while another replica writes.
                            for pragma in [
                                "PRAGMA busy_timeout = 15000",
                                "PRAGMA journal_mode = WAL",
                                synchronous,
                            ] {
                                sqlx::query(pragma).execute(&mut *conn).await?;
                            }
                            Ok(())
                        })
                    })
            }
            Dialect::MySql => options.max_connections(settings.max_connections.unwrap_or(16)),
        };
        let mut store = Self {
            pool: options.connect(url).await?,
            dialect,
            clock_offset: Arc::default(),
            counters: Arc::default(),
            submits: None,
        };
        store.migrate().await?;
        store.sync_clock().await?;
        // The worker ends when every handle on the store is dropped.
        let (submits, queue) = mpsc::channel(SUBMIT_QUEUE);
        tokio::spawn(store.clone().group_commit(queue));
        store.submits = Some(submits);
        Ok(store)
    }

    async fn migrate(&self) -> Result<(), Error> {
        let (text, blob) = match self.dialect {
            Dialect::Sqlite => ("TEXT", "BLOB"),
            Dialect::MySql => ("LONGTEXT", "MEDIUMBLOB"),
        };
        let tables = [
            format!(
                "CREATE TABLE IF NOT EXISTS nodes (
                id VARCHAR(191) NOT NULL PRIMARY KEY,
                name VARCHAR(255) NOT NULL,
                token_hash VARCHAR(64) NOT NULL,
                created_at BIGINT NOT NULL,
                state {text} NULL,
                state_at BIGINT NULL)"
            ),
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
                cancel_requested BIGINT NOT NULL DEFAULT 0,
                seq BIGINT NOT NULL DEFAULT 0,
                blob_digest VARCHAR(64) NULL,
                cancel_sent_session VARCHAR(191) NULL,
                requires {text} NULL)"
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
            // Per-node counter giving jobs their submission order.
            "CREATE TABLE IF NOT EXISTS node_seq (
                node_id VARCHAR(191) NOT NULL PRIMARY KEY,
                next_seq BIGINT NOT NULL)"
                .to_owned(),
            // Latest `ApplyConfig` job of each target of a node.
            "CREATE TABLE IF NOT EXISTS node_config (
                node_id VARCHAR(191) NOT NULL,
                target VARCHAR(191) NOT NULL,
                job_id VARCHAR(191) NOT NULL,
                PRIMARY KEY (node_id, target))"
                .to_owned(),
        ];
        for statement in &tables {
            sqlx::query(statement).execute(&self.pool).await?;
        }
        // Columns added after the first schema: stores created before lack them.
        let nullable_text = format!("{text} NULL");
        for (table, column, definition) in [
            ("jobs", "cancel_requested", "BIGINT NOT NULL DEFAULT 0"),
            ("jobs", "seq", "BIGINT NOT NULL DEFAULT 0"),
            ("jobs", "blob_digest", "VARCHAR(64) NULL"),
            ("jobs", "cancel_sent_session", "VARCHAR(191) NULL"),
            ("jobs", "requires", nullable_text.as_str()),
            ("nodes", "state", nullable_text.as_str()),
            ("nodes", "state_at", "BIGINT NULL"),
        ] {
            let probe = format!("SELECT {column} FROM {table} LIMIT 1");
            if sqlx::query(&probe).fetch_optional(&self.pool).await.is_ok() {
                continue;
            }
            let alter = format!("ALTER TABLE {table} ADD COLUMN {column} {definition}");
            if let Err(err) = sqlx::query(&alter).execute(&self.pool).await {
                // Another replica starting at the same time may have added
                // it: only a column still missing is an error.
                if sqlx::query(&probe)
                    .fetch_optional(&self.pool)
                    .await
                    .is_err()
                {
                    return Err(err.into());
                }
            }
        }
        for (name, target) in [
            // Queued jobs of a node in submission order (drain).
            ("idx_jobs_node_state_seq", "jobs (node_id, state, seq)"),
            // Expiry by deadline.
            ("idx_jobs_state_deadline", "jobs (state, deadline_at)"),
            // Purge by age.
            ("idx_jobs_state_updated", "jobs (state, updated_at)"),
            // Blob still needed by an active job.
            ("idx_jobs_blob", "jobs (blob_digest)"),
            ("idx_blobs_created", "blobs (created_at)"),
            ("idx_sessions_session", "sessions (session_id)"),
        ] {
            let create = format!("CREATE INDEX IF NOT EXISTS {name} ON {target}");
            if sqlx::query(&create).execute(&self.pool).await.is_err() {
                // MySQL has no `IF NOT EXISTS` here: a failure of the plain
                // form then means the index is already there.
                let plain = format!("CREATE INDEX {name} ON {target}");
                if let Err(err) = sqlx::query(&plain).execute(&self.pool).await {
                    debug!(index = name, error = %err, "index not created");
                }
            }
        }
        Ok(())
    }

    fn insert_ignore(&self) -> &'static str {
        match self.dialect {
            Dialect::Sqlite => "INSERT OR IGNORE",
            Dialect::MySql => "INSERT IGNORE",
        }
    }

    // ---- time ----

    /// Current time on the database clock, Unix milliseconds.
    pub(crate) fn now_ms(&self) -> i64 {
        local_now_ms().saturating_add(self.clock_offset.load(Ordering::Relaxed))
    }

    /// Re-estimates the offset between the database clock and the local one.
    pub(crate) async fn sync_clock(&self) -> Result<(), Error> {
        // SQLite runs in-process: its clock is the local clock.
        if self.dialect == Dialect::Sqlite {
            return Ok(());
        }
        let before = local_now_ms();
        let row = sqlx::query("SELECT CAST(UNIX_TIMESTAMP(NOW(3)) * 1000 AS SIGNED) AS now_ms")
            .fetch_one(&self.pool)
            .await?;
        let after = local_now_ms();
        let database: i64 = row.try_get("now_ms")?;
        // The database read its clock about half-way through the round trip.
        let offset = database - (before + (after - before) / 2);
        self.clock_offset.store(offset, Ordering::Relaxed);
        Ok(())
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

    /// Identifier shared by every replica of this store, created once.
    pub(crate) async fn server_id(&self) -> Result<String, Error> {
        let sql = format!(
            "{} INTO meta (name, value) VALUES (?, ?)",
            self.insert_ignore()
        );
        // Concurrent replicas: the first insert wins, all read the same row.
        sqlx::query(&sql)
            .bind("server_id")
            .bind(crate::random_id()?)
            .execute(&self.pool)
            .await?;
        let row = sqlx::query("SELECT value FROM meta WHERE name = ?")
            .bind("server_id")
            .fetch_one(&self.pool)
            .await?;
        Ok(row.try_get("value")?)
    }

    // ---- nodes ----

    /// Enrols (or re-enrols) a node and returns its new token, shown once.
    /// Its session row is removed so that a session opened with a former
    /// token dies on whichever replica holds it.
    pub(crate) async fn enrol_node(&self, id: &str, name: &str) -> Result<String, Error> {
        let token = crate::random_hex::<32>()?;
        sqlx::query("REPLACE INTO nodes (id, name, token_hash, created_at) VALUES (?, ?, ?, ?)")
            .bind(id)
            .bind(name)
            .bind(hash_token(&token))
            .bind(self.now_ms())
            .execute(&self.pool)
            .await?;
        sqlx::query("DELETE FROM sessions WHERE node_id = ?")
            .bind(id)
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
            .bind(self.now_ms())
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
        for table in ["sessions", "node_config"] {
            let sql = format!("DELETE FROM {table} WHERE node_id = ?");
            sqlx::query(&sql).bind(id).execute(&self.pool).await?;
        }
        let done = sqlx::query("DELETE FROM nodes WHERE id = ?")
            .bind(id)
            .execute(&self.pool)
            .await?;
        Ok(done.rows_affected() > 0)
    }

    /// All enrolled nodes.
    pub(crate) async fn list_nodes(&self) -> Result<Vec<NodeRow>, Error> {
        let sql = format!("SELECT {NODE_COLUMNS} FROM nodes ORDER BY id");
        let rows = sqlx::query(&sql).fetch_all(&self.pool).await?;
        Ok(rows.iter().map(node_from_row).collect::<Result<_, _>>()?)
    }

    /// One enrolled node.
    pub(crate) async fn node(&self, id: &str) -> Result<Option<NodeRow>, Error> {
        let sql = format!("SELECT {NODE_COLUMNS} FROM nodes WHERE id = ?");
        let row = sqlx::query(&sql)
            .bind(id)
            .fetch_optional(&self.pool)
            .await?;
        Ok(row.as_ref().map(node_from_row).transpose()?)
    }

    /// Records what the agent of a node reports (JSON document). Called on
    /// change only, by the replica holding the session.
    pub(crate) async fn set_node_state(&self, id: &str, state: &str) -> Result<(), Error> {
        sqlx::query("UPDATE nodes SET state = ?, state_at = ? WHERE id = ?")
            .bind(state)
            .bind(self.now_ms())
            .bind(id)
            .execute(&self.pool)
            .await?;
        Ok(())
    }

    // ---- replicas ----

    /// Records that `id` is alive and reachable by its peers at `peer_addr`.
    pub(crate) async fn heartbeat_replica(&self, id: &str, peer_addr: &str) -> Result<(), Error> {
        sqlx::query("REPLACE INTO replicas (id, peer_addr, last_seen) VALUES (?, ?, ?)")
            .bind(id)
            .bind(peer_addr)
            .bind(self.now_ms())
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

    /// Peer addresses of every known replica.
    pub(crate) async fn replica_addrs(&self) -> Result<Vec<String>, Error> {
        let rows = sqlx::query("SELECT peer_addr FROM replicas")
            .fetch_all(&self.pool)
            .await?;
        Ok(strings(&rows, "peer_addr")?)
    }

    /// Removes the sessions recorded for a replica: on graceful shutdown, and
    /// at startup when a former incarnation with the same id left some.
    pub(crate) async fn remove_replica_sessions(&self, replica_id: &str) -> Result<u64, Error> {
        let done = sqlx::query("DELETE FROM sessions WHERE replica_id = ?")
            .bind(replica_id)
            .execute(&self.pool)
            .await?;
        Ok(done.rows_affected())
    }

    /// Removes a replica and the sessions it held (graceful shutdown).
    pub(crate) async fn remove_replica(&self, id: &str) -> Result<(), Error> {
        self.remove_replica_sessions(id).await?;
        sqlx::query("DELETE FROM replicas WHERE id = ?")
            .bind(id)
            .execute(&self.pool)
            .await?;
        Ok(())
    }

    /// Drops replicas silent for `dead_after_ms` and every session of an
    /// unknown replica. Returns the number of sessions dropped.
    pub(crate) async fn drop_dead_replicas(&self, dead_after_ms: i64) -> Result<u64, Error> {
        sqlx::query("DELETE FROM replicas WHERE last_seen < ?")
            .bind(self.now_ms().saturating_sub(dead_after_ms))
            .execute(&self.pool)
            .await?;
        // Two reads then deletes by key: no subquery on a table being written.
        let holders = sqlx::query("SELECT DISTINCT replica_id FROM sessions")
            .fetch_all(&self.pool)
            .await?;
        let alive = sqlx::query("SELECT id FROM replicas")
            .fetch_all(&self.pool)
            .await?;
        let alive = strings(&alive, "id")?;
        let mut dropped = 0;
        for holder in strings(&holders, "replica_id")? {
            if !alive.contains(&holder) {
                dropped += self.remove_replica_sessions(&holder).await?;
            }
        }
        Ok(dropped)
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
        let now = self.now_ms();
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
                .bind(self.now_ms())
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
        let sql = format!("SELECT {SESSION_COLUMNS} FROM sessions WHERE node_id = ?");
        let row = sqlx::query(&sql)
            .bind(node_id)
            .fetch_optional(&self.pool)
            .await?;
        Ok(row.as_ref().map(session_from_row).transpose()?)
    }

    /// Every session, all replicas included.
    pub(crate) async fn list_sessions(&self) -> Result<Vec<SessionRow>, Error> {
        let sql = format!("SELECT {SESSION_COLUMNS} FROM sessions ORDER BY node_id");
        let rows = sqlx::query(&sql).fetch_all(&self.pool).await?;
        Ok(rows
            .iter()
            .map(session_from_row)
            .collect::<Result<_, _>>()?)
    }

    // ---- jobs ----

    /// Commits a new `queued` job together with its "accepted" event (seq 0)
    /// and its rank in the node's submission order, in one transaction.
    /// Returns false, changing nothing, when a job with this id already
    /// exists.
    ///
    /// Submits made at the same time share one transaction (group commit):
    /// nothing waits for more submits, the transaction just takes those that
    /// queued up while the previous one was committing.
    pub(crate) async fn insert_job(&self, job: NewJob<'_>) -> Result<bool, Error> {
        // Prerequisites resolved in the transaction need one of their own.
        let alone = job.may_exist || !matches!(job.latest_config, LatestConfig::None);
        let Some(submits) = self.submits.as_ref().filter(|_| !alone) else {
            return self.insert_job_alone(job).await;
        };
        let (reply, committed) = oneshot::channel();
        let queued = QueuedSubmit {
            job: QueuedJob {
                id: job.id.to_owned(),
                node_id: job.node_id.to_owned(),
                kind: job.kind.to_owned(),
                action: job.action.to_vec(),
                ttl_ms: job.ttl_ms,
                blob_digest: job.blob_digest.map(str::to_owned),
                accepted_data: job.accepted_data.to_owned(),
                requires: job.requires.to_vec(),
                config_target: job.config_target.map(str::to_owned),
            },
            reply,
        };
        let closed = || Error::Invalid("the store is closing".into());
        submits.send(queued).await.map_err(|_| closed())?;
        committed.await.map_err(|_| closed())?
    }

    /// Commits the submits as they queue up, many per transaction under load.
    async fn group_commit(self, mut queue: mpsc::Receiver<QueuedSubmit>) {
        while let Some(first) = queue.recv().await {
            let mut bytes = first.job.action.len();
            let mut batch = vec![first];
            // Whatever is already there, never waiting for more.
            while batch.len() < SUBMIT_BATCH && bytes < SUBMIT_BYTES {
                let Ok(next) = queue.try_recv() else {
                    break;
                };
                bytes += next.job.action.len();
                batch.push(next);
            }
            metrics::SUBMIT_BATCH_SIZE.record(batch.len() as u64);
            if batch.len() > 1 {
                let jobs: Vec<NewJob<'_>> = batch.iter().map(|q| q.job.as_new()).collect();
                match self.insert_jobs(&jobs).await {
                    Ok(()) => {
                        for queued in batch {
                            // A submitter that went away needs no answer.
                            let _ = queued.reply.send(Ok(true));
                        }
                        continue;
                    }
                    // One by one: a bad job must not fail the others.
                    Err(err) => {
                        warn!(jobs = batch.len(), error = %err, "submit batch not committed, retrying one by one")
                    }
                }
            }
            for queued in batch {
                let outcome = self.insert_job_alone(queued.job.as_new()).await;
                let _ = queued.reply.send(outcome);
            }
        }
    }

    /// Creates the submission counter row of the nodes that lack one.
    async fn ensure_counters(&self, nodes: &[&str]) -> Result<(), Error> {
        for node_id in nodes {
            let known = self
                .counters
                .lock()
                .unwrap_or_else(PoisonError::into_inner)
                .contains(*node_id);
            if known {
                continue;
            }
            // Outside the transaction: two submits creating the counter row
            // inside theirs would deadlock on it. Once per node and process.
            let sql = format!(
                "{} INTO node_seq (node_id, next_seq) VALUES (?, 0)",
                self.insert_ignore()
            );
            sqlx::query(&sql).bind(*node_id).execute(&self.pool).await?;
            self.counters
                .lock()
                .unwrap_or_else(PoisonError::into_inner)
                .insert((*node_id).to_owned());
        }
        Ok(())
    }

    async fn insert_job_alone(&self, job: NewJob<'_>) -> Result<bool, Error> {
        if job.may_exist && self.job(job.id).await?.is_some() {
            return Ok(false);
        }
        self.ensure_counters(&[job.node_id]).await?;
        match with_retry(|| self.insert_job_tx(job)).await {
            Ok(true) => Ok(true),
            Ok(false) => Err(Error::Invalid(format!(
                "more than {MAX_REQUIRES} prerequisites"
            ))),
            // Lost a race against the same id: it exists, which is the goal.
            Err(sqlx::Error::Database(err)) if job.may_exist && err.is_unique_violation() => {
                Ok(false)
            }
            Err(err) => Err(err.into()),
        }
    }

    /// Commits several new jobs, their "accepted" events and their ranks in
    /// ONE transaction. Jobs of a node keep the order of `jobs`.
    async fn insert_jobs(&self, jobs: &[NewJob<'_>]) -> Result<(), Error> {
        // Sorted: every transaction locks the counter rows in the same order.
        let mut counts: BTreeMap<&str, i64> = BTreeMap::new();
        for job in jobs {
            *counts.entry(job.node_id).or_default() += 1;
        }
        let nodes: Vec<&str> = counts.keys().copied().collect();
        self.ensure_counters(&nodes).await?;
        Ok(with_retry(|| self.insert_jobs_tx(jobs, &counts)).await?)
    }

    async fn insert_jobs_tx(
        &self,
        jobs: &[NewJob<'_>],
        counts: &BTreeMap<&str, i64>,
    ) -> Result<(), sqlx::Error> {
        let now = self.now_ms();
        let mut tx = self.pool.begin().await?;
        // The counter row locks order the submits of each node: the rank
        // order is the commit order, whatever the clocks say.
        for (node_id, count) in counts {
            sqlx::query("UPDATE node_seq SET next_seq = next_seq + ? WHERE node_id = ?")
                .bind(*count)
                .bind(*node_id)
                .execute(&mut *tx)
                .await?;
        }
        // Rank of a job: the new counter value minus the jobs of its node
        // that follow it in the batch.
        let mut left = counts.clone();
        let mut rest = jobs;
        while !rest.is_empty() {
            let mut take = 0;
            let mut bytes = 0;
            while take < rest.len() && take < 50 && (take == 0 || bytes < INSERT_BYTES) {
                bytes += rest[take].action.len();
                take += 1;
            }
            let (chunk, tail) = rest.split_at(take);
            rest = tail;
            let row = "(?, ?, ?, ?, ?, 0, NULL, ?, ?, ?, 0, \
                       (SELECT next_seq FROM node_seq WHERE node_id = ?) - ?, ?, ?)";
            let sql = format!(
                "INSERT INTO jobs
                 (id, node_id, kind, action, state, attempts, session_id, created_at, updated_at,
                  deadline_at, cancel_requested, seq, blob_digest, requires)
                 VALUES {}",
                vec![row; chunk.len()].join(", ")
            );
            let mut insert = sqlx::query(&sql);
            for job in chunk {
                let following = left.get_mut(job.node_id).map_or(0, |count| {
                    *count -= 1;
                    *count
                });
                insert = insert
                    .bind(job.id)
                    .bind(job.node_id)
                    .bind(job.kind)
                    .bind(job.action)
                    .bind(state::QUEUED)
                    .bind(now)
                    .bind(now)
                    .bind(now.saturating_add(job.ttl_ms))
                    .bind(job.node_id)
                    .bind(following)
                    .bind(job.blob_digest)
                    .bind(encode_requires(job.requires));
            }
            insert.execute(&mut *tx).await?;
            // In batch order: the last push of a target is the latest one.
            for job in chunk {
                if let Some(target) = job.config_target {
                    Self::set_latest_config(&mut tx, job.node_id, target, job.id).await?;
                }
            }

            let sql = format!(
                "INSERT INTO job_events (job_id, seq, time_ms, code, data) VALUES {}",
                vec!["(?, 0, ?, ?, ?)"; chunk.len()].join(", ")
            );
            let mut insert = sqlx::query(&sql);
            for job in chunk {
                insert = insert
                    .bind(job.id)
                    .bind(now)
                    .bind(i64::from(gorgone_core::legacy::ACTION_BEGIN))
                    .bind(job.accepted_data);
            }
            insert.execute(&mut *tx).await?;
        }
        tx.commit().await
    }

    /// Makes `job_id` the latest `ApplyConfig` job of `target` on the node.
    /// The caller holds the counter row of the node, which orders the pushes.
    async fn set_latest_config(
        tx: &mut sqlx::Transaction<'_, sqlx::Any>,
        node_id: &str,
        target: &str,
        job_id: &str,
    ) -> Result<(), sqlx::Error> {
        sqlx::query("REPLACE INTO node_config (node_id, target, job_id) VALUES (?, ?, ?)")
            .bind(node_id)
            .bind(target)
            .bind(job_id)
            .execute(&mut **tx)
            .await?;
        Ok(())
    }

    /// False, committing nothing, when the prerequisites resolved here make
    /// the job exceed [`MAX_REQUIRES`].
    async fn insert_job_tx(&self, job: NewJob<'_>) -> Result<bool, sqlx::Error> {
        let now = self.now_ms();
        let mut tx = self.pool.begin().await?;
        // The counter row lock orders the submits of a node: the rank order
        // is the commit order, whatever the clocks say.
        sqlx::query("UPDATE node_seq SET next_seq = next_seq + 1 WHERE node_id = ?")
            .bind(job.node_id)
            .execute(&mut *tx)
            .await?;
        let mut requires = encode_requires(job.requires);
        let targets = match job.latest_config {
            LatestConfig::None | LatestConfig::Targets([]) => None,
            LatestConfig::All => Some(&[][..]),
            LatestConfig::Targets(targets) => Some(targets),
        };
        if let Some(targets) = targets {
            // Under the counter lock: no push of the node commits meanwhile.
            // A purged push is not required: it cannot be judged any more.
            let only = if targets.is_empty() {
                String::new()
            } else {
                format!(" AND c.target IN ({})", placeholders(targets.len()))
            };
            let sql = format!(
                "SELECT c.job_id FROM node_config c
                 JOIN jobs j ON j.id = c.job_id AND j.node_id = c.node_id
                 WHERE c.node_id = ?{only} ORDER BY c.target"
            );
            let mut select = sqlx::query(&sql).bind(job.node_id);
            for target in targets {
                select = select.bind(target);
            }
            let rows = select.fetch_all(&mut *tx).await?;
            let mut all = job.requires.to_vec();
            for id in strings(&rows, "job_id")? {
                if !all.contains(&id) {
                    all.push(id);
                }
            }
            if all.len() > MAX_REQUIRES {
                return Ok(false);
            }
            requires = encode_requires(&all);
        }
        sqlx::query(
            "INSERT INTO jobs
             (id, node_id, kind, action, state, attempts, session_id, created_at, updated_at,
              deadline_at, cancel_requested, seq, blob_digest, requires)
             VALUES (?, ?, ?, ?, ?, 0, NULL, ?, ?, ?, 0,
                     (SELECT next_seq FROM node_seq WHERE node_id = ?), ?, ?)",
        )
        .bind(job.id)
        .bind(job.node_id)
        .bind(job.kind)
        .bind(job.action)
        .bind(state::QUEUED)
        .bind(now)
        .bind(now)
        .bind(now.saturating_add(job.ttl_ms))
        // Read in the statement itself: one round trip less.
        .bind(job.node_id)
        .bind(job.blob_digest)
        .bind(requires)
        .execute(&mut *tx)
        .await?;
        if let Some(target) = job.config_target {
            Self::set_latest_config(&mut tx, job.node_id, target, job.id).await?;
        }
        sqlx::query(
            "INSERT INTO job_events (job_id, seq, time_ms, code, data) VALUES (?, 0, ?, ?, ?)",
        )
        .bind(job.id)
        .bind(now)
        .bind(i64::from(gorgone_core::legacy::ACTION_BEGIN))
        .bind(job.accepted_data)
        .execute(&mut *tx)
        .await?;
        tx.commit().await?;
        Ok(true)
    }

    /// True when the node is enrolled.
    pub(crate) async fn node_exists(&self, id: &str) -> Result<bool, Error> {
        Ok(self.node_token_hash(id).await?.is_some())
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

    /// State and number of events of the given jobs, for change detection.
    pub(crate) async fn job_versions(
        &self,
        ids: &[String],
    ) -> Result<Vec<(String, String, i64)>, Error> {
        let mut versions = Vec::with_capacity(ids.len());
        for chunk in ids.chunks(100) {
            let sql = format!(
                "SELECT j.id, j.state,
                        (SELECT COUNT(*) FROM job_events e WHERE e.job_id = j.id) AS events
                 FROM jobs j WHERE j.id IN ({})",
                placeholders(chunk.len())
            );
            let mut query = sqlx::query(&sql);
            for id in chunk {
                query = query.bind(id);
            }
            for row in query.fetch_all(&self.pool).await? {
                versions.push((
                    row.try_get("id")?,
                    row.try_get("state")?,
                    row.try_get("events")?,
                ));
            }
        }
        Ok(versions)
    }

    /// First `queued` jobs of a node in submission order, skipping those
    /// past their deadline or being cancelled.
    pub(crate) async fn queued_jobs(
        &self,
        node_id: &str,
        limit: i64,
    ) -> Result<Vec<JobRow>, Error> {
        let sql = format!(
            "SELECT {JOB_COLUMNS} FROM jobs
             WHERE node_id = ? AND state = ? AND deadline_at > ? AND cancel_requested = 0
             ORDER BY seq LIMIT ?"
        );
        let rows = sqlx::query(&sql)
            .bind(node_id)
            .bind(state::QUEUED)
            .bind(self.now_ms())
            .bind(limit)
            .fetch_all(&self.pool)
            .await?;
        Ok(rows.iter().map(job_from_row).collect::<Result<_, _>>()?)
    }

    /// Compare-and-set `queued → dispatched`, valid only while `session_id`
    /// is still the session of the node. True when this caller won the job.
    #[cfg(test)]
    pub(crate) async fn claim_job(
        &self,
        id: &str,
        node_id: &str,
        session_id: &str,
    ) -> Result<bool, Error> {
        let done = with_retry(|| {
            sqlx::query(
                "UPDATE jobs SET state = ?, session_id = ?, attempts = attempts + 1, updated_at = ?
                 WHERE id = ? AND state = ?
                   AND EXISTS (SELECT 1 FROM sessions WHERE node_id = ? AND session_id = ?)",
            )
            .bind(state::DISPATCHED)
            .bind(session_id)
            .bind(self.now_ms())
            .bind(id)
            .bind(state::QUEUED)
            .bind(node_id)
            .bind(session_id)
            .execute(&self.pool)
        })
        .await?;
        Ok(done.rows_affected() == 1)
    }

    /// Splits queued jobs, in submission order, on their prerequisites:
    /// those to claim, those to fail, and whether the walk stopped at a job
    /// whose prerequisite is still in progress (nothing after it may go).
    async fn ready(&self, jobs: Vec<JobRow>) -> Result<(Vec<JobRow>, Vec<Unmet>, bool), Error> {
        // Common case: no prerequisite, no extra read.
        if jobs.iter().all(|job| job.requires.is_empty()) {
            return Ok((jobs, Vec::new(), false));
        }
        let mut required: Vec<&str> = jobs
            .iter()
            .flat_map(|job| job.requires.iter().map(String::as_str))
            .collect();
        required.sort_unstable();
        required.dedup();
        // Read before the claim: `succeeded` is final, so it still holds then.
        let mut states: HashMap<String, String> = self
            .job_heads(&required)
            .await?
            .into_iter()
            .map(|(id, head)| (id, head.state))
            .collect();
        let mut ready = Vec::with_capacity(jobs.len());
        let mut unmet = Vec::new();
        for job in jobs {
            let mut failure = None;
            let mut waiting = false;
            for prerequisite in &job.requires {
                match states.get(prerequisite).map(String::as_str) {
                    Some(state::SUCCEEDED) => {}
                    Some(state) if state::is_terminal(state) => {
                        failure.get_or_insert((prerequisite, state));
                    }
                    Some(_) => waiting = true,
                    None => {
                        failure.get_or_insert((prerequisite, state::PURGED));
                    }
                }
            }
            if let Some((prerequisite, state)) = failure {
                unmet.push(Unmet {
                    job_id: job.id.clone(),
                    prerequisite: prerequisite.clone(),
                    state: state.to_owned(),
                });
                // Jobs of this batch requiring it fail in the same walk.
                states.insert(job.id, state::FAILED.to_owned());
            } else if waiting {
                return Ok((ready, unmet, true));
            } else {
                ready.push(job);
            }
        }
        Ok((ready, unmet, false))
    }

    /// Claims, in one statement, up to `limit` queued jobs of the node in
    /// submission order for `session_id`, which must still be the session
    /// of the node. Jobs tried but not won were taken or cancelled meanwhile,
    /// or the session is not current any more. A job is only claimed once
    /// its prerequisites succeeded; see [`Claim`] for the other cases.
    pub(crate) async fn claim_jobs(
        &self,
        node_id: &str,
        session_id: &str,
        limit: i64,
    ) -> Result<Claim, Error> {
        let queued = self.queued_jobs(node_id, limit).await?;
        let listed = queued.len();
        let (mut jobs, unmet, blocked) = self.ready(queued).await?;
        let tried = jobs.len();
        let mut claim = Claim {
            jobs: Vec::new(),
            listed,
            tried,
            unmet,
            blocked,
        };
        if jobs.is_empty() {
            return Ok(claim);
        }
        let sql = format!(
            "UPDATE jobs SET state = ?, session_id = ?, attempts = attempts + 1, updated_at = ?
             WHERE id IN ({}) AND state = ? AND cancel_requested = 0
               AND EXISTS (SELECT 1 FROM sessions WHERE node_id = ? AND session_id = ?)",
            placeholders(tried)
        );
        let done = with_retry(|| {
            let mut update = sqlx::query(&sql)
                .bind(state::DISPATCHED)
                .bind(session_id)
                .bind(self.now_ms());
            for job in &jobs {
                update = update.bind(&job.id);
            }
            update
                .bind(state::QUEUED)
                .bind(node_id)
                .bind(session_id)
                .execute(&self.pool)
        })
        .await?;
        if done.rows_affected() as usize != tried {
            // Rare: keep only what this session really got.
            let sql = format!(
                "SELECT id FROM jobs WHERE id IN ({}) AND state = ? AND session_id = ?",
                placeholders(tried)
            );
            let mut select = sqlx::query(&sql);
            for job in &jobs {
                select = select.bind(&job.id);
            }
            let rows = select
                .bind(state::DISPATCHED)
                .bind(session_id)
                .fetch_all(&self.pool)
                .await?;
            let won: HashSet<String> = strings(&rows, "id")?.into_iter().collect();
            jobs.retain(|job| won.contains(&job.id));
        }
        claim.jobs = jobs;
        Ok(claim)
    }

    /// Node, state and attempts of the given jobs, without their payload.
    pub(crate) async fn job_heads(&self, ids: &[&str]) -> Result<HashMap<String, JobHead>, Error> {
        let mut heads = HashMap::with_capacity(ids.len());
        for chunk in ids.chunks(100) {
            let sql = format!(
                "SELECT id, node_id, state, attempts FROM jobs WHERE id IN ({})",
                placeholders(chunk.len())
            );
            let mut query = sqlx::query(&sql);
            for id in chunk {
                query = query.bind(*id);
            }
            for row in query.fetch_all(&self.pool).await? {
                heads.insert(
                    row.try_get("id")?,
                    JobHead {
                        node_id: row.try_get("node_id")?,
                        state: row.try_get("state")?,
                        attempts: row.try_get("attempts")?,
                    },
                );
            }
        }
        Ok(heads)
    }

    /// Commits a batch of agent events and the job state changes they imply
    /// in ONE transaction. A repeated `(job_id, seq)` is ignored; the batch
    /// fails as a whole if an event is neither inserted nor already there.
    /// Returns how many jobs moved to each target state.
    pub(crate) async fn commit_events(
        &self,
        events: &[NewEvent<'_>],
        transitions: &[Transition<'_>],
    ) -> Result<Vec<(&'static str, u64)>, Error> {
        Ok(with_retry(|| self.commit_events_tx(events, transitions)).await?)
    }

    async fn commit_events_tx(
        &self,
        events: &[NewEvent<'_>],
        transitions: &[Transition<'_>],
    ) -> Result<Vec<(&'static str, u64)>, sqlx::Error> {
        let now = self.now_ms();
        let mut tx = self.pool.begin().await?;
        let mut inserted = 0;
        let mut rest = events;
        while !rest.is_empty() {
            // Multi-row insert, bounded in rows and in bytes.
            let mut take = 0;
            let mut bytes = 0;
            while take < rest.len() && take < 100 && (take == 0 || bytes < INSERT_BYTES) {
                bytes += rest[take].data.len();
                take += 1;
            }
            let (chunk, tail) = rest.split_at(take);
            rest = tail;
            let sql = format!(
                "{} INTO job_events (job_id, seq, time_ms, code, data) VALUES {}",
                self.insert_ignore(),
                vec!["(?, ?, ?, ?, ?)"; chunk.len()].join(", ")
            );
            let mut insert = sqlx::query(&sql);
            for event in chunk {
                insert = insert
                    .bind(event.job_id)
                    .bind(event.seq)
                    .bind(event.time_ms)
                    .bind(event.code)
                    .bind(event.data.as_str());
            }
            inserted += insert.execute(&mut *tx).await?.rows_affected();
        }
        if inserted != events.len() as u64 {
            // Some rows were ignored: each must be a real duplicate.
            for event in events {
                let stored: i64 = sqlx::query(
                    "SELECT COUNT(*) AS n FROM job_events WHERE job_id = ? AND seq = ?",
                )
                .bind(event.job_id)
                .bind(event.seq)
                .fetch_one(&mut *tx)
                .await?
                .try_get("n")?;
                if stored == 0 {
                    return Err(sqlx::Error::Protocol(format!(
                        "event {} of job {} was not stored",
                        event.seq, event.job_id
                    )));
                }
            }
        }
        // The last change asked for a job is the one that counts (a final
        // state after `running`); then one statement per target state.
        let mut targets: HashMap<&str, &'static str> = HashMap::new();
        for transition in transitions {
            targets.insert(transition.job_id, transition.to);
        }
        let mut by_state: BTreeMap<&'static str, Vec<&str>> = BTreeMap::new();
        for (job_id, to) in targets {
            by_state.entry(to).or_default().push(job_id);
        }
        let mut applied = Vec::with_capacity(by_state.len());
        for (to, mut job_ids) in by_state {
            // `running` only from before it; a final state from any active one.
            let from: &[&str] = if to == state::RUNNING {
                &[state::QUEUED, state::DISPATCHED]
            } else {
                &state::ACTIVE
            };
            // Same lock order in every transaction.
            job_ids.sort_unstable();
            let mut changed = 0;
            for chunk in job_ids.chunks(200) {
                let sql = format!(
                    "UPDATE jobs SET state = ?, updated_at = ? WHERE id IN ({}) AND state IN ({})",
                    placeholders(chunk.len()),
                    placeholders(from.len())
                );
                let mut update = sqlx::query(&sql).bind(to).bind(now);
                for job_id in chunk {
                    update = update.bind(*job_id);
                }
                for state in from {
                    update = update.bind(*state);
                }
                changed += update.execute(&mut *tx).await?.rows_affected();
            }
            applied.push((to, changed));
        }
        tx.commit().await?;
        Ok(applied)
    }

    /// Gives back a claimed job that could not be pushed to its session.
    pub(crate) async fn unclaim_job(&self, id: &str, session_id: &str) -> Result<bool, Error> {
        let done = sqlx::query(
            "UPDATE jobs SET state = ?, session_id = NULL, updated_at = ?
             WHERE id = ? AND state = ? AND session_id = ?",
        )
        .bind(state::QUEUED)
        .bind(self.now_ms())
        .bind(id)
        .bind(state::DISPATCHED)
        .bind(session_id)
        .execute(&self.pool)
        .await?;
        Ok(done.rows_affected() == 1)
    }

    /// Sets the final state of a job that is not terminal yet.
    pub(crate) async fn finish_job(&self, id: &str, final_state: &str) -> Result<bool, Error> {
        let done = sqlx::query(
            "UPDATE jobs SET state = ?, updated_at = ? WHERE id = ? AND state IN (?, ?, ?)",
        )
        .bind(final_state)
        .bind(self.now_ms())
        .bind(id)
        .bind(state::QUEUED)
        .bind(state::DISPATCHED)
        .bind(state::RUNNING)
        .execute(&self.pool)
        .await?;
        Ok(done.rows_affected() == 1)
    }

    /// Server-side terminal transition: the state (from one of `from`) and
    /// its event are written in one transaction. True when this caller made
    /// the transition.
    pub(crate) async fn terminate(
        &self,
        id: &str,
        from: &[&str],
        termination: Termination<'_>,
    ) -> Result<bool, Error> {
        Ok(with_retry(|| self.terminate_tx(id, from, termination)).await?)
    }

    async fn terminate_tx(
        &self,
        id: &str,
        from: &[&str],
        termination: Termination<'_>,
    ) -> Result<bool, sqlx::Error> {
        let now = self.now_ms();
        let mut tx = self.pool.begin().await?;
        let sql = format!(
            "UPDATE jobs SET state = ?, updated_at = ? WHERE id = ? AND state IN ({})",
            placeholders(from.len())
        );
        let mut update = sqlx::query(&sql).bind(termination.to).bind(now).bind(id);
        for state in from {
            update = update.bind(*state);
        }
        if update.execute(&mut *tx).await?.rows_affected() != 1 {
            return Ok(false);
        }
        let sql = format!(
            "{} INTO job_events (job_id, seq, time_ms, code, data) VALUES (?, ?, ?, ?, ?)",
            self.insert_ignore()
        );
        sqlx::query(&sql)
            .bind(id)
            .bind(termination.seq)
            .bind(now)
            .bind(i64::from(termination.code))
            .bind(termination.data)
            .execute(&mut *tx)
            .await?;
        tx.commit().await?;
        Ok(true)
    }

    /// Puts back in the queue, by primary key, the given dispatched jobs.
    async fn requeue(&self, rows: &[AnyRow]) -> Result<u64, Error> {
        let mut requeued = 0;
        for row in rows {
            let id: String = row.try_get("id")?;
            let session_id: String = row.try_get("session_id")?;
            requeued += u64::from(self.unclaim_job(&id, &session_id).await?);
        }
        Ok(requeued)
    }

    /// Requeues the jobs of `node_id` that were dispatched to a session other
    /// than `current_session` and never produced an agent event: the agent
    /// that got them is gone, they must go first to the new session.
    pub(crate) async fn requeue_stale_dispatched(
        &self,
        node_id: &str,
        current_session: &str,
    ) -> Result<u64, Error> {
        let rows = sqlx::query(
            "SELECT j.id, j.session_id FROM jobs j
             WHERE j.node_id = ? AND j.state = ? AND j.session_id <> ?
               AND NOT EXISTS (SELECT 1 FROM job_events e
                               WHERE e.job_id = j.id AND e.seq > 0 AND e.seq < ?)
               AND EXISTS (SELECT 1 FROM sessions s
                           WHERE s.node_id = ? AND s.session_id = ?)",
        )
        .bind(node_id)
        .bind(state::DISPATCHED)
        .bind(current_session)
        .bind(SERVER_SEQ_FLOOR)
        // Only the current session may take over: a pass still running for
        // a replaced one must not requeue what its successor was given.
        .bind(node_id)
        .bind(current_session)
        .fetch_all(&self.pool)
        .await?;
        self.requeue(&rows).await
    }

    /// Requeues the jobs dispatched to a session that no longer exists and
    /// that never produced an agent event.
    pub(crate) async fn requeue_orphans(&self) -> Result<u64, Error> {
        let rows = sqlx::query(
            "SELECT j.id, j.session_id FROM jobs j
             WHERE j.state = ?
               AND NOT EXISTS (SELECT 1 FROM sessions s WHERE s.session_id = j.session_id)
               AND NOT EXISTS (SELECT 1 FROM job_events e
                               WHERE e.job_id = j.id AND e.seq > 0 AND e.seq < ?)
             LIMIT ?",
        )
        .bind(state::DISPATCHED)
        .bind(SERVER_SEQ_FLOOR)
        .bind(BATCH)
        .fetch_all(&self.pool)
        .await?;
        self.requeue(&rows).await
    }

    /// Expires queued jobs past their deadline, and jobs handed to an agent
    /// that stayed silent `grace_ms` after it. Returns the expired job ids.
    pub(crate) async fn expire_jobs(
        &self,
        grace_ms: i64,
        data: &str,
    ) -> Result<Vec<String>, Error> {
        let now = self.now_ms();
        let rows = sqlx::query(
            "SELECT id FROM jobs
             WHERE (state = ? AND deadline_at <= ?) OR (state IN (?, ?) AND deadline_at <= ?)
             LIMIT ?",
        )
        .bind(state::QUEUED)
        .bind(now)
        .bind(state::DISPATCHED)
        .bind(state::RUNNING)
        .bind(now.saturating_sub(grace_ms))
        .bind(BATCH)
        .fetch_all(&self.pool)
        .await?;
        let termination = Termination {
            to: state::EXPIRED,
            seq: EXPIRED_SEQ,
            code: gorgone_core::legacy::ACTION_FINISH_KO,
            data,
        };
        let mut expired = Vec::new();
        for id in strings(&rows, "id")? {
            // Another replica may run the same sweep: only the winner counts.
            if self.terminate(&id, &state::ACTIVE, termination).await? {
                expired.push(id);
            }
        }
        Ok(expired)
    }

    /// Cancels a job still in the queue. True when this caller cancelled it.
    pub(crate) async fn cancel_queued(&self, id: &str, data: &str) -> Result<bool, Error> {
        let termination = Termination {
            to: state::CANCELLED,
            seq: CANCELLED_SEQ,
            code: gorgone_core::legacy::ACTION_FINISH_KO,
            data,
        };
        self.terminate(id, &[state::QUEUED], termination).await
    }

    /// Flags a job that already reached an agent; its owner replica sends the
    /// `Cancel` frame. True when the job is still dispatched or running.
    pub(crate) async fn request_cancel(&self, id: &str) -> Result<bool, Error> {
        sqlx::query(
            "UPDATE jobs SET cancel_requested = 1
             WHERE id = ? AND state IN (?, ?) AND cancel_requested = 0",
        )
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

    /// Jobs of a node whose `Cancel` frame was not sent to `session_id` yet:
    /// a new session of the node gets it again while the job is not terminal.
    pub(crate) async fn pending_cancels(
        &self,
        node_id: &str,
        session_id: &str,
    ) -> Result<Vec<String>, Error> {
        let rows = sqlx::query(
            "SELECT id FROM jobs
             WHERE node_id = ? AND cancel_requested <> 0 AND state IN (?, ?)
               AND (cancel_sent_session IS NULL OR cancel_sent_session <> ?)",
        )
        .bind(node_id)
        .bind(state::DISPATCHED)
        .bind(state::RUNNING)
        .bind(session_id)
        .fetch_all(&self.pool)
        .await?;
        Ok(strings(&rows, "id")?)
    }

    /// Records that the `Cancel` frame of a job was handed to `session_id`.
    pub(crate) async fn mark_cancel_sent(&self, id: &str, session_id: &str) -> Result<(), Error> {
        sqlx::query("UPDATE jobs SET cancel_sent_session = ? WHERE id = ?")
            .bind(session_id)
            .bind(id)
            .execute(&self.pool)
            .await?;
        Ok(())
    }

    /// Cancels the flagged jobs that went back to the queue (session lost
    /// before the agent produced anything). Returns their ids.
    pub(crate) async fn cancel_requeued(&self, data: &str) -> Result<Vec<String>, Error> {
        let rows =
            sqlx::query("SELECT id FROM jobs WHERE state = ? AND cancel_requested <> 0 LIMIT ?")
                .bind(state::QUEUED)
                .bind(BATCH)
                .fetch_all(&self.pool)
                .await?;
        let mut cancelled = Vec::new();
        for id in strings(&rows, "id")? {
            if self.cancel_queued(&id, data).await? {
                cancelled.push(id);
            }
        }
        Ok(cancelled)
    }

    /// Jobs of a node, in submission order.
    #[cfg(test)]
    pub(crate) async fn jobs_of_node(&self, node_id: &str) -> Result<Vec<JobRow>, Error> {
        let sql = format!("SELECT {JOB_COLUMNS} FROM jobs WHERE node_id = ? ORDER BY seq");
        let rows = sqlx::query(&sql)
            .bind(node_id)
            .fetch_all(&self.pool)
            .await?;
        Ok(rows.iter().map(job_from_row).collect::<Result<_, _>>()?)
    }

    /// Deletes, in bounded batches, the terminal jobs (with their events)
    /// last updated more than `retention_ms` ago, then the blobs of that age
    /// that no active job needs. Returns the number of jobs deleted.
    pub(crate) async fn purge(&self, retention_ms: i64, max_batches: usize) -> Result<u64, Error> {
        let cutoff = self.now_ms().saturating_sub(retention_ms);
        let mut purged = 0;
        for _ in 0..max_batches {
            let sql = format!(
                "SELECT id FROM jobs WHERE state IN ({}) AND updated_at < ? LIMIT ?",
                placeholders(state::TERMINAL.len())
            );
            let mut select = sqlx::query(&sql);
            for state in state::TERMINAL {
                select = select.bind(state);
            }
            let rows = select
                .bind(cutoff)
                .bind(BATCH)
                .fetch_all(&self.pool)
                .await?;
            let ids = strings(&rows, "id")?;
            for chunk in ids.chunks(100) {
                for (table, column) in [("job_events", "job_id"), ("jobs", "id")] {
                    let sql = format!(
                        "DELETE FROM {table} WHERE {column} IN ({})",
                        placeholders(chunk.len())
                    );
                    let mut delete = sqlx::query(&sql);
                    for id in chunk {
                        delete = delete.bind(id);
                    }
                    delete.execute(&self.pool).await?;
                }
            }
            purged += ids.len() as u64;
            if (ids.len() as i64) < BATCH {
                break;
            }
        }

        let rows = sqlx::query("SELECT digest FROM blobs WHERE created_at < ? LIMIT ?")
            .bind(cutoff)
            .bind(BATCH)
            .fetch_all(&self.pool)
            .await?;
        for digest in strings(&rows, "digest")? {
            // A job still waiting for its bundle keeps the blob alive.
            if self.blob_needed(None, &digest).await? {
                continue;
            }
            sqlx::query("DELETE FROM blob_chunks WHERE digest = ?")
                .bind(&digest)
                .execute(&self.pool)
                .await?;
            sqlx::query("DELETE FROM blobs WHERE digest = ? AND created_at < ?")
                .bind(&digest)
                .bind(cutoff)
                .execute(&self.pool)
                .await?;
        }
        Ok(purged)
    }

    // ---- job events ----

    /// Appends an event; a repeated `(job_id, seq)` is ignored. True when new.
    #[cfg(test)]
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
        let done = with_retry(|| {
            sqlx::query(&sql)
                .bind(job_id)
                .bind(seq)
                .bind(time_ms)
                .bind(code)
                .bind(data)
                .execute(&self.pool)
        })
        .await?;
        Ok(done.rows_affected() == 1)
    }

    /// True when the event is stored. An ignored insert is only a duplicate
    /// if the row is really there.
    #[cfg(test)]
    pub(crate) async fn event_exists(&self, job_id: &str, seq: i64) -> Result<bool, Error> {
        let row = sqlx::query("SELECT COUNT(*) AS n FROM job_events WHERE job_id = ? AND seq = ?")
            .bind(job_id)
            .bind(seq)
            .fetch_one(&self.pool)
            .await?;
        Ok(row.try_get::<i64, _>("n")? > 0)
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
            .bind(self.now_ms())
            .bind(&digest)
            .execute(&self.pool)
            .await?;
        if touched.rows_affected() == 0 {
            with_retry(|| self.put_blob_tx(&digest, size, bytes)).await?;
        }
        Ok((digest, size))
    }

    /// One transaction: readers never see a blob without all its chunks.
    async fn put_blob_tx(&self, digest: &str, size: i64, bytes: &[u8]) -> Result<(), sqlx::Error> {
        let mut tx = self.pool.begin().await?;
        for (idx, chunk) in bytes.chunks(CHUNK_SIZE).enumerate() {
            sqlx::query("REPLACE INTO blob_chunks (digest, idx, data) VALUES (?, ?, ?)")
                .bind(digest)
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
            .bind(digest)
            .bind(size)
            .bind(self.now_ms())
            .execute(&mut *tx)
            .await?;
        tx.commit().await
    }

    /// Size of a complete blob.
    pub(crate) async fn blob_size(&self, digest: &str) -> Result<Option<i64>, Error> {
        let row = sqlx::query("SELECT size FROM blobs WHERE digest = ?")
            .bind(digest)
            .fetch_optional(&self.pool)
            .await?;
        Ok(row.map(|r| r.try_get("size")).transpose()?)
    }

    /// True when an active job (of `node_id` when given) needs the blob.
    pub(crate) async fn blob_needed(
        &self,
        node_id: Option<&str>,
        digest: &str,
    ) -> Result<bool, Error> {
        let node_filter = if node_id.is_some() {
            " AND node_id = ?"
        } else {
            ""
        };
        let sql = format!(
            "SELECT COUNT(*) AS n FROM jobs WHERE blob_digest = ? AND state IN ({}){node_filter}",
            placeholders(state::ACTIVE.len())
        );
        let mut query = sqlx::query(&sql).bind(digest);
        for state in state::ACTIVE {
            query = query.bind(state);
        }
        if let Some(node_id) = node_id {
            query = query.bind(node_id);
        }
        let row = query.fetch_one(&self.pool).await?;
        Ok(row.try_get::<i64, _>("n")? > 0)
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
        let now = self.now_ms();
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
pub(crate) mod testing {
    use super::*;

    /// Store on a fresh SQLite file, or on `GORGONE_TEST_DATABASE_URL`.
    pub(crate) async fn store() -> (Store, tempfile::TempDir) {
        let dir = tempfile::tempdir().unwrap();
        let url = std::env::var("GORGONE_TEST_DATABASE_URL")
            .unwrap_or_else(|_| format!("sqlite://{}/gorgone.db?mode=rwc", dir.path().display()));
        (Store::connect(&url).await.unwrap(), dir)
    }

    pub(crate) fn unique(prefix: &str) -> String {
        format!("{prefix}-{}", crate::random_id().unwrap())
    }

    impl Store {
        /// Registers a session held by a replica known to be alive.
        pub(crate) async fn open_session(&self, node: &str, session: &str) {
            self.heartbeat_replica("test-replica", "http://127.0.0.1:1")
                .await
                .unwrap();
            self.upsert_session(node, session, "test-replica", "v")
                .await
                .unwrap();
        }

        /// Makes a job and a blob look `age_ms` old.
        pub(crate) async fn age(&self, job: &str, digests: &[&str], age_ms: i64) {
            let then = self.now_ms() - age_ms;
            sqlx::query("UPDATE jobs SET updated_at = ? WHERE id = ?")
                .bind(then)
                .bind(job)
                .execute(&self.pool)
                .await
                .unwrap();
            for digest in digests {
                sqlx::query("UPDATE blobs SET created_at = ? WHERE digest = ?")
                    .bind(then)
                    .bind(*digest)
                    .execute(&self.pool)
                    .await
                    .unwrap();
            }
        }

        /// Queues a small job for `node`.
        pub(crate) async fn queue(&self, id: &str, node: &str, ttl_ms: i64) {
            let job = NewJob {
                id,
                node_id: node,
                kind: "run_commands",
                action: b"x",
                ttl_ms,
                blob_digest: None,
                accepted_data: "{}",
                may_exist: false,
                requires: &[],
                latest_config: LatestConfig::None,
                config_target: None,
            };
            assert!(self.insert_job(job).await.unwrap());
        }
    }
}

#[cfg(test)]
mod tests {
    use super::testing::{store, unique};
    use super::*;

    #[tokio::test]
    async fn claim_is_a_compare_and_set_bound_to_the_current_session() {
        let (store, _dir) = store().await;
        let (job, node) = (unique("job"), unique("node"));
        store.queue(&job, &node, 60_000).await;

        // No session of the node: nothing can be claimed.
        assert!(!store.claim_job(&job, &node, "s0").await.unwrap());
        store.open_session(&node, "s1").await;
        // A former session cannot claim either.
        assert!(!store.claim_job(&job, &node, "s0").await.unwrap());

        // Concurrent claims of the current session: exactly one winner.
        let claims = (0..8).map(|_| {
            let (store, job, node) = (store.clone(), job.clone(), node.clone());
            tokio::spawn(async move { store.claim_job(&job, &node, "s1").await.unwrap() })
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
    async fn submission_order_is_assigned_by_the_store() {
        let (store, _dir) = store().await;
        let node = unique("node");
        // Concurrent submits: ranks are unique, contiguous, and follow the
        // commit order (exercises the retry on deadlock or busy database).
        let submits = (0..24).map(|i| {
            let (store, node) = (store.clone(), node.clone());
            tokio::spawn(async move { store.queue(&unique(&format!("j{i}")), &node, 60_000).await })
        });
        for submit in submits {
            submit.await.unwrap();
        }
        let last = unique("last");
        store.queue(&last, &node, 60_000).await;

        let jobs = store.jobs_of_node(&node).await.unwrap();
        let ranks: Vec<i64> = jobs.iter().map(|job| job.seq).collect();
        assert_eq!(ranks, (1..=25).collect::<Vec<i64>>());
        assert_eq!(jobs[24].id, last);
        let queued = store.queued_jobs(&node, 100).await.unwrap();
        assert_eq!(queued.iter().map(|job| job.seq).collect::<Vec<_>>(), ranks);

        // The same id twice changes nothing.
        let again = NewJob {
            id: &last,
            node_id: &node,
            kind: "run_commands",
            action: b"y",
            ttl_ms: 1,
            blob_digest: None,
            accepted_data: "{}",
            may_exist: true,
            requires: &[],
            latest_config: LatestConfig::None,
            config_target: None,
        };
        assert!(!store.insert_job(again).await.unwrap());
        assert_eq!(store.jobs_of_node(&node).await.unwrap().len(), 25);
        assert_eq!(store.events(&last, -1).await.unwrap().len(), 1);
    }

    #[tokio::test]
    async fn submit_batch_is_one_transaction_and_keeps_the_order() {
        let (store, _dir) = store().await;
        let (first_node, second_node) = (unique("node"), unique("node"));
        store.queue(&unique("before"), &first_node, 60_000).await;
        let ids: Vec<String> = (0..7).map(|i| unique(&format!("j{i}"))).collect();
        let jobs: Vec<NewJob<'_>> = ids
            .iter()
            .enumerate()
            .map(|(i, id)| NewJob {
                id,
                node_id: if i % 3 == 2 {
                    &second_node
                } else {
                    &first_node
                },
                kind: "run_commands",
                action: b"x",
                ttl_ms: 60_000,
                blob_digest: None,
                accepted_data: "{}",
                may_exist: false,
                requires: &[],
                latest_config: LatestConfig::None,
                config_target: None,
            })
            .collect();
        store.insert_jobs(&jobs).await.unwrap();

        // Ranks continue the counter of each node, in the order of the batch.
        let first = store.jobs_of_node(&first_node).await.unwrap();
        assert_eq!(
            first.iter().map(|job| job.seq).collect::<Vec<_>>(),
            [1, 2, 3, 4, 5, 6]
        );
        assert_eq!(
            first[1..].iter().map(|job| &job.id).collect::<Vec<_>>(),
            [&ids[0], &ids[1], &ids[3], &ids[4], &ids[6]]
        );
        let second = store.jobs_of_node(&second_node).await.unwrap();
        assert_eq!(second.iter().map(|job| job.seq).collect::<Vec<_>>(), [1, 2]);
        assert_eq!(
            second.iter().map(|job| &job.id).collect::<Vec<_>>(),
            [&ids[2], &ids[5]]
        );
        for id in &ids {
            let events = store.events(id, -1).await.unwrap();
            assert_eq!(events.len(), 1);
            assert_eq!((events[0].seq, events[0].code), (0, 0));
        }

        // A batch holding an id that exists commits nothing at all.
        let fresh = unique("fresh");
        let mut again = jobs[..1].to_vec();
        again.insert(
            0,
            NewJob {
                id: &fresh,
                ..jobs[0]
            },
        );
        assert!(store.insert_jobs(&again).await.is_err());
        assert!(store.job(&fresh).await.unwrap().is_none());
        assert_eq!(store.jobs_of_node(&first_node).await.unwrap().len(), 6);
    }

    #[tokio::test]
    async fn batch_claim_follows_the_order_and_needs_the_current_session() {
        let (store, _dir) = store().await;
        let node = unique("node");
        let ids: Vec<String> = (0..5).map(|i| unique(&format!("j{i}"))).collect();
        for id in &ids {
            store.queue(id, &node, 60_000).await;
        }

        // No session, then a former session: listed, nothing won.
        let claim = store.claim_jobs(&node, "s0", 3).await.unwrap();
        assert_eq!((claim.jobs.len(), claim.tried), (0, 3));
        store.open_session(&node, "s1").await;
        let claim = store.claim_jobs(&node, "s0", 3).await.unwrap();
        assert_eq!((claim.jobs.len(), claim.tried), (0, 3));

        // The current session takes the first three, in submission order.
        let claim = store.claim_jobs(&node, "s1", 3).await.unwrap();
        assert_eq!((claim.listed, claim.tried), (3, 3));
        let won = claim.jobs;
        assert_eq!(
            won.iter().map(|job| job.id.as_str()).collect::<Vec<_>>(),
            ids[..3].iter().map(String::as_str).collect::<Vec<_>>()
        );
        for id in &ids[..3] {
            let row = store.job(id).await.unwrap().unwrap();
            assert_eq!((row.state.as_str(), row.attempts), (state::DISPATCHED, 1));
        }

        // A cancelled job is neither listed nor claimed.
        assert!(store.cancel_queued(&ids[3], "{}").await.unwrap());
        let claim = store.claim_jobs(&node, "s1", 10).await.unwrap();
        assert_eq!((claim.jobs.len(), claim.listed), (1, 1));
        assert_eq!(claim.jobs[0].id, ids[4]);
        let claim = store.claim_jobs(&node, "s1", 10).await.unwrap();
        assert_eq!((claim.jobs.len(), claim.listed), (0, 0));
    }

    #[tokio::test]
    async fn event_batch_is_one_idempotent_commit() {
        let (store, _dir) = store().await;
        let node = unique("node");
        let (first, second) = (unique("job"), unique("job"));
        store.open_session(&node, "s1").await;
        for id in [&first, &second] {
            store.queue(id, &node, 60_000).await;
        }
        let claim = store.claim_jobs(&node, "s1", 10).await.unwrap();
        assert_eq!(claim.jobs.len(), 2);
        let heads = store
            .job_heads(&[&first, &second, "missing"])
            .await
            .unwrap();
        assert_eq!(heads.len(), 2);
        assert_eq!(heads[&first].state, state::DISPATCHED);
        assert_eq!(
            (heads[&first].node_id.as_str(), heads[&first].attempts),
            (node.as_str(), 1)
        );

        let event = |job_id, seq, data: &str| NewEvent {
            job_id,
            seq,
            time_ms: 1,
            code: 4,
            data: data.to_owned(),
        };
        // A duplicate inside the batch, two jobs, and both kinds of change.
        let events = [
            event(&first, 1, "{\"n\":1}"),
            event(&first, 1, "{\"n\":\"dup\"}"),
            event(&first, 2, "{\"n\":2}"),
            event(&second, 1, "{\"n\":1}"),
        ];
        let transitions = [
            Transition {
                job_id: &first,
                to: state::RUNNING,
            },
            Transition {
                job_id: &first,
                to: state::SUCCEEDED,
            },
            Transition {
                job_id: &second,
                to: state::RUNNING,
            },
        ];
        let applied = store.commit_events(&events, &transitions).await.unwrap();
        assert_eq!(applied, [(state::RUNNING, 1), (state::SUCCEEDED, 1)]);
        let stored = store.events(&first, 0).await.unwrap();
        assert_eq!(stored.len(), 2);
        assert_eq!(stored[0].data, "{\"n\":1}");
        assert_eq!(
            store.job(&first).await.unwrap().unwrap().state,
            state::SUCCEEDED
        );
        assert_eq!(
            store.job(&second).await.unwrap().unwrap().state,
            state::RUNNING
        );

        // The whole batch again: nothing changes, and it is not an error.
        let applied = store.commit_events(&events, &transitions).await.unwrap();
        assert_eq!(applied, [(state::RUNNING, 0), (state::SUCCEEDED, 0)]);
        assert_eq!(store.events(&first, 0).await.unwrap().len(), 2);
        assert_eq!(
            store.job(&first).await.unwrap().unwrap().state,
            state::SUCCEEDED
        );
    }

    #[tokio::test]
    async fn orphans_are_requeued_unless_they_have_agent_events() {
        let (store, _dir) = store().await;
        let node = unique("node");
        let session = unique("gone");
        let (silent, started) = (unique("job"), unique("job"));
        store.open_session(&node, &session).await;
        for job in [&silent, &started] {
            store.queue(job, &node, 60_000).await;
            assert!(store.claim_job(job, &node, &session).await.unwrap());
        }
        let now = store.now_ms();
        assert!(store.insert_event(&started, 1, now, 4, "{}").await.unwrap());
        // Still held by a live session: nothing moves.
        store.requeue_orphans().await.unwrap();
        assert_eq!(
            store.job(&silent).await.unwrap().unwrap().state,
            state::DISPATCHED
        );

        store.remove_session(&node, &session).await.unwrap();
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
    async fn stale_dispatches_of_a_node_go_back_to_the_queue() {
        let (store, _dir) = store().await;
        let node = unique("node");
        let (old, new) = (unique("old"), unique("new"));
        let (lost, kept) = (unique("job"), unique("job"));
        store.open_session(&node, &old).await;
        store.queue(&lost, &node, 60_000).await;
        assert!(store.claim_job(&lost, &node, &old).await.unwrap());
        store.open_session(&node, &new).await;
        store.queue(&kept, &node, 60_000).await;
        assert!(store.claim_job(&kept, &node, &new).await.unwrap());

        // At most one: on a shared test database the sweep of another test
        // may have requeued it first, its former session being gone.
        // A pass still running for the replaced session requeues nothing,
        // in particular not what the new session was given.
        assert_eq!(
            store.requeue_stale_dispatched(&node, &old).await.unwrap(),
            0
        );
        assert_eq!(
            store.job(&kept).await.unwrap().unwrap().state,
            state::DISPATCHED
        );
        assert!(store.requeue_stale_dispatched(&node, &new).await.unwrap() <= 1);
        let queued = store.queued_jobs(&node, 10).await.unwrap();
        assert_eq!(queued.len(), 1);
        assert_eq!(queued[0].id, lost);
    }

    #[tokio::test]
    async fn events_are_idempotent_on_job_and_seq() {
        let (store, _dir) = store().await;
        let job = unique("job");
        assert!(!store.event_exists(&job, 1).await.unwrap());
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
        assert!(store.event_exists(&job, 1).await.unwrap());
        let events = store.events(&job, -1).await.unwrap();
        assert_eq!(events.len(), 1);
        assert_eq!(events[0].data, "{\"a\":1}");
    }

    #[tokio::test]
    async fn server_termination_writes_state_and_event_together() {
        let (store, _dir) = store().await;
        let (job, node) = (unique("job"), unique("node"));
        store.queue(&job, &node, 60_000).await;
        assert!(store.cancel_queued(&job, "{\"m\":1}").await.unwrap());
        // Already terminal: no second transition, no second event.
        assert!(!store.cancel_queued(&job, "{\"m\":2}").await.unwrap());
        assert!(!store.finish_job(&job, state::FAILED).await.unwrap());
        let row = store.job(&job).await.unwrap().unwrap();
        assert_eq!(row.state, state::CANCELLED);
        let events = store.events(&job, 0).await.unwrap();
        assert_eq!(events.len(), 1);
        assert_eq!((events[0].seq, events[0].code), (CANCELLED_SEQ, 1));
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
        let salt = unique("blob");
        let bytes: Vec<u8> = salt
            .bytes()
            .chain((0..(CHUNK_SIZE * 2 + 17)).map(|i| (i % 251) as u8))
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
    async fn purge_keeps_active_jobs_and_the_blobs_they_need() {
        let (store, _dir) = store().await;
        let node = unique("node");
        let (done, waiting) = (unique("done"), unique("waiting"));
        let needed = store.put_blob(unique("needed").as_bytes()).await.unwrap().0;
        let unused = store.put_blob(unique("unused").as_bytes()).await.unwrap().0;
        store.queue(&done, &node, 60_000).await;
        assert!(store.cancel_queued(&done, "{}").await.unwrap());
        let job = NewJob {
            id: &waiting,
            node_id: &node,
            kind: "apply_config",
            action: b"x",
            ttl_ms: 60_000,
            blob_digest: Some(&needed),
            accepted_data: "{}",
            may_exist: false,
            requires: &[],
            latest_config: LatestConfig::None,
            config_target: None,
        };
        assert!(store.insert_job(job).await.unwrap());
        assert!(store.blob_needed(Some(&node), &needed).await.unwrap());
        assert!(!store.blob_needed(Some("other"), &needed).await.unwrap());
        assert!(!store.blob_needed(None, &unused).await.unwrap());

        // Only rows made very old are purged: a shared test database holds
        // the rows of other tests.
        let year = 365 * 24 * 3_600_000;
        store.age(&done, &[&needed, &unused], 2 * year).await;
        store.age(&waiting, &[], 2 * year).await;
        assert_eq!(store.purge(year, 10).await.unwrap(), 1);
        assert!(store.job(&done).await.unwrap().is_none());
        assert!(store.events(&done, -1).await.unwrap().is_empty());
        assert!(store.job(&waiting).await.unwrap().is_some());
        assert!(store.blob_size(&needed).await.unwrap().is_some());
        assert!(store.blob_size(&unused).await.unwrap().is_none());
        assert!(store.blob_chunk(&unused, 0).await.unwrap().is_none());
    }

    #[tokio::test]
    async fn node_token_is_stored_hashed_and_re_enrolment_drops_the_session() {
        let (store, _dir) = store().await;
        let node = unique("node");
        let token = store.enrol_node(&node, "poller").await.unwrap();
        assert_eq!(token.len(), 64);
        let hash = store.node_token_hash(&node).await.unwrap().unwrap();
        assert_eq!(hash, hash_token(&token));
        assert_ne!(hash, token);

        store.open_session(&node, "s1").await;
        assert_ne!(store.enrol_node(&node, "poller").await.unwrap(), token);
        assert!(store.session(&node).await.unwrap().is_none());

        assert!(store.delete_node(&node).await.unwrap());
        assert!(!store.node_exists(&node).await.unwrap());
    }

    #[tokio::test]
    async fn dead_replicas_lose_their_sessions() {
        let (store, _dir) = store().await;
        let (dead, alive) = (unique("replica"), unique("replica"));
        let (lost, kept) = (unique("node"), unique("node"));
        store.heartbeat_replica(&alive, "http://a").await.unwrap();
        store.upsert_session(&lost, "s1", &dead, "v").await.unwrap();
        store
            .upsert_session(&kept, "s2", &alive, "v")
            .await
            .unwrap();
        // The dead replica has no row at all; a generous delay keeps the
        // replicas of concurrent tests alive.
        assert!(store.drop_dead_replicas(3_600_000).await.unwrap() >= 1);
        assert!(store.session(&lost).await.unwrap().is_none());
        assert!(store.session(&kept).await.unwrap().is_some());
        assert_eq!(
            store.remove_replica_sessions(&alive).await.unwrap(),
            1,
            "startup cleanup removes the sessions of a replica id"
        );
    }

    /// Queues a job with prerequisites, a configuration target, or both.
    async fn queue_with(
        store: &Store,
        id: &str,
        node: &str,
        requires: &[String],
        latest_config: LatestConfig<'_>,
        config_target: Option<&str>,
    ) -> Result<bool, Error> {
        let job = NewJob {
            id,
            node_id: node,
            kind: "run_commands",
            action: b"x",
            ttl_ms: 60_000,
            blob_digest: None,
            accepted_data: "{}",
            may_exist: false,
            requires,
            latest_config,
            config_target,
        };
        store.insert_job(job).await
    }

    fn ids(jobs: &[JobRow]) -> Vec<&str> {
        jobs.iter().map(|job| job.id.as_str()).collect()
    }

    #[tokio::test]
    async fn claim_waits_for_prerequisites_and_keeps_the_order() {
        let (store, _dir) = store().await;
        let node = unique("node");
        let [a, b, c] = [unique("a"), unique("b"), unique("c")];
        store.open_session(&node, "s1").await;
        store.queue(&a, &node, 60_000).await;
        assert!(
            queue_with(
                &store,
                &b,
                &node,
                std::slice::from_ref(&a),
                LatestConfig::None,
                None
            )
            .await
            .unwrap()
        );
        store.queue(&c, &node, 60_000).await;
        assert_eq!(store.job(&b).await.unwrap().unwrap().requires, [a.as_str()]);

        // `b` waits for `a`, and `c` for its turn behind `b`.
        let claim = store.claim_jobs(&node, "s1", 10).await.unwrap();
        assert_eq!(ids(&claim.jobs), [a.as_str()]);
        assert_eq!((claim.listed, claim.tried, claim.blocked), (3, 1, true));
        let claim = store.claim_jobs(&node, "s1", 10).await.unwrap();
        assert!(claim.jobs.is_empty() && claim.unmet.is_empty() && claim.blocked);
        assert_eq!((claim.listed, claim.tried), (2, 0));

        assert!(store.finish_job(&a, state::SUCCEEDED).await.unwrap());
        // Still bound to the current session.
        let claim = store.claim_jobs(&node, "s0", 10).await.unwrap();
        assert_eq!((claim.jobs.len(), claim.tried), (0, 2));
        let claim = store.claim_jobs(&node, "s1", 10).await.unwrap();
        assert_eq!(ids(&claim.jobs), [b.as_str(), c.as_str()]);
        assert!(!claim.blocked && claim.unmet.is_empty());
    }

    #[tokio::test]
    async fn claim_reports_jobs_whose_prerequisite_did_not_succeed() {
        let (store, _dir) = store().await;
        let node = unique("node");
        let [d, e, f, g, h] = [
            unique("d"),
            unique("e"),
            unique("f"),
            unique("g"),
            unique("h"),
        ];
        let gone = unique("gone");
        store.open_session(&node, "s1").await;
        store.queue(&d, &node, 60_000).await;
        for (id, requires) in [(&e, &d), (&f, &e), (&h, &gone)] {
            let requires = std::slice::from_ref(requires);
            assert!(
                queue_with(&store, id, &node, requires, LatestConfig::None, None)
                    .await
                    .unwrap()
            );
        }
        store.queue(&g, &node, 60_000).await;
        assert!(store.cancel_queued(&d, "{}").await.unwrap());

        // `e` fails on `d`, `f` on `e` in the same walk, `h` on a job that
        // no longer exists; `g` goes on.
        let claim = store.claim_jobs(&node, "s1", 10).await.unwrap();
        assert_eq!(ids(&claim.jobs), [g.as_str()]);
        assert!(!claim.blocked);
        let unmet: Vec<(&str, &str, &str)> = claim
            .unmet
            .iter()
            .map(|u| (u.job_id.as_str(), u.prerequisite.as_str(), u.state.as_str()))
            .collect();
        assert_eq!(
            unmet,
            [
                (e.as_str(), d.as_str(), state::CANCELLED),
                (f.as_str(), e.as_str(), state::FAILED),
                (h.as_str(), gone.as_str(), state::PURGED),
            ]
        );
        // Reported, not changed: failing them is up to the caller.
        assert_eq!(store.job(&e).await.unwrap().unwrap().state, state::QUEUED);
    }

    #[tokio::test]
    async fn latest_configuration_pushes_are_resolved_in_the_submit() {
        let (store, _dir) = store().await;
        let node = unique("node");
        let job = |name: &str| unique(name);
        let (old, engine, broker) = (job("old"), job("engine"), job("broker"));
        // Nothing pushed yet: nothing required.
        let first = job("first");
        assert!(
            queue_with(&store, &first, &node, &[], LatestConfig::All, None)
                .await
                .unwrap()
        );
        assert!(
            store
                .job(&first)
                .await
                .unwrap()
                .unwrap()
                .requires
                .is_empty()
        );

        for (id, target) in [(&old, "engine"), (&engine, "engine"), (&broker, "broker")] {
            assert!(
                queue_with(&store, id, &node, &[], LatestConfig::None, Some(target))
                    .await
                    .unwrap()
            );
        }
        // The latest push of each target, after the explicit ones, once each.
        let restart = job("restart");
        let explicit = [first.clone(), engine.clone()];
        assert!(
            queue_with(&store, &restart, &node, &explicit, LatestConfig::All, None)
                .await
                .unwrap()
        );
        assert_eq!(
            store.job(&restart).await.unwrap().unwrap().requires,
            [first.clone(), engine.clone(), broker.clone()]
        );
        // Only the targets named, those never pushed adding nothing.
        for (targets, expected) in [
            (&["broker", "vmware"][..], vec![broker.clone()]),
            (&["vmware"][..], vec![]),
            (&[][..], vec![]),
            (
                &["engine", "broker", "engine"][..],
                vec![broker.clone(), engine.clone()],
            ),
        ] {
            let targets: Vec<String> = targets.iter().map(|t| (*t).to_owned()).collect();
            let id = job("some");
            let latest = LatestConfig::Targets(&targets);
            assert!(
                queue_with(&store, &id, &node, &[], latest, None)
                    .await
                    .unwrap()
            );
            assert_eq!(store.job(&id).await.unwrap().unwrap().requires, expected);
        }
        // Another node has its own pushes.
        let other = job("other");
        assert!(
            queue_with(
                &store,
                &other,
                &unique("node"),
                &[],
                LatestConfig::All,
                None
            )
            .await
            .unwrap()
        );
        assert!(
            store
                .job(&other)
                .await
                .unwrap()
                .unwrap()
                .requires
                .is_empty()
        );

        // Over the cap once resolved: refused, nothing committed.
        let many: Vec<String> = (0..MAX_REQUIRES - 1).map(|i| format!("x{i}")).collect();
        let over = job("over");
        let refused = queue_with(&store, &over, &node, &many, LatestConfig::All, None).await;
        assert!(matches!(refused, Err(Error::Invalid(_))), "{refused:?}");
        assert!(store.job(&over).await.unwrap().is_none());
        assert_eq!(store.jobs_of_node(&node).await.unwrap().len(), 9);
    }

    #[tokio::test]
    async fn node_state_is_stored_with_the_node() {
        let (store, _dir) = store().await;
        let node = unique("node");
        assert!(store.node(&node).await.unwrap().is_none());
        store.ensure_node(&node).await.unwrap();
        let row = store.node(&node).await.unwrap().unwrap();
        assert_eq!((row.state, row.state_at), (None, None));

        store.set_node_state(&node, "{\"a\":1}").await.unwrap();
        let row = store.node(&node).await.unwrap().unwrap();
        assert_eq!(row.state.as_deref(), Some("{\"a\":1}"));
        assert!(row.state_at.is_some());
        let listed = store.list_nodes().await.unwrap();
        let listed = listed.iter().find(|n| n.id == node).unwrap();
        assert_eq!(listed.state.as_deref(), Some("{\"a\":1}"));
    }

    #[tokio::test]
    async fn store_created_before_the_new_columns_is_migrated() {
        // Needs a database of its own; the shared one is already migrated.
        if std::env::var("GORGONE_TEST_DATABASE_URL").is_ok() {
            return;
        }
        let dir = tempfile::tempdir().unwrap();
        let url = format!("sqlite://{}/old.db?mode=rwc", dir.path().display());
        sqlx::any::install_default_drivers();
        let pool = AnyPoolOptions::new().connect(&url).await.unwrap();
        for statement in [
            "CREATE TABLE nodes (id VARCHAR(191) NOT NULL PRIMARY KEY, name VARCHAR(255) NOT NULL,
             token_hash VARCHAR(64) NOT NULL, created_at BIGINT NOT NULL)",
            "CREATE TABLE jobs (id VARCHAR(191) NOT NULL PRIMARY KEY,
             node_id VARCHAR(191) NOT NULL, kind VARCHAR(32) NOT NULL, action BLOB NOT NULL,
             state VARCHAR(16) NOT NULL, attempts BIGINT NOT NULL, session_id VARCHAR(191) NULL,
             created_at BIGINT NOT NULL, updated_at BIGINT NOT NULL, deadline_at BIGINT NOT NULL)",
            "INSERT INTO nodes VALUES ('n', 'n', '', 1)",
            "INSERT INTO jobs VALUES ('j', 'n', 'run_commands', x'78', 'queued', 0, NULL, 1, 1, 2)",
        ] {
            sqlx::query(statement).execute(&pool).await.unwrap();
        }
        pool.close().await;

        let store = Store::connect(&url).await.unwrap();
        assert!(store.job("j").await.unwrap().unwrap().requires.is_empty());
        assert_eq!(store.node("n").await.unwrap().unwrap().state, None);
        store.set_node_state("n", "{}").await.unwrap();
        assert!(
            queue_with(
                &store,
                "k",
                "n",
                &["j".to_owned()],
                LatestConfig::All,
                Some("engine")
            )
            .await
            .unwrap()
        );
        assert_eq!(store.job("k").await.unwrap().unwrap().requires, ["j"]);
    }

    #[tokio::test]
    async fn database_clock_is_close_to_the_local_one_in_tests() {
        let (store, _dir) = store().await;
        store.sync_clock().await.unwrap();
        assert!((store.now_ms() - local_now_ms()).abs() < 5_000);
    }
}
