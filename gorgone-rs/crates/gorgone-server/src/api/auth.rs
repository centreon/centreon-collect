//! REST access control: client network allowlist, then bearer or basic
//! credentials.

use std::net::SocketAddr;

use axum::extract::{ConnectInfo, Request, State};
use axum::http::{HeaderMap, StatusCode, header};
use axum::middleware::Next;
use axum::response::{IntoResponse, Response};
use base64::Engine;
use base64::engine::general_purpose::STANDARD;

use super::{ApiError, Shared};
use crate::config::RestAuthConfig;
use crate::secrets_match;

fn credentials_ok(auth: &RestAuthConfig, headers: &HeaderMap) -> bool {
    let Some((scheme, value)) = headers
        .get(header::AUTHORIZATION)
        .and_then(|v| v.to_str().ok())
        .and_then(|v| v.split_once(' '))
    else {
        return false;
    };
    let value = value.trim();
    if scheme.eq_ignore_ascii_case("bearer") {
        return auth
            .token
            .as_deref()
            .is_some_and(|token| secrets_match(value, token));
    }
    if !scheme.eq_ignore_ascii_case("basic") {
        return false;
    }
    let Some(decoded) = STANDARD
        .decode(value)
        .ok()
        .and_then(|raw| String::from_utf8(raw).ok())
    else {
        return false;
    };
    let Some((user, password)) = decoded.split_once(':') else {
        return false;
    };
    match (&auth.basic_username, &auth.basic_password) {
        // Evaluate both so that timing does not tell which one failed.
        (Some(expected_user), Some(expected_password)) => {
            let user_ok = secrets_match(user, expected_user);
            let password_ok = secrets_match(password, expected_password);
            user_ok && password_ok
        }
        _ => auth
            .token
            .as_deref()
            .is_some_and(|token| secrets_match(password, token)),
    }
}

/// Middleware guarding `/v1` and `/api`.
pub(super) async fn require(State(state): State<Shared>, request: Request, next: Next) -> Response {
    let auth = &state.auth;
    if !auth.allowed_cidrs.is_empty() {
        let client = request
            .extensions()
            .get::<ConnectInfo<SocketAddr>>()
            .map(|info| info.0.ip().to_canonical());
        let allowed =
            client.is_some_and(|ip| auth.allowed_cidrs.iter().any(|net| net.contains(&ip)));
        if !allowed {
            return ApiError::new(
                StatusCode::FORBIDDEN,
                "forbidden",
                "client address not allowed",
            )
            .into_response();
        }
    }
    if auth.requires_credentials() && !credentials_ok(auth, request.headers()) {
        let mut response = ApiError::new(
            StatusCode::UNAUTHORIZED,
            "unauthorized",
            "missing or invalid credentials",
        )
        .into_response();
        response.headers_mut().insert(
            header::WWW_AUTHENTICATE,
            header::HeaderValue::from_static("Bearer"),
        );
        return response;
    }
    next.run(request).await
}

#[cfg(test)]
mod tests {
    use super::*;

    fn headers(value: &str) -> HeaderMap {
        let mut headers = HeaderMap::new();
        headers.insert(header::AUTHORIZATION, value.parse().unwrap());
        headers
    }

    #[test]
    fn bearer_and_basic() {
        let auth = RestAuthConfig {
            token: Some("s3cret".into()),
            basic_username: Some("web".into()),
            basic_password: Some("pw".into()),
            allowed_cidrs: Vec::new(),
        };
        assert!(credentials_ok(&auth, &headers("Bearer s3cret")));
        assert!(!credentials_ok(&auth, &headers("Bearer nope")));
        let basic = |pair: &str| format!("Basic {}", STANDARD.encode(pair));
        assert!(credentials_ok(&auth, &headers(&basic("web:pw"))));
        assert!(!credentials_ok(&auth, &headers(&basic("web:s3cret"))));
        assert!(!credentials_ok(&auth, &HeaderMap::new()));

        // Without basic credentials the token is the password of any user.
        let token_only = RestAuthConfig {
            token: Some("s3cret".into()),
            ..RestAuthConfig::default()
        };
        assert!(credentials_ok(
            &token_only,
            &headers(&basic("anyone:s3cret"))
        ));
    }
}
