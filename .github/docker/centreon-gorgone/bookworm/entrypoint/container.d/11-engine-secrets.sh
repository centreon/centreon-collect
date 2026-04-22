#!/bin/sh
# Fetch engine secrets from Centreon central API and write engine-context.json.
# Only runs in poller mode. Requires a shared volume with the centengine container
# mounted at /etc/centreon-engine/.

set -e

TYPE="${TYPE:-central}"

if [ "$TYPE" != "poller" ]; then
    echo "=== Engine Secrets: central mode, skipping ==="
    exit 0
fi

echo "=== Fetching Engine Secrets from Central ==="

CENTRAL_ADDRESS="${CENTRAL_ADDRESS:?ERROR: CENTRAL_ADDRESS must be set for poller mode}"
CENTREON_WEB_PORT="${CENTREON_WEB_PORT:-80}"
CENTREON_WEB_PREFIX="${CENTREON_WEB_PREFIX:-/centreon}"
CENTREON_API_URL="${CENTREON_API_URL:-http://${CENTRAL_ADDRESS}:${CENTREON_WEB_PORT}${CENTREON_WEB_PREFIX}}"
WEB_API_USERNAME="${WEB_API_USERNAME:?ERROR: WEB_API_USERNAME must be set}"
WEB_API_PASSWORD="${WEB_API_PASSWORD:?ERROR: WEB_API_PASSWORD must be set}"
ENGINE_CONTEXT="/etc/centreon-engine/engine-context.json"
MAX_RETRIES="${ENGINE_SECRETS_MAX_RETRIES:-10}"
RETRY_DELAY="${ENGINE_SECRETS_RETRY_DELAY:-5}"

echo "Central API : $CENTREON_API_URL"
echo ""

# Authenticate with retry (central may not be up yet)
attempt=1
TOKEN=""
while [ $attempt -le "$MAX_RETRIES" ]; do
    echo "Authentication attempt $attempt/$MAX_RETRIES..."
    TOKEN=$(curl -s -X POST \
        -H "Content-Type: application/json" \
        -d "{\"security\":{\"credentials\":{\"login\":\"${WEB_API_USERNAME}\",\"password\":\"${WEB_API_PASSWORD}\"}}}" \
        "${CENTREON_API_URL}/api/latest/login" 2>/dev/null | \
        grep -o '"token":"[^"]*' | cut -d'"' -f4)

    if [ -n "$TOKEN" ] && [ "$TOKEN" != "null" ]; then
        echo "  ✓ Authentication successful"
        break
    fi

    echo "  Authentication failed, retrying in ${RETRY_DELAY}s..."
    sleep "$RETRY_DELAY"
    attempt=$((attempt + 1))
done

if [ -z "$TOKEN" ] || [ "$TOKEN" = "null" ]; then
    echo "✗ ERROR: Authentication failed after $MAX_RETRIES attempts"
    exit 1
fi

# Fetch engine secrets
echo "Fetching engine secrets..."
HTTP_STATUS=$(curl -s \
    -o /tmp/engine-secrets.json \
    -w "%{http_code}" \
    -H "X-AUTH-TOKEN: ${TOKEN}" \
    "${CENTREON_API_URL}/api/latest/administration/engine/secrets")

if [ "$HTTP_STATUS" != "200" ]; then
    echo "✗ ERROR: Failed to fetch engine secrets (HTTP $HTTP_STATUS)"
    cat /tmp/engine-secrets.json >&2
    rm -f /tmp/engine-secrets.json
    exit 1
fi

# engine-context.json may be owned by centreon-engine (644) from the image build.
# The /etc/centreon-engine/ directory is group-writable (775, centreon-engine group),
# and centreon-gorgone belongs to that group, so we can remove and recreate the file.
rm -f "$ENGINE_CONTEXT"
cp /tmp/engine-secrets.json "$ENGINE_CONTEXT"
chmod 640 "$ENGINE_CONTEXT"
rm -f /tmp/engine-secrets.json

echo "✓ Engine secrets written to $ENGINE_CONTEXT"
if [ "${DEBUG}" = "true" ] || [ "${DEBUG}" = "1" ]; then
    echo "Debug: $(cat "$ENGINE_CONTEXT")"
fi
echo ""
