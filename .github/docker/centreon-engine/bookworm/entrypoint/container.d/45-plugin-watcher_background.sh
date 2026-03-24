#!/bin/sh

PLUGINS_JSON="/etc/centreon-engine/plugins.json"
POLL_INTERVAL=30
last_hash=""

install_from_json() {
    PKGS=$(python3 /var/lib/centreon-engine/check_plugins.py "$PLUGINS_JSON")
    if [ -n "$PKGS" ]; then
        echo "plugins.json changed — installing: $PKGS"
        export DEBIAN_FRONTEND=noninteractive
        eval "sudo apt-get install -y $PKGS" > /proc/1/fd/1 2>&1 || true
    else
        echo "plugins.json changed — all plugins already up-to-date, skipping install"
    fi
}

while true; do
    sleep "$POLL_INTERVAL"
    if [ -f "$PLUGINS_JSON" ]; then
        current_hash=$(md5sum "$PLUGINS_JSON" 2>/dev/null | cut -d' ' -f1)
        if [ "$current_hash" != "$last_hash" ]; then
            last_hash="$current_hash"
            install_from_json
        fi
    fi
done
