//! Legacy `/api` routes, answering with the shapes of the Perl daemon so
//! that centreon-web and the monitoring plugin keep working unchanged.

use std::collections::HashMap;
use std::sync::atomic::Ordering;
use std::time::Duration;

use axum::body::Bytes;
use axum::extract::{Path, Query, State};
use axum::http::StatusCode;
use axum::response::{IntoResponse, Response};
use axum::routing::{get, post};
use axum::{Json, Router};
use gorgone_proto::agent::action::Kind;
use gorgone_proto::agent::{Action, Command, EngineCommands, RunCommands};
use serde_json::{Value, json};

use super::{ApiError, Shared};

/// Upper bound of the wait translated from `log_wait` / `sync_wait`.
const MAX_LEGACY_WAIT: Duration = Duration::from_secs(5);

type Params = Query<HashMap<String, String>>;

pub(super) fn router() -> Router<Shared> {
    Router::new()
        .route("/nodes/{id}/core/action/command", post(action_command))
        .route("/nodes/{id}/centreon/engine/command", post(engine_command))
        .route("/log/{token}", get(log))
        .route("/nodes/{id}/log/{token}", get(node_log))
        .route("/internal/constatus", get(constatus))
        .route("/internal/information", get(information))
        .route("/internal/thumbprint", get(thumbprint))
        .route("/nodes/{id}/internal/thumbprint", get(thumbprint))
        .fallback(method_unknown)
        .method_not_allowed_fallback(method_unknown)
}

async fn method_unknown() -> Response {
    let body = json!({
        "error": "method_unknown",
        "message": "Method not implemented",
        "http_response_code": "404",
    });
    (StatusCode::NOT_FOUND, Json(body)).into_response()
}

/// `log_wait` and `sync_wait` are microseconds in the Perl API; both become
/// one bounded wait for the job to finish.
fn legacy_wait(params: &HashMap<String, String>) -> Option<Duration> {
    let micros = ["log_wait", "sync_wait"]
        .iter()
        .filter_map(|key| params.get(*key)?.parse::<u64>().ok())
        .max()?;
    Some(Duration::from_micros(micros).min(MAX_LEGACY_WAIT))
}

fn decode_body(body: &[u8]) -> Result<Value, ApiError> {
    serde_json::from_slice(body).map_err(|_| {
        ApiError::new(
            StatusCode::BAD_REQUEST,
            "decode_error",
            "Cannot decode json",
        )
    })
}

/// Perl is loose about numbers: accept `30`, `"30"`, `true`.
fn loose_u64(value: &Value) -> Option<u64> {
    match value {
        Value::Number(n) => n.as_u64().or_else(|| n.as_f64().map(|f| f.max(0.0) as u64)),
        Value::String(s) => s.trim().parse().ok(),
        Value::Bool(b) => Some(u64::from(*b)),
        _ => None,
    }
}

/// Body `[{command, timeout, continue_on_error}]`.
fn parse_commands(body: &Value) -> Result<Vec<Command>, ApiError> {
    let invalid = || ApiError::bad_request("expected an array of {command, timeout}");
    body.as_array()
        .filter(|items| !items.is_empty())
        .ok_or_else(invalid)?
        .iter()
        .map(|item| {
            let command = item
                .get("command")
                .and_then(Value::as_str)
                .filter(|c| !c.is_empty())
                .ok_or_else(invalid)?;
            Ok(Command {
                command: command.to_owned(),
                timeout_s: item
                    .get("timeout")
                    .and_then(loose_u64)
                    .and_then(|t| u32::try_from(t).ok())
                    .unwrap_or(0),
                // Perl only stops when the flag is defined and equal to 0.
                continue_on_error: item
                    .get("continue_on_error")
                    .and_then(loose_u64)
                    .is_none_or(|flag| flag != 0),
            })
        })
        .collect()
}

/// Body `{commands: [...]}`; a list of such objects is accepted too.
fn parse_engine_commands(body: &Value) -> Result<Vec<String>, ApiError> {
    let items = match body {
        Value::Array(items) => items.iter().collect::<Vec<_>>(),
        other => vec![other],
    };
    let commands: Vec<String> = items
        .iter()
        .filter_map(|item| item.get("commands")?.as_array())
        .flatten()
        .filter_map(|c| c.as_str().map(str::to_owned))
        .collect();
    if commands.is_empty() {
        return Err(ApiError::bad_request("expected {commands: [...]}"));
    }
    Ok(commands)
}

async fn submit(
    state: &Shared,
    node: &str,
    kind: Kind,
    params: &HashMap<String, String>,
) -> Result<Json<Value>, ApiError> {
    state.legacy_requests.fetch_add(1, Ordering::Relaxed);
    let action = Action { kind: Some(kind) };
    let token = state.dispatcher.submit(node, action, None).await?;
    // With `log_wait` the Perl API answers with the logs instead of the token.
    match legacy_wait(params) {
        Some(wait) if params.contains_key("log_wait") => logs(state, &token, Some(wait)).await,
        _ => Ok(Json(json!({ "token": token }))),
    }
}

/// `POST /api/nodes/{id}/core/action/command`
async fn action_command(
    State(state): State<Shared>,
    Path(node): Path<String>,
    Query(params): Params,
    body: Bytes,
) -> Result<Json<Value>, ApiError> {
    let commands = parse_commands(&decode_body(&body)?)?;
    let kind = Kind::RunCommands(RunCommands { commands });
    submit(&state, &node, kind, &params).await
}

