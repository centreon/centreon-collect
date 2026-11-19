#!/bin/sh
# Tails cbd/cbwd's log file into stdout so `docker logs` shows activity
# (cbd never logs to stdout itself). Log path is derived from the JSON
# config (centreonBroker.log.directory/filename, falling back to
# /var/log/centreon-broker/<broker_name>.log) since it isn't fixed.
#
# Usage: log-tail.sh <path-to-config.json>, run in the background.
#        log-tail.sh --link-stdout <path-to-config.json>, run in the
#        foreground before cbd starts: symlinks the log to /dev/stdout
#        instead of tailing it.

MODE="tail"
if [ "$1" = "--link-stdout" ]; then
    MODE="link"
    shift
fi
CONFIG="$1"
DEFAULT_DIR="/var/log/centreon-broker"

[ -n "$CONFIG" ] && [ -f "$CONFIG" ] || exit 0

# Value of the first "<key>": "<string>" or "<key>": <number> of the
# config. No JSON parser in the image: the keys read here (directory,
# filename, max_size, broker_name) appear once in a broker configuration,
# under centreonBroker.log for the first three.
json_value() {
    tr -d '\n' < "$CONFIG" \
        | grep -o "\"$1\"[[:space:]]*:[[:space:]]*\(\"[^\"]*\"\|[0-9]*\)" \
        | head -n 1 \
        | sed 's/^[^:]*:[[:space:]]*//; s/^"//; s/"$//'
}

LOG_DIR=$(json_value directory)
LOG_DIR="${LOG_DIR:-$DEFAULT_DIR}"
LOG_FILE=$(json_value filename)
if [ -z "$LOG_FILE" ]; then
    LOG_FILE="$(json_value broker_name).log"
fi

[ -n "$LOG_FILE" ] && [ "$LOG_FILE" != ".log" ] || exit 0

LOG_PATH="${LOG_DIR%/}/$LOG_FILE"

if [ "$MODE" = "link" ]; then
    # log.max_size > 0: cbd rotates (renames) the log, moving the symlink
    # aside. Sinks are only built at start, so this check is enough.
    MAX_SIZE=$(json_value max_size)
    if [ "${MAX_SIZE:-0}" != "0" ]; then
        echo "WARNING: broker log rotation is on (log max_size=$MAX_SIZE), $LOG_PATH stays a file, tailing it instead of linking it to /dev/stdout" >&2
    elif ln -sfn /dev/stdout "$LOG_PATH"; then
        echo "Broker log $LOG_PATH -> /dev/stdout"
        exit 0
    else
        # Not linkable: fall back to tailing.
        echo "WARNING: could not link $LOG_PATH to /dev/stdout, tailing it instead" >&2
    fi
    # Drop a link left by an earlier start of this container before tailing.
    [ -L "$LOG_PATH" ] && rm -f "$LOG_PATH"
    "$0" "$CONFIG" &
    exit 0
fi

# Already stdout: avoid tailing our own output.
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
# setup.
exec tail -F "$LOG_PATH"
