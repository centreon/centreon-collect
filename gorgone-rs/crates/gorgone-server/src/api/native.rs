//! Native `/v1` routes.

use std::collections::HashMap;
use std::convert::Infallible;
use std::time::Duration;

use axum::Json;
use axum::body::Bytes;
use axum::extract::{Path, Query, State};
use axum::http::header::CONTENT_TYPE;
use axum::http::{HeaderMap, StatusCode};
use axum::response::sse::{Event, KeepAlive, Sse};
use axum::response::{IntoResponse, Response};
use gorgone_core::bundle::{self, DEFAULT_MAX_UNPACKED};
use gorgone_core::legacy;
use gorgone_proto::agent::action::Kind;
use gorgone_proto::agent::service_control::Operation;
use gorgone_proto::agent::{
    Action, ApplyConfig, ApplyEngineConfiguration, Command, EngineCommands, RunCommands,
    ServiceControl, ValidateConfig,
};
use prost::Message;
use serde::{Deserialize, Serialize};
use serde_json::{Value, json};
use tokio::sync::mpsc;
use tokio_stream::wrappers::ReceiverStream;

use super::{ApiError, ApiState, Shared};
use crate::dispatcher::{CancelOutcome, SubmitOptions};
use crate::hub::job_notified;
use crate::store::{ComponentRow, EventRow, JobRow, LatestConfig, NodeRow, SessionRow, state};

/// Longest long-poll accepted by `GET /v1/jobs/{id}?wait=`.
const MAX_WAIT: Duration = Duration::from_secs(60);
/// Longest an event stream sleeps without re-reading the store.
const SSE_RECHECK: Duration = Duration::from_secs(30);

/// JSON form of an action, e.g. `{"run_commands": {"commands": [...]}}`.
#[derive(Debug, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub(super) enum ActionJson {
    RunCommands {
        commands: Vec<CommandJson>,
    },
    EngineCommands {
        commands: Vec<String>,
    },
    ApplyConfig {
        target: String,
        digest: String,
        #[serde(default)]
        size: u64,
    },
    ServiceControl {
        service: String,
        operation: OperationJson,
    },
    ValidateConfig {
        target: String,
        digest: String,
        #[serde(default)]
        size: u64,
    },
    /// A structured engine configuration already stored (see
    /// `PUT /v1/nodes/{node}/engine-configuration`).
    ApplyEngineConfiguration {
        #[serde(default = "engine_target")]
        target: String,
        digest: String,
        #[serde(default)]
        size: u64,
    },
}

/// Target a structured engine configuration is reported under by default.
fn engine_target() -> String {
    gorgone_core::DEFAULT_COMPONENT.to_owned()
}

