#!/bin/sh

set -e

PLUGINS_JSON="/etc/centreon-engine/plugins.json"

# Install plugins listed in plugins.json (targeted, not greedy)
(
    export DEBIAN_FRONTEND=noninteractive
    sudo apt-get update -qq

    if [ -f "$PLUGINS_JSON" ]; then
        # Extract package names (keys) from JSON using python3
        PKGS=$(python3 -c "
import json, sys
try:
    d = json.load(open('$PLUGINS_JSON'))
    print(' '.join(k + '-*' for k in d.keys()))
except Exception:
    print('', end='')
" 2>/dev/null)
        if [ -n "$PKGS" ]; then
            echo "Installing plugins from plugins.json: $PKGS"
            eval "sudo apt-get install -y $PKGS" > /proc/1/fd/1 2>&1 || true
        else
            echo "plugins.json is empty or unreadable, skipping plugin install"
        fi
    else
        echo "No plugins.json found at $PLUGINS_JSON, skipping plugin install"
    fi
) &

# Use pre-installed Python venv from /opt/centreon/venv
# (fastapi and uvicorn are already installed in the Docker image)
export PATH="/opt/centreon/venv/bin:$PATH"
