#!/bin/sh

set -e

PLUGINS_JSON="/etc/centreon-engine/plugins.json"

# Install plugins listed in plugins.json (targeted, not greedy)
(
    export DEBIAN_FRONTEND=noninteractive
    sudo apt-get update -qq

    if [ -f "$PLUGINS_JSON" ]; then
        PKGS=$(python3 /var/lib/centreon-engine/check_plugins.py "$PLUGINS_JSON" 2>/dev/null)
        if [ -n "$PKGS" ]; then
            echo "Installing plugins from plugins.json: $PKGS"
            # shellcheck disable=SC2086
            sudo apt-get install -y -- $PKGS > /proc/1/fd/1 2>&1 || true
        else
            echo "plugins.json is empty or unreadable, skipping plugin install"
        fi
    else
        echo "No plugins.json found at $PLUGINS_JSON, skipping plugin install"
    fi
) &