#[derive(Debug, Serialize, Deserialize)]
pub(super) struct CommandJson {
    command: String,
    #[serde(default)]
    timeout_s: u32,
    #[serde(default)]
    continue_on_error: bool,
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub(super) enum OperationJson {
    Start,
    Stop,
    Reload,
    Restart,
}

impl From<ActionJson> for Action {
    fn from(action: ActionJson) -> Self {
        let kind = match action {
            ActionJson::RunCommands { commands } => Kind::RunCommands(RunCommands {
                commands: commands
                    .into_iter()
                    .map(|c| Command {
                        command: c.command,
                        timeout_s: c.timeout_s,
                        continue_on_error: c.continue_on_error,
                    })
                    .collect(),
            }),
            ActionJson::EngineCommands { commands } => {
                Kind::EngineCommands(EngineCommands { commands })
            }
            ActionJson::ApplyConfig {
                target,
                digest,
                size,
            } => Kind::ApplyConfig(ApplyConfig {
                target,
                digest: digest.to_ascii_lowercase(),
                size,
            }),
            ActionJson::ServiceControl { service, operation } => {
                let operation = match operation {
                    OperationJson::Start => Operation::Start,
                    OperationJson::Stop => Operation::Stop,
                    OperationJson::Reload => Operation::Reload,
                    OperationJson::Restart => Operation::Restart,
                };
                Kind::ServiceControl(ServiceControl {
                    service,
                    operation: operation.into(),
                })
            }
            ActionJson::ValidateConfig {
                target,
                digest,
                size,
            } => Kind::ValidateConfig(ValidateConfig {
                target,
                digest: digest.to_ascii_lowercase(),
                size,
            }),
            ActionJson::ApplyEngineConfiguration {
                target,
                digest,
                size,
            } => Kind::ApplyEngineConfiguration(ApplyEngineConfiguration {
                target,
                digest: digest.to_ascii_lowercase(),
                size,
            }),
        };
        Self { kind: Some(kind) }
    }
}

/// JSON view of a stored action; `null` when it cannot be decoded.
fn action_json(encoded: &[u8]) -> Value {
    let Ok(action) = Action::decode(encoded) else {
        return Value::Null;
    };
    match action.kind {
        Some(Kind::RunCommands(run)) => json!({ "run_commands": { "commands":
            run.commands.iter().map(|c| json!({
                "command": c.command,
                "timeout_s": c.timeout_s,
                "continue_on_error": c.continue_on_error,
            })).collect::<Vec<_>>() } }),
        Some(Kind::EngineCommands(engine)) => {
            json!({ "engine_commands": { "commands": engine.commands } })
        }
        Some(Kind::ApplyConfig(apply)) => json!({ "apply_config": {
            "target": apply.target, "digest": apply.digest, "size": apply.size } }),
        Some(Kind::ServiceControl(control)) => {
            let operation = match control.operation() {
                Operation::Start => "start",
                Operation::Stop => "stop",
                Operation::Reload => "reload",
                Operation::Restart => "restart",
                Operation::Unspecified => "unspecified",
            };
            json!({ "service_control": { "service": control.service, "operation": operation } })
        }
        Some(Kind::ValidateConfig(validate)) => json!({ "validate_config": {
            "target": validate.target, "digest": validate.digest, "size": validate.size } }),
        Some(Kind::ApplyEngineConfiguration(apply)) => json!({ "apply_engine_configuration": {
            "target": apply.target, "digest": apply.digest, "size": apply.size } }),
        None => Value::Null,
    }
}

fn event_json(event: &EventRow) -> Value {
    json!({
        "seq": event.seq,
        "time_ms": event.time_ms,
        "code": event.code,
        // Stored as JSON text; a corrupt row is shown as is.
        "data": serde_json::from_str::<Value>(&event.data)
            .unwrap_or_else(|_| Value::String(event.data.clone())),
    })
}

fn job_json(job: &JobRow, events: &[EventRow]) -> Value {
    json!({
        "id": job.id,
        "node_id": job.node_id,
        // Component of the node whose agent runs it.
        "component": job.component,
        "kind": job.kind,
        "state": job.state,
        "attempts": job.attempts,
        "created_at": job.created_at,
        "updated_at": job.updated_at,
        "deadline_at": job.deadline_at,
        "cancel_requested": job.cancel_requested,
        // Rank in the node's submission order, which is the dispatch order.
        "seq": job.seq,
        // Jobs that had to succeed before this one was dispatched.
        "requires": job.requires,
        "action": action_json(&job.action),
        "events": events.iter().map(event_json).collect::<Vec<_>>(),
    })
}

#[derive(Debug, Deserialize)]
pub(super) struct SubmitBody {
    #[serde(flatten)]
    action: ActionJson,
    /// Seconds before the job expires; server default when absent.
    deadline_s: Option<u64>,
    /// Caller-chosen job id, making a retry of the request harmless.
    id: Option<String>,
    /// Jobs of the node that must succeed before this one is dispatched.
    #[serde(default)]
    requires: Vec<String>,
    /// Also require the latest configuration push of the node: of every
    /// target (`true`), or of the targets named.
    #[serde(default)]
    requires_latest_config: LatestConfigJson,
}

#[derive(Debug, Deserialize)]
#[serde(untagged)]
pub(super) enum LatestConfigJson {
    Every(bool),
    Targets(Vec<String>),
}

impl Default for LatestConfigJson {
    fn default() -> Self {
        Self::Every(false)
    }
}

impl LatestConfigJson {
    fn as_store(&self) -> LatestConfig<'_> {
        match self {
            Self::Every(false) => LatestConfig::None,
            Self::Every(true) => LatestConfig::All,
            Self::Targets(targets) => LatestConfig::Targets(targets),
        }
    }
}

