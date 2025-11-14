#!/bin/sh
# Configuration availability checker and fetcher
# This script runs before 10-create-config.sh in the container startup sequence
# Handles two scenarios:
#   1. USE_ETCD=true  -> Fetch configuration from etcd
#   2. USE_ETCD=false -> Wait for configuration from shared volume

set -e

# Configuration
TYPE="${TYPE:-central}"
USE_ETCD="${USE_ETCD:-false}"
ETCD_HOST="${ETCD_HOST:-etcd}"
ETCD_PORT="${ETCD_PORT:-2379}"
CONFIG_DIR="${CONFIG_DIR:-/etc/centreon/config.d}"
MAX_RETRIES="${MAX_RETRIES:-10}"
RETRY_DELAY="${RETRY_DELAY:-2}"
CONFIG_WAIT_TIMEOUT="${CONFIG_WAIT_TIMEOUT:-300}"
DATABASE_CONFIG="${CONFIG_DIR}/10-database.yaml"

echo "=== Configuration Availability Check ==="
echo "Type: ${TYPE}"
echo "Config dir: ${CONFIG_DIR}"

# Pollers don't need database configuration - skip to end
if [ "$TYPE" != "poller" ]; then
    echo "Required config: ${DATABASE_CONFIG}"
    echo ""
fi

# Ensure config directory exists and is writable
if [ ! -d "$CONFIG_DIR" ]; then
    echo "Creating config directory: $CONFIG_DIR"
    mkdir -p "$CONFIG_DIR" || {
        echo "ERROR: Cannot create config directory"
        exit 1
    }
fi

# Check if directory is writable
if [ ! -w "$CONFIG_DIR" ]; then
    echo "WARNING: Config directory is not writable"
    echo "Current user: $(whoami) (UID=$(id -u))"
    ls -ld "$CONFIG_DIR"
fi

################################################################################
# Database configuration is only needed for central mode
################################################################################
if [ "$TYPE" != "poller" ]; then

