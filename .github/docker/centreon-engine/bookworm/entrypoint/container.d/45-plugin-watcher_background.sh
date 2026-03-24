#!/bin/sh

PLUGINS_JSON="/etc/centreon-engine/plugins.json"
POLL_INTERVAL=30
last_hash=""

install_from_json() {
    PKGS=$(python3 -c "
import json, sys
try:
    d = json.load(open('$PLUGINS_JSON'))
    print(' '.join(k + '-*' for k in d.keys()))
except Exception:
    print('', end='')
" 2>/dev/null)
    if [ -n "$PKGS" ]; then
        echo "plugins.json changed — installing: $PKGS"
        export DEBIAN_FRONTEND=noninteractive
        sudo apt-get update -qq
        eval "sudo apt-get install -y $PKGS" > /proc/1/fd/1 2>&1 || true
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
