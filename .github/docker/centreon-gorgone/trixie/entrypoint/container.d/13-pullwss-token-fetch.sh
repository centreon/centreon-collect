#!/bin/sh
# Optional: self-fetch a real Centreon API token for pullwss auth instead of
# hardcoding GORGONE_TOKEN, whose value is random on every install (POST
# /administration/tokens returns the plaintext only once, at creation; GET
# .../tokens/{name}[/users/{id}] returns it again for an existing token).
# Gated behind PULLWSS_TOKEN_NAME: unset/empty = no-op.
#
# Reserved for gorgone-central-poller (Central acting as its own poller)
# ONLY: it never goes through the "Add Poller" UI flow that hands a normal
# poller its own static token, so it has to self-discover one, which is why
# this needs Centreon API admin-style credentials in the first place. NEVER
# set PULLWSS_TOKEN_NAME on a classic/remote poller — it already has a static
# GORGONE_TOKEN, and admin credentials on top would only widen their exposure.
#
# Mechanism:
# - POST {base_url}/login -> {"security":{"token":...}}
#   (gorgone/gorgone/class/tpapi/centreonv2.pm:98-155, authenticate()).
# - GET {base_url}/administration/tokens/{name}[/users/{user_id}], header
#   X-AUTH-TOKEN: <session token> -> plaintext "token" field (centreon repo:
#   src/Core/Security/Token/Infrastructure/API/GetToken/GetTokenRoute.yaml).
#   Without PULLWSS_TOKEN_USER_ID this hits the "current user" route variant
#   instead of GetTokenByUserId. Works for a pre-existing token too, not only
#   right after creation.
# - gorgoned reads the plain GORGONE_TOKEN env var natively
#   (gorgone/gorgone/class/script.pm:364-370) as the pullwss module's token —
#   no GORGONE__ prefix for this one.
#
# Retries on transient failures only (API not up yet); a permanent error (bad
# credentials, unknown token/user) fails fast instead of burning the retry
# budget on something a retry can't fix.

set -e

if [ -z "${PULLWSS_TOKEN_NAME}" ]; then
    return 0
fi

# Everything from here on handles the password, the API session token, and
# the fetched pullwss token - tracing any of it (DEBUG=true sets -x on the
# whole entrypoint, inherited here) would print secrets straight into
# `docker logs`, starting with the plain CENTREON_API_PASSWORD validation
# just below. Turned off once for this whole section and restored on every
# exit path (see restore_trace()) instead of guarding each command
# individually, since almost every command below touches one of these values.
debug_was_on=0
if [ "${DEBUG}" = "true" ] || [ "${DEBUG}" = "1" ]; then
    debug_was_on=1
fi
set +x

restore_trace() {
    if [ "$debug_was_on" = "1" ]; then
        set -x
    fi
}

: "${CENTREON_API_BASE_URL:?ERROR: CENTREON_API_BASE_URL must be set when PULLWSS_TOKEN_NAME is set}"
: "${CENTREON_API_USERNAME:?ERROR: CENTREON_API_USERNAME must be set when PULLWSS_TOKEN_NAME is set}"
: "${CENTREON_API_PASSWORD:?ERROR: CENTREON_API_PASSWORD must be set when PULLWSS_TOKEN_NAME is set}"

BASE_URL="${CENTREON_API_BASE_URL%/}"
RETRIES="${PULLWSS_TOKEN_FETCH_RETRIES:-30}"
DELAY="${PULLWSS_TOKEN_FETCH_DELAY:-2}"

if [ -n "${PULLWSS_TOKEN_USER_ID}" ]; then
    TOKEN_ENDPOINT="${BASE_URL}/administration/tokens/${PULLWSS_TOKEN_NAME}/users/${PULLWSS_TOKEN_USER_ID}"
else
    TOKEN_ENDPOINT="${BASE_URL}/administration/tokens/${PULLWSS_TOKEN_NAME}"
fi

echo "=== Fetching pullwss token '${PULLWSS_TOKEN_NAME}' from Centreon API ==="