/// True for `[A-Za-z0-9_-]{1,64}`, the form of a caller-chosen job id.
fn valid_job_id(id: &str) -> bool {
    (1..=64).contains(&id.len())
        && id
            .bytes()
            .all(|b| b.is_ascii_alphanumeric() || b == b'_' || b == b'-')
}

/// Submits a job. `Ok(Some(job))` is the job a former request created with
/// the same caller-chosen id: the answer must then be the one it got.
async fn submit(
    state: &ApiState,
    node: &str,
    action: Action,
    options: SubmitOptions<'_>,
) -> Result<(String, Option<JobRow>), ApiError> {
    if options.id.is_some_and(|id| !valid_job_id(id)) {
        return Err(ApiError::bad_request("id must match [A-Za-z0-9_-]{1,64}"));
    }
    let (id, created) = state.dispatcher.submit_job(node, action, options).await?;
    if created {
        return Ok((id, None));
    }
    match state.store.job(&id).await? {
        Some(job) if job.node_id == node => Ok((id, Some(job))),
        _ => Err(ApiError::conflict("job id already used by another node")),
    }
}

/// `POST /v1/nodes/{node}/jobs`
pub(super) async fn submit_job(
    State(state): State<Shared>,
    Path(node): Path<String>,
    Json(body): Json<SubmitBody>,
) -> Result<(StatusCode, Json<Value>), ApiError> {
    if let ActionJson::ApplyConfig { digest, .. }
    | ActionJson::ValidateConfig { digest, .. }
    | ActionJson::ApplyEngineConfiguration { digest, .. } = &body.action
        && state
            .store
            .blob_size(&digest.to_ascii_lowercase())
            .await?
            .is_none()
    {
        return Err(ApiError::bad_request("unknown blob digest"));
    }
    let options = SubmitOptions {
        id: body.id.as_deref(),
        requires: &body.requires,
        latest_config: body.requires_latest_config.as_store(),
        deadline: body.deadline_s.map(Duration::from_secs),
    };
    let (id, _) = submit(&state, &node, body.action.into(), options).await?;
    Ok((StatusCode::ACCEPTED, Json(json!({ "id": id }))))
}

#[derive(Debug, Deserialize)]
pub(super) struct WaitQuery {
    /// Seconds to long-poll for a terminal state.
    wait: Option<f64>,
}

/// `GET /v1/jobs/{id}`
pub(super) async fn get_job(
    State(state): State<Shared>,
    Path(id): Path<String>,
    Query(query): Query<WaitQuery>,
) -> Result<Json<Value>, ApiError> {
    let wait = query
        .wait
        .filter(|w| w.is_finite() && *w > 0.0)
        .map_or(Duration::ZERO, |w| {
            Duration::from_secs_f64(w.min(MAX_WAIT.as_secs_f64()))
        });
    let job = state
        .dispatcher
        .wait_job(&id, wait)
        .await?
        .ok_or_else(|| ApiError::not_found("unknown job"))?;
    let events = state.store.events(&id, -1).await?;
    Ok(Json(job_json(&job, &events)))
}

/// `DELETE /v1/jobs/{id}`: cancels a queued job, or asks the agent to stop
/// one it already received.
pub(super) async fn cancel_job(
    State(state): State<Shared>,
    Path(id): Path<String>,
) -> Result<(StatusCode, Json<Value>), ApiError> {
    let status = match state.dispatcher.cancel(&id).await? {
        CancelOutcome::Cancelled => StatusCode::OK,
        CancelOutcome::Requested => StatusCode::ACCEPTED,
        CancelOutcome::AlreadyFinished => StatusCode::CONFLICT,
        CancelOutcome::NotFound => return Err(ApiError::not_found("unknown job")),
    };
    let job = state
        .store
        .job(&id)
        .await?
        .ok_or_else(|| ApiError::not_found("unknown job"))?;
    let events = state.store.events(&id, -1).await?;
    Ok((status, Json(job_json(&job, &events))))
}

