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
    Action, ApplyConfig, Command, EngineCommands, RunCommands, ServiceControl,
};
use prost::Message;
use serde::{Deserialize, Serialize};
use serde_json::{Value, json};
use tokio::sync::mpsc;
use tokio_stream::wrappers::ReceiverStream;

use super::{ApiError, ApiState, Shared};
use crate::dispatcher::{CancelOutcome, SubmitOptions};
use crate::hub::job_notified;
use crate::store::{EventRow, JobRow, LatestConfig, NodeRow, SessionRow, state};

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
    if let ActionJson::ApplyConfig { digest, .. } = &body.action
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

/// `PUT /v1/nodes/{node}/config/{target}`: stores the bundle and submits the
/// matching `ApplyConfig` job.
pub(super) async fn put_config(
    State(state): State<Shared>,
    Path((node, target)): Path<(String, String)>,
    Query(query): Query<HashMap<String, String>>,
    headers: HeaderMap,
    body: Bytes,
) -> Result<(StatusCode, Json<Value>), ApiError> {
    // A plain tar is accepted for clients without zstd, and repacked.
    let plain_tar = headers
        .get(CONTENT_TYPE)
        .and_then(|value| value.to_str().ok())
        .is_some_and(|value| value.trim().eq_ignore_ascii_case("application/x-tar"));
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
    let (digest, size) = state.store.put_blob(&body).await?;
    let action = Action {
        kind: Some(Kind::ApplyConfig(ApplyConfig {
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
    let (id, existing) = submit(&state, &node, action, options).await?;
    // A retry is told what the first request submitted.
    let pushed = existing.and_then(
        |job| match Action::decode(job.action.as_slice()).ok()?.kind {
            Some(Kind::ApplyConfig(apply)) => Some((apply.digest, json!(apply.size))),
            _ => None,
        },
    );
    let (digest, size) = pushed.unwrap_or((digest, json!(size)));
    Ok((
        StatusCode::ACCEPTED,
        Json(json!({ "id": id, "digest": digest, "size": size })),
    ))
}

/// JSON view of a node: connection status, and what its agent last
/// reported (kept when the node disconnects).
fn node_json(node: &NodeRow, session: Option<&SessionRow>) -> Value {
    let reported: Value = node
        .state
        .as_deref()
        .and_then(|state| serde_json::from_str(state).ok())
        .unwrap_or_default();
    json!({
        "id": node.id,
        "name": node.name,
        "created_at": node.created_at,
        "connected": session.is_some(),
        "session": session.map(|s| json!({
            "session_id": s.session_id,
            "replica_id": s.replica_id,
            "agent_version": s.agent_version,
            "connected_at": s.connected_at,
            "last_seen": s.last_seen,
        })),
        // Digest of the bundle applied to each target.
        "applied": reported.get("applied").cloned().unwrap_or_else(|| json!({})),
        // `null` when the agent does not reach the engine through its API.
        "engine": reported.get("engine").cloned().unwrap_or_default(),
        "state_changed_at": node.state_at,
    })
}

/// `GET /v1/nodes`: enrolled nodes with their connection status and state.
pub(super) async fn list_nodes(State(state): State<Shared>) -> Result<Json<Value>, ApiError> {
    let nodes = state.store.list_nodes().await?;
    let sessions: HashMap<String, _> = state
        .store
        .list_sessions()
        .await?
        .into_iter()
        .map(|s| (s.node_id.clone(), s))
        .collect();
    let nodes: Vec<Value> = nodes
        .iter()
        .map(|node| node_json(node, sessions.get(&node.id)))
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
    let session = state.store.session(&node).await?;
    Ok(Json(node_json(&row, session.as_ref())))
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
