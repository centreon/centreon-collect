#!/bin/sh

PLUGINS_JSON="/etc/centreon-engine/plugins.json"
PLUGINS_DIR="/etc/centreon-engine"
APT_LOCK="/var/lock/centreon-engine-apt.lock"

install_from_json() {
    PKGS=$(python3 /var/lib/centreon-engine/check_plugins.py "$PLUGINS_JSON")
    if [ -n "$PKGS" ]; then
        echo "plugins.json changed — installing: $PKGS"
        (
            flock -w 120 9 || { echo "apt lock timeout, skipping plugin install"; exit 0; }
            DEBIAN_FRONTEND=noninteractive sudo apt-get update -qq > /proc/1/fd/1 2>&1 || true
            # shellcheck disable=SC2086
            DEBIAN_FRONTEND=noninteractive sudo apt-get install -y -- $PKGS > /proc/1/fd/1 2>&1 || true
        ) 9>"${APT_LOCK}"
    else
        echo "plugins.json changed — all plugins already up-to-date, skipping install"
    fi
}

# close_write fires when the file is closed after writing (content is complete).
# moved_to catches atomic replacements (write-then-rename used by config tools).
# Note: inotify-tools 4.x matches --include against the full event line
# (e.g. "/etc/centreon-engine/ MOVED_TO plugins.json"), so ^ anchor must be omitted.
inotifywait -m -q \
    -e close_write -e moved_to \
    --include 'plugins\.json$' \
    "$PLUGINS_DIR" |
while read -r _dir _event _file; do
    install_from_json
done
