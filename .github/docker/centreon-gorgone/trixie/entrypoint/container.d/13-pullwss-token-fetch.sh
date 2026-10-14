#!/bin/sh
# Self-fetch a real Centreon API token for pullwss auth instead of a static
# GORGONE_TOKEN. Gated behind PULLWSS_TOKEN_NAME (unset = no-op). Reserved
# for gorgone-central-poller: it skips the "Add Poller" flow that hands a
# normal poller its static token, so it self-discovers one via the API —
# NEVER set this on a classic/remote poller, which already has a token.
#
# POST {base_url}/login -> session token (centreonv2.pm:98-155). GET
# {base_url}/administration/tokens/{name}[/users/{user_id}] with that token
# -> plaintext token (works for a pre-existing token too). gorgoned reads
# plain GORGONE_TOKEN natively (script.pm:364-370) — no GORGONE__ prefix.
#
# Retries only on transient failure (API not up yet); bad creds/unknown
# token fail fast.

set -e

if [ -z "${PULLWSS_TOKEN_NAME}" ]; then
    return 0
fi

# set +x: everything below touches the password/tokens; DEBUG=true must not
# trace it into docker logs.
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

# JSON::XS (already a gorgone dependency), not a regex — avoids matching the
# wrong "token" key if the response nests one elsewhere.
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

# 000/5xx: API not up yet, worth retrying. 4xx: wrong request, won't fix itself.
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
