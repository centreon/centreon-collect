#!/bin/sh

# Plugin install run by cbmod after a deployment committed a new plugins.json
# (env CENTREON_PLUGINS_INSTALL, set by the image). Replaces the inotify
# watcher (45-plugin-watcher).
#
# Usage: plugins-install.sh <plugins.json>
#
# Stdout, one line per package of plugins.json (read by cbmod):
#   installed <pkg> | uptodate <pkg> | failed <pkg> <short message>
# apt output goes to the container log (/proc/1/fd/1), never to stdout.

PLUGINS_JSON="$1"
LIB=/usr/local/lib/centreon-engine
PKG_RE='^[a-z0-9][a-z0-9+.-]+$'

[ -n "$PLUGINS_JSON" ] || { echo "usage: $0 <plugins.json>" >&2; exit 2; }
[ -r "$PLUGINS_JSON" ] || { echo "$PLUGINS_JSON not readable" >&2; exit 2; }

. "$LIB/apt_install.sh"

# Every package name of plugins.json, lowercase, one per line.
ALL=$(python3 -c '
import json, sys
with open(sys.argv[1]) as f:
    for name in json.load(f):
        print(str(name).lower())
' "$PLUGINS_JSON") || { echo "$PLUGINS_JSON is not a JSON object" >&2; exit 2; }

# Packages missing or outdated.
TODO=" $(python3 "$LIB/check_plugins.py" "$PLUGINS_JSON" 2>/dev/null) "

VALID=""
for p in $TODO; do
    if echo "$p" | grep -Eq "$PKG_RE"; then
        VALID="$VALID $p"
    fi
done
if [ -n "$VALID" ]; then
    echo "plugins-install: installing$VALID" > /proc/1/fd/1 2>/dev/null || true
    # shellcheck disable=SC2086
    apt_install_pkgs $VALID
fi

for p in $ALL; do
    if ! echo "$p" | grep -Eq "$PKG_RE"; then
        echo "failed $p invalid package name"
        continue
    fi
    case "$TODO" in
        *" $p "*) ;;
        *)
            echo "uptodate $p"
            continue
            ;;
    esac
    st=$(dpkg-query -W -f='${Status}' "$p" 2>/dev/null)
    if [ "$st" = "install ok installed" ]; then
        echo "installed $p"
    else
        echo "failed $p not installed by apt-get (unknown package or install error)"
    fi
done
exit 0
