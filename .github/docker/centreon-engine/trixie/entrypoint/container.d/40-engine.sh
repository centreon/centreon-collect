#!/bin/sh

# Sourced by container.sh, which already sets `set -e` for the whole entrypoint.

PLUGINS_JSON="/etc/centreon-engine/plugins.json"

. /var/lib/centreon-engine/apt_install.sh

if [ -f "$PLUGINS_JSON" ]; then
    PKGS=$(python3 /var/lib/centreon-engine/check_plugins.py "$PLUGINS_JSON" 2>/dev/null)
    if [ -n "$PKGS" ]; then
        echo "Installing plugins from plugins.json: $PKGS"
        # Run in background so centengine starts without waiting for apt.
        # shellcheck disable=SC2086
        apt_install_pkgs $PKGS &
    else
        echo "plugins.json is empty or unreadable, skipping plugin install"
    fi
else
    echo "No plugins.json found at $PLUGINS_JSON, skipping plugin install"
fi
