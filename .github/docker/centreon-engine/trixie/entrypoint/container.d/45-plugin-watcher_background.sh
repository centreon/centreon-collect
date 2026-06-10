#!/bin/sh

PLUGINS_JSON="/etc/centreon-engine/plugins.json"
PLUGINS_DIR="/etc/centreon-engine"

install_from_json() {
    PKGS=$(python3 /var/lib/centreon-engine/check_plugins.py "$PLUGINS_JSON")
    if [ -n "$PKGS" ]; then
        echo "plugins.json changed — installing: $PKGS"
        DEBIAN_FRONTEND=noninteractive eval "sudo apt-get install -y $PKGS" > /proc/1/fd/1 2>&1 || true
    else
        echo "plugins.json changed — all plugins already up-to-date, skipping install"
    fi
}

# close_write fires when the file is closed after writing (content is complete).
# moved_to catches atomic replacements (write-then-rename used by config tools).
inotifywait -m -q \
    -e close_write -e moved_to \
    --include '^plugins\.json$' \
    "$PLUGINS_DIR" |
while read -r _dir _event _file; do
    install_from_json
done
