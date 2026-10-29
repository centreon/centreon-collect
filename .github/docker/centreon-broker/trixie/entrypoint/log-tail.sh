#!/bin/sh
# Tails cbd/cbwd's log file into stdout so `docker logs` shows activity
# (cbd never logs to stdout itself). Log path is derived from the JSON
# config (centreonBroker.log.directory/filename, falling back to
# /var/log/centreon-broker/<broker_name>.log) since it isn't fixed.
#
# Usage: log-tail.sh <path-to-config.json>, run in the background.
#        log-tail.sh --link-stdout <path-to-config.json>, run in the
#        foreground before cbd starts: replaces the log file with a symlink
#        to /dev/stdout, so cbd writes straight to the container's stdout
#        (its log directory must stay writable: /dev itself is refused).

MODE=tail
if [ "$1" = "--link-stdout" ]; then
    MODE=link
    shift
fi
CONFIG="$1"
DEFAULT_DIR="/var/log/centreon-broker"

[ -n "$CONFIG" ] && [ -f "$CONFIG" ] || exit 0

LOG_DIR=$(python3 -c "
import json
try:
    with open('$CONFIG') as f:
        c = json.load(f)
    print(c.get('centreonBroker', {}).get('log', {}).get('directory') or '')
except Exception:
    pass
" 2>/dev/null)
LOG_DIR="${LOG_DIR:-$DEFAULT_DIR}"

LOG_FILE=$(python3 -c "
import json
try:
    with open('$CONFIG') as f:
        c = json.load(f)
    cb = c.get('centreonBroker', {})
    name = cb.get('log', {}).get('filename') or ''
    if not name:
        name = cb.get('broker_name', '') + '.log'
    print(name)
except Exception:
    pass
" 2>/dev/null)

[ -n "$LOG_FILE" ] && [ "$LOG_FILE" != ".log" ] || exit 0

LOG_PATH="$LOG_DIR/$LOG_FILE"

if [ "$MODE" = "link" ]; then
    if ln -sfn /dev/stdout "$LOG_PATH"; then
        echo "Broker log $LOG_PATH -> /dev/stdout"
        exit 0
    fi
    # Not linkable (e.g. a log directory cbd itself would refuse): keep the
    # file and tail it as without BROKER_LOG_STDOUT.
    echo "WARNING: could not link $LOG_PATH to /dev/stdout, tailing it instead" >&2
    "$0" "$CONFIG" &
    exit 0
fi

# Already the container's stdout (symlink, /dev/stdout, /proc/<pid>/fd/1):
# tailing it would read the container's own output.
[ -L "$LOG_PATH" ] && exit 0
case "$LOG_PATH" in
    /dev/std*|/proc/*/fd/*) exit 0 ;;
esac

# Wait for cbd to create the file rather than racing it.
i=0
while [ ! -f "$LOG_PATH" ] && [ "$i" -lt 30 ]; do
    sleep 1
    i=$((i + 1))
done
touch "$LOG_PATH" 2>/dev/null || true

# No explicit /proc/1/fd/1 redirect: this script is backgrounded directly
# from container.sh (plain `&`, no stdout redirection in between), so its
# own stdout/stderr are already the container's real stdout/stderr by
# normal fd inheritance - reaching into /proc/1/fd/1 is both unnecessary and
# can fail with "Permission denied" depending on the container runtime/init
# setup (confirmed: a direct `cbd`-owned PID 1 in one environment; real
# failure observed once PID 1 became the api_control.py wrapper instead).
exec tail -F "$LOG_PATH"