/// `GET /v1/jobs/{id}/events`: server-sent events, closed on the terminal one.
pub(super) async fn job_events(
    State(state): State<Shared>,
    Path(id): Path<String>,
) -> Result<Response, ApiError> {
    if state.store.job(&id).await?.is_none() {
        return Err(ApiError::not_found("unknown job"));
    }
    let (tx, rx) = mpsc::channel::<Result<Event, Infallible>>(16);
    tokio::spawn(async move {
        let mut updates = state.hub.subscribe();
        // Woken by local commits and by the shared poller of the replica.
        let _watch = state.hub.watch(&id);
        let mut last_seq = -1;
        loop {
            // State first: events committed before a terminal state are then
            // always part of the read that follows.
            let Ok(Some(job)) = state.store.job(&id).await else {
                return;
            };
            let Ok(events) = state.store.events(&id, last_seq).await else {
                return;
            };
            for event in &events {
                last_seq = event.seq;
                let sse = Event::default()
                    .event("job_event")
                    .id(event.seq.to_string())
                    .data(event_json(event).to_string());
                if tx.send(Ok(sse)).await.is_err() {
                    return;
                }
                if i32::try_from(event.code).is_ok_and(legacy::is_terminal) {
                    return;
                }
            }
            if state::is_terminal(&job.state) {
                return;
            }
            tokio::select! {
                // The client went away: nothing left to stream to.
                () = tx.closed() => return,
                _ = tokio::time::timeout(SSE_RECHECK, job_notified(&mut updates, &id, false)) => {}
            }
        }
    });
    Ok(Sse::new(ReceiverStream::new(rx))
        .keep_alive(KeepAlive::default())
        .into_response())
}

/// What a configuration upload submits once its blob is stored.
#[derive(Debug, Clone, Copy)]
enum ConfigJob {
    Apply,
    Validate,
}

/// True for the lowercase hex form of a SHA-256.
fn valid_digest(digest: &str) -> bool {
    digest.len() == 64
        && digest
            .bytes()
            .all(|b| b.is_ascii_digit() || (b'a'..=b'f').contains(&b))
}

/// Checks an uploaded bundle (a plain tar is repacked) and stores it.
/// Returns the digest and the size of the blob.
async fn store_bundle(
    state: &ApiState,
    headers: &HeaderMap,
    body: Bytes,
) -> Result<(String, i64), ApiError> {
    // A plain tar is accepted for clients without zstd, and repacked.
    let plain_tar = headers
        .get(CONTENT_TYPE)
        .and_then(|value| value.to_str().ok())
        .is_some_and(|value| value.trim().eq_ignore_ascii_case("application/x-tar"));
    // (De)compression is CPU bound: keep it off the async workers.
    let body = tokio::task::spawn_blocking(move || {
        if plain_tar {
            bundle::from_tar(&body, DEFAULT_MAX_UNPACKED).map(Bytes::from)
        } else {
            bundle::verify(&body, DEFAULT_MAX_UNPACKED).map(|_| body)
        }
    })
    .await
    .map_err(|_| ApiError::bad_request("bundle verification aborted"))?
    .map_err(|err| ApiError::bad_request(err.to_string()))?;
    Ok(state.store.put_blob(&body).await?)
}

/// True when a job already has the caller-chosen id.
async fn retried(state: &ApiState, id: Option<&String>) -> Result<bool, ApiError> {
    Ok(match id {
        Some(id) => state.store.job(id).await?.is_some(),
        None => false,
    })
}

