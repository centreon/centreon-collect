#!/bin/sh
# Write engine-context.json from APP_SECRET and SALT env vars.
# Only runs in poller mode. Requires a shared volume with the centengine container
# mounted at /etc/centreon-engine/.

set -e

if [ "$TYPE" != "poller" ]; then
    echo "=== Engine Secrets: not poller mode, skipping ==="
    return 0
fi

echo "=== Writing Engine Secrets ==="

APP_SECRET="${APP_SECRET:?ERROR: APP_SECRET must be set for poller mode}"
SALT="${SALT:?ERROR: SALT must be set for poller mode}"
ENGINE_CONTEXT="/etc/centreon-engine/engine-context.json"

# centreon-gorgone is in the centreon-engine group, and /etc/centreon-engine/
# is group-writable (775), so it can remove/recreate this file.
rm -f "$ENGINE_CONTEXT"
printf '{"app_secret":"%s","salt":"%s"}\n' "$APP_SECRET" "$SALT" > "$ENGINE_CONTEXT"
chmod 640 "$ENGINE_CONTEXT"

echo "✓ Engine secrets written to $ENGINE_CONTEXT"
echo ""
