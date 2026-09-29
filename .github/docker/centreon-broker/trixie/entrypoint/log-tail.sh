#!/bin/sh
# cbd/cbwd log to a file, never to stdout (no "stdout" logger type). Stream
# that file into the container's stdout so `docker logs` shows broker
# activity, not just silence.
#
# The log path isn't fixed: broker/core/src/config/parser.cc derives it from
# the JSON config at startup —
#   - directory: centreonBroker.log.directory, falls back to
#     /var/log/centreon-broker if absent/empty (parser.cc:392-401)
#   - filename: centreonBroker.log.filename, falls back to
#     "<centreonBroker.broker_name>.log" if left empty — which the shipped
#     config templates do by default (parser.cc:545-546,
#     broker/config/central-broker.json.in:4,15)
# so this script re-derives the same path from the same config file rather
# than assuming a fixed name.
#
# Usage: log-tail.sh <path-to-config.json>  — run in the background, before
# exec'ing cbd/cbwd with that same config file.

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

# cbd creates the file itself once its logger initializes — wait for it
# rather than racing it (mirrors the equivalent wait in the centreon-engine
# image's own log-tail script).
i=0
while [ ! -f "$LOG_PATH" ] && [ "$i" -lt 30 ]; do
    sleep 1
    i=$((i + 1))
done
touch "$LOG_PATH" 2>/dev/null || true

exec tail -F "$LOG_PATH" > /proc/1/fd/1 2>&1