/// Common part of the configuration routes: stores the bundle (a push may
/// name a stored one with `?digest=` instead) and submits `job`.
async fn submit_config(
    state: &ApiState,
    job: ConfigJob,
    (node, target): (String, String),
    query: &HashMap<String, String>,
    headers: &HeaderMap,
    body: Bytes,
) -> Result<(StatusCode, Json<Value>), ApiError> {
    let deadline = query
        .get("deadline_s")
        .map(|v| v.parse::<u64>().map(Duration::from_secs))
        .transpose()
        .map_err(|_| ApiError::bad_request("deadline_s must be an integer"))?;
    // `?requires=id1,id2`
    let requires: Vec<String> = query
        .get("requires")
        .map(|ids| {
            ids.split(',')
                .filter(|id| !id.is_empty())
                .map(str::to_owned)
        })
        .into_iter()
        .flatten()
        .collect();
    // Only a push may reuse a blob: it is how a validated bundle is applied
    // without being uploaded twice.
    let reused = match job {
        ConfigJob::Apply => query.get("digest"),
        ConfigJob::Validate => None,
    };
    let (digest, size) = match reused {
        Some(_) if !body.is_empty() => {
            return Err(ApiError::bad_request(
                "digest names a stored bundle: the body must be empty",
            ));
        }
        Some(digest) => {
            let digest = digest.to_ascii_lowercase();
            if !valid_digest(&digest) {
                return Err(ApiError::bad_request("digest must be a hex SHA-256"));
            }
            let size = match state.store.touch_blob(&digest).await? {
                Some(size) => size,
                // A retry gets the answer of the first request even when
                // the blob was purged since.
                None if retried(state, query.get("id")).await? => 0,
                None => {
                    return Err(ApiError::not_found(
                        "no stored bundle has this digest (never uploaded, or purged): \
                         upload it again",
                    ));
                }
            };
            (digest, size)
        }
        None => store_bundle(state, headers, body).await?,
    };
    let unsigned_size = u64::try_from(size).unwrap_or_default();
    let kind = match job {
        ConfigJob::Apply => Kind::ApplyConfig(ApplyConfig {
            target,
            digest: digest.clone(),
            size: unsigned_size,
        }),
        ConfigJob::Validate => Kind::ValidateConfig(ValidateConfig {
            target,
            digest: digest.clone(),
            size: unsigned_size,
        }),
    };
    let options = SubmitOptions {
        id: query.get("id").map(String::as_str),
        requires: &requires,
        latest_config: LatestConfig::None,
        deadline,
    };
    let action = Action { kind: Some(kind) };
    let (id, existing) = submit(state, &node, action, options).await?;
    // A retry is told what the first request submitted.
    let submitted =
        existing.and_then(
            |job| match Action::decode(job.action.as_slice()).ok()?.kind {
                Some(Kind::ApplyConfig(apply)) => Some((apply.digest, json!(apply.size))),
                Some(Kind::ValidateConfig(validate)) => {
                    Some((validate.digest, json!(validate.size)))
                }
                _ => None,
            },
        );
    let (digest, size) = submitted.unwrap_or((digest, json!(size)));
    Ok((
        StatusCode::ACCEPTED,
        Json(json!({ "id": id, "digest": digest, "size": size })),
    ))
}

/// Versions of the structured configuration document this server accepts.
const ENGINE_DOCUMENT_VERSIONS: std::ops::RangeInclusive<u64> = 1..=5;

/// Highest document version the engine of a node reads, from the version
/// its heartbeat reports: `centreon-engine 0.1.0 (configuration 1-5)`. An
/// engine that reports no range reads version 1 only; `None` when the node
/// reports no centreon-engine (yet): the engine checks it when it gets it.
fn node_document_version(state: Option<&str>) -> Option<u64> {
    let state: Value = serde_json::from_str(state?).ok()?;
    let version = state["engine"]["version"].as_str()?;
    if !version.starts_with("centreon-engine ") {
        return None;
    }
    let max = version
        .split_once("(configuration ")
        .and_then(|(_, rest)| rest.split_once(')'))
        .and_then(|(range, _)| range.rsplit('-').next())
        .and_then(|max| max.trim().parse().ok());
    Some(max.unwrap_or(1))
}

/// Only the version of a document: the rest is skipped, not kept.
#[derive(Deserialize)]
struct DocumentProbe {
    version: Option<Value>,
}