/// `POST /api/nodes/{id}/centreon/engine/command`
async fn engine_command(
    State(state): State<Shared>,
    Path(node): Path<String>,
    Query(params): Params,
    body: Bytes,
) -> Result<Json<Value>, ApiError> {
    let commands = parse_engine_commands(&decode_body(&body)?)?;
    let kind = Kind::EngineCommands(EngineCommands { commands });
    submit(&state, &node, kind, &params).await
}

/// Log answer of the Perl `get_log`: rows of `gorgone_history`, where `data`
/// is a JSON document serialised as a string.
async fn logs(
    state: &Shared,
    token: &str,
    wait: Option<Duration>,
) -> Result<Json<Value>, ApiError> {
    if let Some(wait) = wait {
        state.dispatcher.wait_job(token, wait).await?;
    }
    let events = state.store.events(token, -1).await?;
    if events.is_empty() {
        // HTTP 200 on purpose: the web application does not handle a 404.
        return Ok(Json(json!({
            "error": "no_log",
            "message": "No log found for token",
            "data": [],
            "token": token,
        })));
    }
    let data: Vec<Value> = events
        .iter()
        .enumerate()
        .map(|(index, event)| {
            json!({
                "id": index + 1,
                "token": token,
                "code": event.code,
                "etime": event.time_ms / 1000,
                "ctime": event.time_ms as f64 / 1000.0,
                "instant": 0,
                "data": event.data,
            })
        })
        .collect();
    Ok(Json(
        json!({ "message": "Logs found", "token": token, "data": data }),
    ))
}

/// `GET /api/log/{token}`
async fn log(
    State(state): State<Shared>,
    Path(token): Path<String>,
    Query(params): Params,
) -> Result<Json<Value>, ApiError> {
    state.legacy_requests.fetch_add(1, Ordering::Relaxed);
    logs(&state, &token, legacy_wait(&params)).await
}

/// `GET /api/nodes/{id}/log/{token}`: the node is not needed, every replica
/// reads the same store.
async fn node_log(
    State(state): State<Shared>,
    Path((_node, token)): Path<(String, String)>,
    Query(params): Params,
) -> Result<Json<Value>, ApiError> {
    state.legacy_requests.fetch_add(1, Ordering::Relaxed);
    logs(&state, &token, legacy_wait(&params)).await
}

/// `GET /api/internal/constatus`, built from the sessions of every replica.
async fn constatus(State(state): State<Shared>) -> Result<Json<Value>, ApiError> {
    state.legacy_requests.fetch_add(1, Ordering::Relaxed);
    let data: serde_json::Map<String, Value> = state
        .store
        .list_sessions()
        .await?
        .into_iter()
        .map(|session| {
            let status = json!({
                "last_ping_sent": session.last_seen / 1000,
                "last_ping_recv": session.last_seen / 1000,
                "nodes": {},
                "ping_ok": 1,
                "ping_failed": 0,
            });
            (session.node_id, status)
        })
        .collect();
    Ok(Json(
        json!({ "action": "constatus", "message": "ok", "data": data }),
    ))
}

/// `GET /api/internal/information`
async fn information(State(state): State<Shared>) -> Json<Value> {
    let total = state.legacy_requests.fetch_add(1, Ordering::Relaxed) + 1;
    Json(json!({
        "action": "information",
        "message": "ok",
        "data": {
            "counters": { "total": total, "external": { "total": total }, "internal": {}, "proxy": {} },
            "modules": {
                "httpserver": "gorgone-server",
                "action": "gorgone-server",
                "engine": "gorgone-server",
                "proxy": "gorgone-server",
            },
            "api_endpoints": {
                "POST_/core/action/command": "COMMAND",
                "POST_/centreon/engine/command": "ENGINECOMMAND",
                "GET_/internal/constatus": "CONSTATUS",
                "GET_/internal/information": "INFORMATION",
                "GET_/internal/thumbprint": "GETTHUMBPRINT",
            },
        },
    }))
}

/// `GET /api/[nodes/{id}/]internal/thumbprint`
async fn thumbprint(State(state): State<Shared>) -> Json<Value> {
    state.legacy_requests.fetch_add(1, Ordering::Relaxed);
    Json(json!({
        "action": "getthumbprint",
        "message": "ok",
        "data": { "thumbprint": state.thumbprint },
    }))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn commands_follow_perl_looseness() {
        let commands = parse_commands(&json!([
            { "command": "echo a", "timeout": "30" },
            { "command": "echo b", "timeout": 5, "continue_on_error": 0 },
        ]))
        .unwrap();
        assert_eq!(
            (commands[0].timeout_s, commands[0].continue_on_error),
            (30, true)
        );
        assert_eq!(
            (commands[1].timeout_s, commands[1].continue_on_error),
            (5, false)
        );
        assert!(parse_commands(&json!({ "command": "x" })).is_err());
        assert!(parse_commands(&json!([{ "timeout": 1 }])).is_err());
    }

    #[test]
    fn waits_are_bounded() {
        let params = |pairs: &[(&str, &str)]| {
            pairs
                .iter()
                .map(|(k, v)| ((*k).to_owned(), (*v).to_owned()))
                .collect::<HashMap<_, _>>()
        };
        assert_eq!(legacy_wait(&params(&[])), None);
        assert_eq!(
            legacy_wait(&params(&[("log_wait", "100000")])),
            Some(Duration::from_millis(100))
        );
        assert_eq!(
            legacy_wait(&params(&[("sync_wait", "60000000")])),
            Some(MAX_LEGACY_WAIT)
        );
    }
}