################################################################################
# SCENARIO 1: etcd Integration
################################################################################
if [ "$USE_ETCD" = "true" ] || [ "$USE_ETCD" = "1" ]; then
    echo "Mode: etcd integration (USE_ETCD=${USE_ETCD})"
    echo "etcd: http://${ETCD_HOST}:${ETCD_PORT}"
    echo ""

    # Function to fetch a single key from etcd
    fetch_from_etcd() {
        local etcd_key="$1"
        local output_file="$2"
        local attempt=1

        echo "Fetching: $etcd_key -> $output_file"

        while [ $attempt -le $MAX_RETRIES ]; do
            echo "  Attempt $attempt of $MAX_RETRIES..."

            # Base64 encode the key
            local key_b64=$(printf '%s' "$etcd_key" | base64 | tr -d '\n')

            # Fetch from etcd using wget (lighter than curl on Alpine)
            local response=$(wget -q -O - \
                --post-data="{\"key\":\"${key_b64}\"}" \
                --header="Content-Type: application/json" \
                "http://${ETCD_HOST}:${ETCD_PORT}/v3/kv/range" 2>/dev/null || echo "")

            if [ -n "$response" ]; then
                # Extract value using grep and sed (no jq on minimal Alpine)
                local value_b64=$(echo "$response" | grep -o '"value":"[^"]*"' | head -1 | sed 's/"value":"\([^"]*\)"/\1/')

                if [ -n "$value_b64" ]; then
                    # Decode and write to file
                    local dir_path="$(dirname "$output_file")"

                    echo "  Debug: Checking directory permissions"
                    echo "  Current user: $(whoami) (UID=$(id -u), GID=$(id -g))"
                    echo "  Target directory: $dir_path"

                    # Create directory if needed
                    if [ ! -d "$dir_path" ]; then
                        echo "  Creating directory: $dir_path"
                        mkdir -p "$dir_path" 2>/dev/null || {
                            echo "  ✗ Failed to create directory"
                            ls -ld "$(dirname "$dir_path")" 2>/dev/null || echo "  Parent directory doesn't exist"
                            return 1
                        }
                    fi

                    # Show directory permissions
                    ls -ld "$dir_path"

                    # Try to write the file
                    if echo "$value_b64" | base64 -d > "$output_file" 2>/dev/null; then
                        # Set proper permissions (best effort)
                        chown centreon-gorgone:centreon "$output_file" 2>/dev/null || true
                        chmod 644 "$output_file" 2>/dev/null || true

                        echo "  ✓ Fetched successfully ($(wc -c < "$output_file") bytes)"

                        # Debug: Show file content if DEBUG is enabled
                        if [ "${DEBUG}" = "true" ] || [ "${DEBUG}" = "1" ]; then
                            echo "  Debug: File content after fetch:"
                            cat "$output_file" | head -20
                            echo ""
                            echo "  Debug: File permissions:"
                            ls -l "$output_file"
                            echo ""
                        fi

                        return 0
                    else
                        echo "  ✗ Failed to write file (permission denied)"
                        return 1
                    fi
                fi
            fi

            echo "  Failed, retrying in ${RETRY_DELAY}s..."
            sleep $RETRY_DELAY
            attempt=$((attempt + 1))
        done

        echo "  ✗ Failed to fetch after $MAX_RETRIES attempts"
        return 1
    }

    # Configuration files to fetch from etcd
    # Format: etcd_key:local_file_path
    CONFIGS="
    /centreon/config/10-database.yaml:${CONFIG_DIR}/10-database.yaml
    "

    # Add more configs here if needed, for example:
    # /centreon/config/20-broker.yaml:${CONFIG_DIR}/20-broker.yaml
    # /centreon/config/30-engine.yaml:${CONFIG_DIR}/30-engine.yaml

    echo "Fetching configuration from etcd..."
    failed=0
    for config in $CONFIGS; do
        if [ -z "$config" ]; then
            continue
        fi

        etcd_key="${config%%:*}"
        output_file="${config##*:}"

        if ! fetch_from_etcd "$etcd_key" "$output_file"; then
            failed=$((failed + 1))
        fi
    done

    if [ $failed -gt 0 ]; then
        echo ""
        echo "✗ Failed to fetch $failed configuration file(s) from etcd"
        echo "Cannot continue without configuration"
        exit 1
    fi

    echo ""
    echo "✓ All configurations fetched successfully from etcd"
    echo "Configuration files are now available at: ${CONFIG_DIR}"

################################################################################
# SCENARIO 2: Shared Volume (Traditional)
################################################################################
else
    echo "Mode: Shared volume (USE_ETCD=${USE_ETCD})"
    echo "Waiting for configuration from shared volume..."
    echo ""

    # Wait for database config file with timeout
    ELAPSED=0
    while [ ! -f "$DATABASE_CONFIG" ]; do
        if [ $ELAPSED -ge $CONFIG_WAIT_TIMEOUT ]; then
            echo "✗ ERROR: Timeout waiting for ${DATABASE_CONFIG} after ${CONFIG_WAIT_TIMEOUT}s"
            exit 1
        fi
        echo "Waiting for ${DATABASE_CONFIG} to be present... (${ELAPSED}s/${CONFIG_WAIT_TIMEOUT}s)"
        sleep 2
        ELAPSED=$((ELAPSED + 2))
    done

    echo "✓ Database config file found after ${ELAPSED}s"
    echo ""

    # Verify the database config is readable
    if [ ! -r "$DATABASE_CONFIG" ]; then
        echo "✗ ERROR: Database config file exists but is not readable!"
        ls -l "$DATABASE_CONFIG"
        echo "Current user: $(whoami)"
        echo "Current groups: $(groups)"
        exit 1
    fi
    echo "✓ Database config file is readable"

    # Show database config permissions for debugging
    if [ "${DEBUG}" = "true" ] || [ "${DEBUG}" = "1" ]; then
        echo ""
        echo "Debug: Database config file details:"
        ls -l "$DATABASE_CONFIG"
    fi
fi

fi  # End of TYPE != "poller" check

echo ""
if [ "$TYPE" = "poller" ]; then
    echo "✓ Configuration check complete (poller mode - database config skipped)"
else
    echo "✓ Configuration check complete"
fi
echo ""
