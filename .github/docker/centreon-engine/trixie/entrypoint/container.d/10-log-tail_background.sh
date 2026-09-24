#!/bin/sh

# centengine logs to a file (log_v2_logger=file in centengine.cfg — Engine has
# no "stdout" logger, only "file" or "syslog"). Stream that file into the
# container's stdout so `docker logs` shows checks/notifications/etc.,
# not just the startup config messages emitted before the file logger takes over.
CFG="/etc/centreon-engine/centengine.cfg"
DEFAULT_LOG="/var/log/centreon-engine/centengine.log"

LOG_FILE=$(grep -E '^log_file=' "$CFG" 2>/dev/null | cut -d= -f2-)
LOG_FILE="${LOG_FILE:-$DEFAULT_LOG}"

# If log_file already points at a stdout-like special path (e.g. /dev/stdout,
# /proc/1/fd/1), centengine is already writing straight to the container's
# stdout — tailing it would just be tailing our own output.
case "$LOG_FILE" in
    /dev/std*|/proc/*/fd/*)
        return 0
        ;;
esac

touch "$LOG_FILE" 2>/dev/null || true
exec tail -F "$LOG_FILE" > /proc/1/fd/1 2>&1
