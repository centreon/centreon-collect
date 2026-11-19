//! Enrolment routes of `/v1`: the central records the tokens it gives
//! (hashes only), follows the enrolments made with them, approves or
//! rejects those that wait for it, and assigns a node to those of fleet
//! tokens. Also the rotation request of the credential of a node.

use axum::Json;
use axum::extract::{Path, State};
use axum::http::StatusCode;
use serde::Deserialize;
use serde_json::{Value, json};

use super::{ApiError, Shared};
use crate::store::{
    AssignOutcome, EnrolmentRow, EnrolmentTokenRow, NewEnrolmentToken, NextCredential,
    enrolment_state,
};

/// Longest validity of a token: one year.
const MAX_VALIDITY_S: u64 = 365 * 24 * 3600;

fn valid_id(id: &str) -> bool {
    !id.is_empty() && id.len() <= 191
}

fn token_json(token: &EnrolmentTokenRow) -> Value {
    json!({
        "id": token.id,
        "node_id": token.node_id,
        "name": token.name,
        "expires_at": token.expires_at,
        "max_uses": token.max_uses,
        "uses": token.uses,
        "created_at": token.created_at,
        "approval_required": token.approval_required,
        "last_used_at": token.last_used_at,
    })
}

fn enrolment_json(enrolment: &EnrolmentRow) -> Value {
    json!({
        "id": enrolment.id,
        "token_id": enrolment.token_id,
        "state": enrolment.state,
        "node_id": enrolment.node_id,
        "name": enrolment.name,
        "address": enrolment.address,
        "agent_version": enrolment.agent_version,
        "component": enrolment.component,
        "created_at": enrolment.created_at,
        "updated_at": enrolment.updated_at,
        "fleet": enrolment.fleet,
        "approval_required": enrolment.approval_required,
    })
}

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
pub(super) struct TokenBody {
    /// Lowercase hex SHA-256 of the token; the token itself never comes here.
    token_hash: String,
    /// Node the token enrols (single use); absent for a fleet token.
    node_id: Option<String>,
    name: Option<String>,
    /// Validity from now, on the server clock.
    expires_in_s: u64,
    /// Fleet tokens only: enrolments allowed, unlimited when absent.
    max_uses: Option<i64>,
    /// Fleet tokens only: each enrolment waits for an approval
    /// (`POST /v1/enrolments/{id}/approve`) before a node may be assigned.
    #[serde(default)]
    approval_required: bool,
}

/// `GET /v1/enrolment-tokens`: the tokens, without their hash.
pub(super) async fn list_tokens(State(state): State<Shared>) -> Result<Json<Value>, ApiError> {
    let tokens = state.store.list_enrolment_tokens().await?;
    let tokens: Vec<Value> = tokens.iter().map(token_json).collect();
    Ok(Json(json!({ "tokens": tokens })))
}

/// `PUT /v1/enrolment-tokens/{id}`: records a token; 201 when created, 200
/// when the same one was already there.
pub(super) async fn put_token(
    State(state): State<Shared>,
    Path(id): Path<String>,
    Json(body): Json<TokenBody>,
) -> Result<(StatusCode, Json<Value>), ApiError> {
    if !valid_id(&id) {
        return Err(ApiError::bad_request("invalid token id"));
    }
    let hash = &body.token_hash;
    if hash.len() != 64 || !hash.bytes().all(|b| matches!(b, b'0'..=b'9' | b'a'..=b'f')) {
        return Err(ApiError::bad_request(
            "token_hash must be a lowercase hex SHA-256",
        ));
    }
    if body.node_id.as_deref().is_some_and(|node| !valid_id(node)) {
        return Err(ApiError::bad_request("invalid node id"));
    }
    if body.name.as_deref().is_some_and(|name| name.len() > 255) {
        return Err(ApiError::bad_request("name is longer than 255 bytes"));
    }
    if body.expires_in_s == 0 || body.expires_in_s > MAX_VALIDITY_S {
        return Err(ApiError::bad_request(format!(
            "expires_in_s must be between 1 and {MAX_VALIDITY_S}"
        )));
    }
    if body.max_uses.is_some_and(|uses| uses < 1) {
        return Err(ApiError::bad_request("max_uses must be at least 1"));
    }
    if body.node_id.is_some() && body.max_uses.is_some_and(|uses| uses != 1) {
        return Err(ApiError::bad_request(
            "a token bound to a node is single use",
        ));
    }
    if body.node_id.is_some() && body.approval_required {
        return Err(ApiError::bad_request(
            "approval_required is for fleet tokens: a token bound to a node names it",
        ));
    }
    let expires_at = state
        .store
        .now_ms()
        .saturating_add(i64::try_from(body.expires_in_s * 1000).unwrap_or(i64::MAX));
    let created = state
        .store
        .put_enrolment_token(NewEnrolmentToken {
            id: &id,
            token_hash: hash,
            node_id: body.node_id.as_deref(),
            name: body.name.as_deref(),
            expires_at,
            max_uses: body.max_uses,
            approval_required: body.approval_required,
        })
        .await
        .map_err(|err| match err {
            crate::Error::Invalid(message) => ApiError::conflict(message),
            other => other.into(),
        })?;
    let status = if created {
        StatusCode::CREATED
    } else {
        StatusCode::OK
    };
    Ok((status, Json(json!({ "id": id }))))
}