# Real parses via JSON::XS (already a dependency of this image, gorgoned
# itself uses it) rather than a hand-rolled regex, which risks matching the
# wrong "token" key if a response ever nests one under another name.
extract_login_token() {
    perl -MJSON::XS -0777 -ne '
        my $d = eval { decode_json($_) };
        print(($d && ref($d->{security}) eq "HASH" && defined($d->{security}{token})) ? $d->{security}{token} : "");
    '
}

extract_api_token() {
    perl -MJSON::XS -0777 -ne '
        my $d = eval { decode_json($_) };
        print(($d && defined($d->{token})) ? $d->{token} : "");
    '
}

# 000 (connection failure, curl could not even reach the server) and 5xx are
# worth retrying (API not up yet). 4xx means the request itself is wrong
# (bad creds, unknown token/user) and will not fix itself by waiting.
is_retryable() {
    case "$1" in
        000|5*) return 0 ;;
        *) return 1 ;;
    esac
}

session_token=""
last_code=""
attempt=0
while [ "$attempt" -lt "$RETRIES" ]; do
    attempt=$((attempt + 1))

    login_response=$(curl -s -w '\n%{http_code}' -X POST "${BASE_URL}/login" \
        -H 'Content-Type: application/json; charset=utf-8' \
        -d "{\"security\":{\"credentials\":{\"login\":\"${CENTREON_API_USERNAME}\",\"password\":\"${CENTREON_API_PASSWORD}\"}}}") || login_response=""

    http_code=$(printf '%s' "$login_response" | tail -n1)
    http_code="${http_code:-000}"
    body=$(printf '%s' "$login_response" | sed '$d')
    last_code="$http_code"

    if [ "$http_code" = "200" ]; then
        session_token=$(printf '%s' "$body" | extract_login_token)
        if [ -n "$session_token" ]; then
            break
        fi
    fi

    if ! is_retryable "$http_code"; then
        echo "ERROR: login to Centreon API rejected (http ${http_code}), not retrying" >&2
        restore_trace
        exit 1
    fi

    if [ "$attempt" -lt "$RETRIES" ]; then
        echo "  login attempt ${attempt}/${RETRIES} failed (http ${http_code}), retrying in ${DELAY}s..."
        sleep "$DELAY"
    fi
done

if [ -z "$session_token" ]; then
    echo "ERROR: could not log into Centreon API at ${BASE_URL} after ${RETRIES} attempts (last http code: ${last_code})" >&2
    restore_trace
    exit 1
fi

token_value=""
last_code=""
attempt=0
while [ "$attempt" -lt "$RETRIES" ]; do
    attempt=$((attempt + 1))

    fetch_response=$(curl -s -w '\n%{http_code}' \
        -H "X-AUTH-TOKEN: ${session_token}" \
        "${TOKEN_ENDPOINT}") || fetch_response=""

    http_code=$(printf '%s' "$fetch_response" | tail -n1)
    http_code="${http_code:-000}"
    body=$(printf '%s' "$fetch_response" | sed '$d')
    last_code="$http_code"

    if [ "$http_code" = "200" ]; then
        token_value=$(printf '%s' "$body" | extract_api_token)
        if [ -n "$token_value" ]; then
            break
        fi
    fi

    if ! is_retryable "$http_code"; then
        echo "ERROR: fetching token '${PULLWSS_TOKEN_NAME}' rejected (http ${http_code}), not retrying" >&2
        restore_trace
        exit 1
    fi

    if [ "$attempt" -lt "$RETRIES" ]; then
        echo "  token fetch attempt ${attempt}/${RETRIES} failed (http ${http_code}), retrying in ${DELAY}s..."
        sleep "$DELAY"
    fi
done

if [ -z "$token_value" ]; then
    echo "ERROR: could not fetch token '${PULLWSS_TOKEN_NAME}' from Centreon API after ${RETRIES} attempts (last http code: ${last_code})" >&2
    restore_trace
    exit 1
fi

export GORGONE_TOKEN="${PULLWSS_TOKEN_NAME}:${token_value}"
restore_trace
echo "✓ pullwss token fetched for '${PULLWSS_TOKEN_NAME}' (GORGONE_TOKEN set)"
echo ""