/// `PUT /v1/nodes/{node}/engine-configuration`: takes the JSON document of
/// a structured engine configuration (`gorgone-rs/schemas/`), stores it
/// compressed as a blob and submits the `ApplyEngineConfiguration` job.
/// Only its syntax and version are checked here: the engine checks the rest
/// and refuses an invalid one. Query: `id`, `requires`, `deadline_s`, as
/// for the bundles; `target` (default `engine`).
pub(super) async fn put_engine_configuration(
    State(state): State<Shared>,
    Path(node): Path<String>,
    Query(query): Query<HashMap<String, String>>,
    body: Bytes,
) -> Result<(StatusCode, Json<Value>), ApiError> {
    let deadline = query
        .get("deadline_s")
        .map(|v| v.parse::<u64>().map(Duration::from_secs))
        .transpose()
        .map_err(|_| ApiError::bad_request("deadline_s must be an integer"))?;
    let requires: Vec<String> = query
        .get("requires")
        .map(|ids| {
            ids.split(',')
                .filter(|id| !id.is_empty())
                .map(str::to_owned)
        })
        .into_iter()
        .flatten()
        .collect();
    let target = query.get("target").cloned().unwrap_or_else(engine_target);
    if let Some(id) = query.get("id")
        && let Some(job) = state.store.job(id).await?
    {
        // A retry: the answer of the first request.
        let submitted = Action::decode(job.action.as_slice())
            .ok()
            .and_then(|action| match action.kind {
                Some(Kind::ApplyEngineConfiguration(apply)) => Some((apply.digest, apply.size)),
                _ => None,
            });
        let Some((digest, size)) = submitted.filter(|_| job.node_id == node) else {
            return Err(ApiError::conflict("job id already used by another job"));
        };
        return Ok((
            StatusCode::ACCEPTED,
            Json(json!({ "id": id, "digest": digest, "size": size })),
        ));
    }
    // Checked and compressed off the async workers: it may be large.
    let blob = tokio::task::spawn_blocking(move || {
        let probe: DocumentProbe = serde_json::from_slice(&body)
            .map_err(|error| format!("invalid JSON document: {error}"))?;
        let version = match probe.version {
            Some(Value::Number(version))
                if version
                    .as_u64()
                    .is_some_and(|v| ENGINE_DOCUMENT_VERSIONS.contains(&v)) =>
            {
                version.as_u64().unwrap_or_default()
            }
            Some(version) => return Err(format!("unsupported document version {version}")),
            None => return Err("document without version".to_owned()),
        };
        gorgone_core::document::pack(&body)
            .map(|blob| (blob, version))
            .map_err(|error| error.to_string())
    })
    .await
    .map_err(|_| ApiError::bad_request("document check aborted"))?
    .map_err(ApiError::bad_request)?;
    let (blob, version) = blob;
    // The engine would refuse a version it does not read: say it now, with
    // what the node reads, rather than through a failed job.
    let node_row = state.store.node(&node).await?;
    if let Some(max) = node_document_version(node_row.as_ref().and_then(|r| r.state.as_deref()))
        && version > max
    {
        return Err(ApiError::conflict(format!(
            "node {node} runs a centreon-engine that reads configuration documents up to \
             version {max}, not {version}: upgrade it"
        )));
    }
    let (digest, size) = state.store.put_blob(&blob).await?;
    let action = Action {
        kind: Some(Kind::ApplyEngineConfiguration(ApplyEngineConfiguration {
            target,
            digest: digest.clone(),
            size: u64::try_from(size).unwrap_or_default(),
        })),
    };
    let options = SubmitOptions {
        id: query.get("id").map(String::as_str),
        requires: &requires,
        latest_config: LatestConfig::None,
        deadline,
    };
    let (id, _) = submit(&state, &node, action, options).await?;
    Ok((
        StatusCode::ACCEPTED,
        Json(json!({ "id": id, "digest": digest, "size": size })),
    ))
}

/// `PUT /v1/nodes/{node}/config/{target}`: stores the bundle and submits the
/// matching `ApplyConfig` job. With `?digest=` and an empty body, the job
/// reuses the bundle already stored under that digest.
pub(super) async fn put_config(
    State(state): State<Shared>,
    Path(path): Path<(String, String)>,
    Query(query): Query<HashMap<String, String>>,
    headers: HeaderMap,
    body: Bytes,
) -> Result<(StatusCode, Json<Value>), ApiError> {
    submit_config(&state, ConfigJob::Apply, path, &query, &headers, body).await
}

/// `POST /v1/nodes/{node}/config/{target}/validation`: stores the bundle and
/// submits a `ValidateConfig` job, which checks it on the node without
/// applying it.
pub(super) async fn validate_config(
    State(state): State<Shared>,
    Path(path): Path<(String, String)>,
    Query(query): Query<HashMap<String, String>>,
    headers: HeaderMap,
    body: Bytes,
) -> Result<(StatusCode, Json<Value>), ApiError> {
    submit_config(&state, ConfigJob::Validate, path, &query, &headers, body).await
}