/// `DELETE /v1/enrolment-tokens/{id}`: the token enrols nothing more.
pub(super) async fn delete_token(
    State(state): State<Shared>,
    Path(id): Path<String>,
) -> Result<StatusCode, ApiError> {
    if state.store.delete_enrolment_token(&id).await? {
        Ok(StatusCode::NO_CONTENT)
    } else {
        Err(ApiError::not_found("unknown enrolment token"))
    }
}

/// `GET /v1/enrolments`: every enrolment, oldest first.
pub(super) async fn list_enrolments(State(state): State<Shared>) -> Result<Json<Value>, ApiError> {
    let enrolments = state.store.list_enrolments().await?;
    let enrolments: Vec<Value> = enrolments.iter().map(enrolment_json).collect();
    Ok(Json(json!({ "enrolments": enrolments })))
}

/// `GET /v1/enrolments/{id}`
pub(super) async fn get_enrolment(
    State(state): State<Shared>,
    Path(id): Path<String>,
) -> Result<Json<Value>, ApiError> {
    let enrolment = state
        .store
        .enrolment(&id)
        .await?
        .ok_or_else(|| ApiError::not_found("unknown enrolment"))?;
    Ok(Json(enrolment_json(&enrolment)))
}

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
pub(super) struct AssignBody {
    node_id: String,
    name: Option<String>,
}

/// `POST /v1/enrolments/{id}/assign`: gives a waiting enrolment its node,
/// created with the credential of the enrolment; 409 while it awaits an
/// approval.
pub(super) async fn assign(
    State(state): State<Shared>,
    Path(id): Path<String>,
    Json(body): Json<AssignBody>,
) -> Result<Json<Value>, ApiError> {
    if !valid_id(&body.node_id) {
        return Err(ApiError::bad_request("invalid node id"));
    }
    if body.name.as_deref().is_some_and(|name| name.len() > 255) {
        return Err(ApiError::bad_request("name is longer than 255 bytes"));
    }
    match state
        .store
        .assign_enrolment(&id, &body.node_id, body.name.as_deref())
        .await?
    {
        AssignOutcome::Assigned => {
            // A session opened under this node id before must not survive.
            state.hub.kick(&body.node_id);
            get_enrolment(State(state), Path(id)).await
        }
        AssignOutcome::NotFound => Err(ApiError::not_found("unknown enrolment")),
        AssignOutcome::Conflict(reason) => Err(ApiError::conflict(reason)),
    }
}

/// Moves an enrolment between states, 409 when it is in none of `from`.
async fn transition(
    state: &Shared,
    id: &str,
    from: &[&str],
    to: &str,
) -> Result<StatusCode, ApiError> {
    if state.store.set_enrolment_state(id, from, to).await? {
        return Ok(StatusCode::NO_CONTENT);
    }
    match state.store.enrolment(id).await? {
        Some(enrolment) if enrolment.state == to => Ok(StatusCode::NO_CONTENT),
        Some(enrolment) => Err(ApiError::conflict(format!(
            "enrolment is {}",
            enrolment.state
        ))),
        None => Err(ApiError::not_found("unknown enrolment")),
    }
}

/// `POST /v1/enrolments/{id}/approve`: a pending enrolment may get its node.
pub(super) async fn approve(
    State(state): State<Shared>,
    Path(id): Path<String>,
) -> Result<StatusCode, ApiError> {
    transition(
        &state,
        &id,
        &[enrolment_state::PENDING],
        enrolment_state::APPROVED,
    )
    .await
}

/// `POST /v1/enrolments/{id}/reject`: an enrolment waiting for its node is
/// refused; its agent is told so from then on.
pub(super) async fn reject(
    State(state): State<Shared>,
    Path(id): Path<String>,
) -> Result<StatusCode, ApiError> {
    transition(
        &state,
        &id,
        &[enrolment_state::PENDING, enrolment_state::APPROVED],
        enrolment_state::REJECTED,
    )
    .await
}

/// `POST /v1/nodes/{node}/rotate-credential`: the agent of the node is asked
/// to replace its credential; its sessions are closed so that the next one,
/// opened at once, carries the request. 202: the rotation itself happens
/// when the agent comes back (`credential_rotated_at` of the node).
pub(super) async fn rotate_credential(
    State(state): State<Shared>,
    Path(node): Path<String>,
) -> Result<(StatusCode, Json<Value>), ApiError> {
    match state.store.request_rotation(&node).await? {
        NextCredential::Recorded | NextCredential::Current => {
            state.hub.kick(&node);
            Ok((StatusCode::ACCEPTED, Json(json!({ "node_id": node }))))
        }
        NextCredential::NoCredential => Err(ApiError::conflict(
            "this node has no credential to rotate (accepted without one)",
        )),
        NextCredential::UnknownNode => Err(ApiError::not_found("unknown node")),
    }
}

/// `POST /v1/enrolments/{id}/complete`: the central finished setting the
/// node up (informative; the node works the same either way).
pub(super) async fn complete(
    State(state): State<Shared>,
    Path(id): Path<String>,
) -> Result<StatusCode, ApiError> {
    transition(
        &state,
        &id,
        &[enrolment_state::ENROLLED],
        enrolment_state::COMPLETED,
    )
    .await
}
