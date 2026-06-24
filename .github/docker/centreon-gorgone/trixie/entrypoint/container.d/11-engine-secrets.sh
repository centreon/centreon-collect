#!/bin/sh
# Write engine-context.json from APP_SECRET and SALT env vars.
# Only runs in poller mode. Requires a shared volume with the centengine container
# mounted at /etc/centreon-engine/.

set -e

TYPE="${TYPE:-central}"

if [ "$TYPE" != "poller" ]; then
    echo "=== Engine Secrets: central mode, skipping ==="
    exit 0
fi

echo "=== Writing Engine Secrets ==="

APP_SECRET="${APP_SECRET:?ERROR: APP_SECRET must be set for poller mode}"
SALT="${SALT:?ERROR: SALT must be set for poller mode}"
ENGINE_CONTEXT="/etc/centreon-engine/engine-context.json"

# engine-context.json may be owned by centreon-engine (644) from the image build.
# The /etc/centreon-engine/ directory is group-writable (775, centreon-engine group),
# and centreon-gorgone belongs to that group, so we can remove and recreate the file.
rm -f "$ENGINE_CONTEXT"
printf '{"app_secret":"%s","salt":"%s"}\n' "$APP_SECRET" "$SALT" > "$ENGINE_CONTEXT"
chmod 640 "$ENGINE_CONTEXT"

echo "✓ Engine secrets written to $ENGINE_CONTEXT"
echo ""