fn session_json(session: &SessionRow) -> Value {
    json!({
        "session_id": session.session_id,
        "replica_id": session.replica_id,
        "agent_version": session.agent_version,
        "connected_at": session.connected_at,
        "last_seen": session.last_seen,
    })
}

/// `applied` and `engine` of a stored agent report.
fn reported_json(state: Option<&str>) -> (Value, Value) {
    let reported: Value = state
        .and_then(|state| serde_json::from_str(state).ok())
        .unwrap_or_default();
    (
        // Digest of the bundle applied to each target.
        reported
            .get("applied")
            .cloned()
            .unwrap_or_else(|| json!({})),
        // `null` when the agent does not reach the engine through its API.
        reported.get("engine").cloned().unwrap_or_default(),
    )
}

/// JSON view of a node: connection status, and what its agents last
/// reported (kept when they disconnect). The top-level session and state
/// are those of the engine component; `components` lists every component
/// with its own.
fn node_json(node: &NodeRow, sessions: &[&SessionRow], components: &[&ComponentRow]) -> Value {
    let (applied, engine) = reported_json(node.state.as_deref());
    let engine_session = sessions
        .iter()
        .find(|s| s.component == gorgone_core::DEFAULT_COMPONENT);
    let mut names: Vec<&str> = components
        .iter()
        .map(|c| c.component.as_str())
        .chain(sessions.iter().map(|s| s.component.as_str()))
        .collect();
    names.sort_unstable();
    names.dedup();
    let components: Vec<Value> = names
        .into_iter()
        .map(|name| {
            let session = sessions.iter().find(|s| s.component == name);
            let row = components.iter().find(|c| c.component == name);
            let (applied, engine) = reported_json(row.and_then(|c| c.state.as_deref()));
            json!({
                "component": name,
                "connected": session.is_some(),
                "session": session.map(|s| session_json(s)),
                "targets": row.map_or(&[][..], |c| c.targets.as_slice()),
                "applied": applied,
                "engine": engine,
                "state_changed_at": row.and_then(|c| c.state_at),
            })
        })
        .collect();
    json!({
        "id": node.id,
        "name": node.name,
        "created_at": node.created_at,
        "connected": !sessions.is_empty(),
        "session": engine_session.map(|s| session_json(s)),
        "applied": applied,
        "engine": engine,
        "state_changed_at": node.state_at,
        "components": components,
    })
}

/// `GET /v1/nodes`: enrolled nodes with their connection status and state.
pub(super) async fn list_nodes(State(state): State<Shared>) -> Result<Json<Value>, ApiError> {
    let nodes = state.store.list_nodes().await?;
    let mut sessions: HashMap<String, Vec<SessionRow>> = HashMap::new();
    for session in state.store.list_sessions().await? {
        sessions
            .entry(session.node_id.clone())
            .or_default()
            .push(session);
    }
    let mut components: HashMap<String, Vec<ComponentRow>> = HashMap::new();
    for (node_id, component) in state.store.all_components().await? {
        components.entry(node_id).or_default().push(component);
    }
    let nodes: Vec<Value> = nodes
        .iter()
        .map(|node| {
            let sessions: Vec<&SessionRow> = sessions.get(&node.id).into_iter().flatten().collect();
            let components: Vec<&ComponentRow> =
                components.get(&node.id).into_iter().flatten().collect();
            node_json(node, &sessions, &components)
        })
        .collect();
    Ok(Json(json!({ "nodes": nodes })))
}

/// `GET /v1/nodes/{node}`: one node, as in the list.
pub(super) async fn get_node(
    State(state): State<Shared>,
    Path(node): Path<String>,
) -> Result<Json<Value>, ApiError> {
    let row = state
        .store
        .node(&node)
        .await?
        .ok_or_else(|| ApiError::not_found("unknown node"))?;
    let sessions = state.store.sessions_of(&node).await?;
    let components = state.store.components(&node).await?;
    Ok(Json(node_json(
        &row,
        &sessions.iter().collect::<Vec<_>>(),
        &components.iter().collect::<Vec<_>>(),
    )))
}

#[derive(Debug, Default, Deserialize)]
pub(super) struct EnrolBody {
    name: Option<String>,
}

