#!/bin/sh

touch /tmp/docker.ready
echo "Centreon Engine is ready"

# Stream centengine log file to docker logs in background
# centengine logs to file (not stdout), so we tail it here
(
    LOG="/var/log/centreon-engine/centengine.log"
    # Wait up to 30s for the log file to appear
    waited=0
    while [ ! -f "$LOG" ] && [ "$waited" -lt 30 ]; do
        sleep 1
        waited=$((waited + 1))
    done
    if [ -f "$LOG" ]; then
        tail -F "$LOG" 2>/dev/null | while IFS= read -r line; do
            echo "[ENGINE-LOG] $line"
        done
    else
        echo "Warning: $LOG not found after 30s, log streaming disabled"
    fi
) &

exec "$@"
