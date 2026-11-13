//! REST API: native `/v1`, legacy `/api`, probes and metrics.

mod auth;
mod legacy;
mod native;

use std::sync::Arc;
use std::sync::atomic::AtomicU64;

use axum::extract::{DefaultBodyLimit, State};
use axum::http::StatusCode;
use axum::response::{IntoResponse, Response};
use axum::routing::{get, post, put};
use axum::{Json, Router};
use serde_json::json;
use tracing::warn;

use crate::config::RestAuthConfig;
use crate::dispatcher::Dispatcher;
use crate::error::Error;
use crate::hub::Hub;
use crate::store::Store;

/// Shared state of the REST handlers.
pub(crate) struct ApiState {
    pub(crate) store: Store,
    pub(crate) hub: Arc<Hub>,
    pub(crate) dispatcher: Arc<Dispatcher>,
    pub(crate) auth: RestAuthConfig,
    pub(crate) thumbprint: String,
    /// Body limit of the bundle upload route; the others get [`SMALL_BODY`].
    pub(crate) body_limit: usize,
    /// Serve `/metrics` without credentials.
    pub(crate) metrics_public: bool,
    /// Requests served by the legacy layer, for `/api/internal/information`.
    pub(crate) legacy_requests: AtomicU64,
}

type Shared = Arc<ApiState>;

/// Body limit of every route but the bundle upload.
const SMALL_BODY: usize = 1 << 20;

/// Error answered as `{"error", "message"}`.
#[derive(Debug)]
pub(crate) struct ApiError {
    status: StatusCode,
    code: &'static str,
    message: String,
}

impl ApiError {
    fn new(status: StatusCode, code: &'static str, message: impl Into<String>) -> Self {
        Self {
            status,
            code,
            message: message.into(),
        }
    }

    fn bad_request(message: impl Into<String>) -> Self {
        Self::new(StatusCode::BAD_REQUEST, "bad_request", message)
    }

    fn not_found(message: impl Into<String>) -> Self {
        Self::new(StatusCode::NOT_FOUND, "not_found", message)
    }
}

impl From<Error> for ApiError {
    fn from(err: Error) -> Self {
        match err {
            Error::Invalid(message) => Self::bad_request(message),
            Error::UnknownNode(node) => Self::not_found(format!("unknown node: {node}")),
            other => {
                // Details stay in the logs, not in the answer.
                warn!(error = %other, "request failed");
                Self::new(
                    StatusCode::INTERNAL_SERVER_ERROR,
                    "internal_error",
                    "internal error",
                )
            }
        }
    }
}

impl IntoResponse for ApiError {
    fn into_response(self) -> Response {
        let body = json!({ "error": self.code, "message": self.message });
        (self.status, Json(body)).into_response()
    }
}

async fn healthz() -> &'static str {
    "ok"
}

async fn readyz(State(state): State<Shared>) -> Response {
    match state.store.ping().await {
        Ok(()) => "ready".into_response(),
        Err(err) => {
            warn!(error = %err, "store not ready");
            (StatusCode::SERVICE_UNAVAILABLE, "store unavailable").into_response()
        }
    }
}

async fn metrics() -> String {
    crate::metrics::render()
}

/// Builds the whole REST router.
pub(crate) fn router(state: Shared) -> Router {
    let guard = || axum::middleware::from_fn_with_state(Arc::clone(&state), auth::require);
    let protected = Router::new()
        .route("/v1/nodes", get(native::list_nodes))
        .route(
            "/v1/nodes/{node}",
            put(native::enrol_node).delete(native::delete_node),
        )
        .route("/v1/nodes/{node}/jobs", post(native::submit_job))
        .route(
            "/v1/nodes/{node}/config/{target}",
            // Only bundles may be large; set closest to the handler, this
            // limit replaces the small one below.
            put(native::put_config).layer(DefaultBodyLimit::max(state.body_limit)),
        )
        .route(
            "/v1/jobs/{id}",
            get(native::get_job).delete(native::cancel_job),
        )
        .route("/v1/jobs/{id}/events", get(native::job_events))
        .nest("/api", legacy::router())
        .layer(DefaultBodyLimit::max(SMALL_BODY))
        .layer(guard());
    // Metrics describe the activity: same access control unless opted out.
    let metrics_route = Router::new().route("/metrics", get(metrics));
    let metrics_route = if state.metrics_public {
        metrics_route
    } else {
        metrics_route.layer(guard())
    };
    // Probes stay reachable without credentials.
    Router::new()
        .route("/healthz", get(healthz))
        .route("/readyz", get(readyz))
        .merge(metrics_route)
        .merge(protected)
        .with_state(state)
}
