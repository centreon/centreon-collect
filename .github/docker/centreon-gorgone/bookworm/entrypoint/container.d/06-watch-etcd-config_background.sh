#!/bin/sh
# Background script to watch etcd for configuration changes
# Automatically reloads Gorgone when configuration is updated

set -e

USE_ETCD="${USE_ETCD:-false}"

# Only run if etcd is enabled
if [ "$USE_ETCD" != "true" ] && [ "$USE_ETCD" != "1" ]; then
    echo "=== etcd Config Watch Disabled ==="
    echo "etcd integration disabled (USE_ETCD=${USE_ETCD})"
    exit 0
fi

ETCD_HOST="${ETCD_HOST:-etcd}"
ETCD_PORT="${ETCD_PORT:-2379}"
CONFIG_DIR="${CONFIG_DIR:-/etc/centreon/config.d}"
DATABASE_CONFIG="${CONFIG_DIR}/10-database.yaml"
WATCH_KEY="/centreon/config/10-database.yaml"

echo "=== etcd Configuration Watch Started ==="
echo "Watching: $WATCH_KEY"
echo "Target: $DATABASE_CONFIG"
echo "etcd: http://${ETCD_HOST}:${ETCD_PORT}"
echo ""

# Function to fetch updated config from etcd
fetch_updated_config() {
    local etcd_key="$1"
    local output_file="$2"

    echo "[$(date '+%Y-%m-%d %H:%M:%S')] Detected configuration change"

    # Base64 encode the key
    local key_b64=$(printf '%s' "$etcd_key" | base64 | tr -d '\n')

    # Fetch from etcd
    local response=$(wget -q -O - \
        --post-data="{\"key\":\"${key_b64}\"}" \
        --header="Content-Type: application/json" \
        "http://${ETCD_HOST}:${ETCD_PORT}/v3/kv/range" 2>/dev/null || echo "")

    if [ -n "$response" ]; then
        # Extract value
        local value_b64=$(echo "$response" | grep -o '"value":"[^"]*"' | head -1 | sed 's/"value":"\([^"]*\)"/\1/')

        if [ -n "$value_b64" ]; then
            # Backup current config
            if [ -f "$output_file" ]; then
                cp "$output_file" "${output_file}.bak"
            fi

            # Write new config
            if echo "$value_b64" | base64 -d > "$output_file" 2>/dev/null; then
                chmod 644 "$output_file"
                chown centreon-gorgone:centreon "$output_file" 2>/dev/null || true
                echo "  ✓ Updated configuration file"
                return 0
            else
                # Restore backup on failure
                if [ -f "${output_file}.bak" ]; then
                    mv "${output_file}.bak" "$output_file"
                fi
                echo "  ✗ Failed to write updated config"
                return 1
            fi
        fi
    fi

    echo "  ✗ Failed to fetch updated config from etcd"
    return 1
}

# Function to reload Gorgone
reload_gorgone() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] Reloading Gorgone configuration..."

    # Find gorgoned process
    local gorgone_pid=$(pgrep -f "gorgoned.*--config" || echo "")

    if [ -n "$gorgone_pid" ]; then
        # Send SIGHUP to reload configuration
        # Note: Not all applications support SIGHUP reload
        # If gorgoned doesn't support it, we may need to restart the container
        kill -HUP "$gorgone_pid" 2>/dev/null && {
            echo "  ✓ Sent SIGHUP to gorgoned (PID: $gorgone_pid)"
            return 0
        } || {
            echo "  ⚠ SIGHUP failed, trying graceful restart..."
            kill -TERM "$gorgone_pid" 2>/dev/null
            sleep 2

            # The container should restart gorgoned automatically
            echo "  ✓ Sent SIGTERM to gorgoned - container will restart it"
            return 0
        }
    else
        echo "  ⚠ Gorgone process not found (may not be started yet)"
        return 1
    fi
}

# Get initial revision to start watching from
initial_revision=$(wget -q -O - \
    --post-data='{"key":"'$(printf '%s' "$WATCH_KEY" | base64 | tr -d '\n')'"}' \
    --header="Content-Type: application/json" \
    "http://${ETCD_HOST}:${ETCD_PORT}/v3/kv/range" 2>/dev/null | \
    grep -o '"mod_revision":"[0-9]*"' | head -1 | sed 's/"mod_revision":"\([0-9]*\)"/\1/' || echo "0")

echo "Starting watch from revision: ${initial_revision}"
echo ""

# Watch for changes using etcd v3 watch API
# This will block and output events as they happen
watch_key_b64=$(printf '%s' "$WATCH_KEY" | base64 | tr -d '\n')

while true; do
    # Use watch API to get notified of changes
    # This is more efficient than polling
    response=$(wget -q -O - --timeout=0 \
        --post-data='{"create_request":{"key":"'${watch_key_b64}'","start_revision":'${initial_revision}'}}' \
        --header="Content-Type: application/json" \
        "http://${ETCD_HOST}:${ETCD_PORT}/v3/watch" 2>/dev/null || echo "")

    if [ -n "$response" ]; then
        # Check if this is a PUT event (config change)
        if echo "$response" | grep -q '"type":"PUT"'; then
            echo ""
            echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
            echo "Configuration change detected in etcd!"
            echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

            # Fetch updated config
            if fetch_updated_config "$WATCH_KEY" "$DATABASE_CONFIG"; then
                # Give it a moment for filesystem to sync
                sleep 1

                # Reload Gorgone
                reload_gorgone

                echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
                echo ""
            fi

            # Update revision for next watch
            initial_revision=$((initial_revision + 1))
        fi
    fi

    # If watch connection drops, reconnect after short delay
    sleep 5
done
