#!/bin/sh

DEPS_JSON="/etc/centreon-engine/custom-deps.json"
DEPS_DIR="/etc/centreon-engine"

install_custom_deps() {
    PKGS=$(python3 /var/lib/centreon-engine/check_custom_deps.py "$DEPS_JSON")
    if [ -n "$PKGS" ]; then
        echo "custom-deps.json changed — installing: $PKGS"
        DEBIAN_FRONTEND=noninteractive sudo apt-get update -qq > /proc/1/fd/1 2>&1 || true
        # shellcheck disable=SC2086
        DEBIAN_FRONTEND=noninteractive sudo apt-get install -y -- $PKGS > /proc/1/fd/1 2>&1 || true
    else
        echo "custom-deps.json changed — no valid APT packages, skipping install"
    fi
}

# close_write fires when the file is closed after writing (content is complete).
# moved_to catches atomic replacements (write-then-rename used by config tools).
# Note: inotify-tools 4.x matches --include against the full event line
# (e.g. "/etc/centreon-engine/ MOVED_TO custom-deps.json"), so ^ anchor must be omitted.
inotifywait -m -q \
    -e close_write -e moved_to \
    --include 'custom-deps\.json$' \
    "$DEPS_DIR" |
while read -r _dir _event _file; do
    install_custom_deps
done