/// `PUT /v1/nodes/{node}`: enrols the node and returns its token, once.
pub(super) async fn enrol_node(
    State(state): State<Shared>,
    Path(node): Path<String>,
    body: Bytes,
) -> Result<Json<Value>, ApiError> {
    if node.is_empty() || node.len() > 191 {
        return Err(ApiError::bad_request("invalid node id"));
    }
    // The body is optional.
    let body: EnrolBody = if body.is_empty() {
        EnrolBody::default()
    } else {
        serde_json::from_slice(&body).map_err(|e| ApiError::bad_request(e.to_string()))?
    };
    let name = body.name.unwrap_or_else(|| node.clone());
    let token = state.store.enrol_node(&node, &name).await?;
    // A session opened with the former token must not survive: the store
    // dropped its row (seen by the replica holding it), this closes ours.
    state.hub.kick(&node);
    Ok(Json(json!({ "id": node, "name": name, "token": token })))
}

/// `DELETE /v1/nodes/{node}`
pub(super) async fn delete_node(
    State(state): State<Shared>,
    Path(node): Path<String>,
) -> Result<StatusCode, ApiError> {
    let existed = state.store.delete_node(&node).await?;
    state.dispatcher.forget_node(&node);
    state.hub.kick(&node);
    if existed {
        Ok(StatusCode::NO_CONTENT)
    } else {
        Err(ApiError::not_found("unknown node"))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn action_json_round_trip() {
        let body: SubmitBody = serde_json::from_value(json!({
            "run_commands": { "commands": [{ "command": "echo ok", "timeout_s": 30 }] },
            "deadline_s": 5,
        }))
        .unwrap();
        assert_eq!(body.deadline_s, Some(5));
        assert!(body.id.is_none() && body.requires.is_empty());
        assert!(matches!(
            body.requires_latest_config.as_store(),
            LatestConfig::None
        ));
        let action: Action = body.action.into();
        assert_eq!(
            action_json(&action.encode_to_vec()),
            json!({ "run_commands": { "commands": [
                { "command": "echo ok", "timeout_s": 30, "continue_on_error": false }
            ] } })
        );

        let body: SubmitBody = serde_json::from_value(json!({
            "service_control": { "service": "engine", "operation": "restart" },
            "id": "restart-1",
            "requires": ["a", "b"],
            "requires_latest_config": true,
        }))
        .unwrap();
        assert_eq!(body.id.as_deref(), Some("restart-1"));
        assert_eq!(body.requires, ["a", "b"]);
        assert!(matches!(
            body.requires_latest_config.as_store(),
            LatestConfig::All
        ));
        let body: SubmitBody = serde_json::from_value(json!({
            "service_control": { "service": "engine", "operation": "restart" },
            "requires_latest_config": ["engine", "broker"],
        }))
        .unwrap();
        assert!(matches!(
            body.requires_latest_config.as_store(),
            LatestConfig::Targets([engine, broker]) if engine == "engine" && broker == "broker"
        ));
        assert!(valid_job_id("Abc_09-z") && valid_job_id(&"a".repeat(64)));
        for bad in ["", "a b", "a/b", "é", &"a".repeat(65)] {
            assert!(!valid_job_id(bad), "{bad}");
        }

        let validate: ActionJson = serde_json::from_value(
            json!({ "validate_config": { "target": "engine", "digest": "AB12" } }),
        )
        .unwrap();
        let action: Action = validate.into();
        assert_eq!(
            action_json(&action.encode_to_vec()),
            json!({ "validate_config": { "target": "engine", "digest": "ab12", "size": 0 } })
        );
        assert!(valid_digest(&"0a".repeat(32)));
        for bad in [
            "",
            "0a",
            &"0A".repeat(32),
            &"0g".repeat(32),
            &"0".repeat(65),
        ] {
            assert!(!valid_digest(bad), "{bad}");
        }

        let control: ActionJson = serde_json::from_value(
            json!({ "service_control": { "service": "engine", "operation": "reload" } }),
        )
        .unwrap();
        let action: Action = control.into();
        assert_eq!(
            action_json(&action.encode_to_vec())["service_control"]["operation"],
            "reload"
        );
    }
}
