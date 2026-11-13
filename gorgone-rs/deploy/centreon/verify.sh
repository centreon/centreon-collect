#!/usr/bin/env bash
# Checks the integration bench of compose.yaml once it is up.
set -euo pipefail

COMPOSE=(docker compose -f "$(dirname "$0")/compose.yaml")
GORGONE_AUTH=centreon-web:bench-only
failures=0

check() {
  local label=$1
  shift
  if "$@" > /dev/null 2>&1; then
    echo "ok    ${label}"
  else
    echo "FAIL  ${label}"
    failures=$((failures + 1))
  fi
}

web() {
  "${COMPOSE[@]}" exec -T web "$@"
}

gorgone_api() {
  local method=$1 path=$2
  shift 2
  web curl -s -u "${GORGONE_AUTH}" -X "${method}" "http://gorgone-server:8085${path}" "$@"
}

sql() {
  web mysql -hdb -uroot -pcentreon -N -e "$1"
}

all_nodes_connected() {
  gorgone_api GET /v1/nodes \
    | python3 -I -c 'import json,sys; n=json.load(sys.stdin)["nodes"]; sys.exit(0 if len(n) >= 3 and all(x["connected"] for x in n) else 1)'
}

instance_running() {
  sql "SELECT running FROM centreon_storage.instances WHERE name = '$1'" | grep -qx 1
}

# A forced check asked through the centreon-web API must reach the engine of
# the central poller: web -> Gorgone REST API -> agent -> engine.
web_command_reaches_engine() {
  local token
  token=$(web curl -s -X POST -H 'Content-Type: application/json' \
    -d '{"security":{"credentials":{"login":"admin","password":"Centreon!2021"}}}' \
    http://localhost/centreon/api/latest/login \
    | python3 -I -c 'import json,sys; print(json.load(sys.stdin)["security"]["token"])')
  web curl -s -f -X POST -H 'Content-Type: application/json' -H "X-AUTH-TOKEN: ${token}" \
    -d '{"check":{"is_forced":true},"resources":[{"type":"host","id":14,"parent":null}]}' \
    http://localhost/centreon/api/latest/monitoring/resources/check
  sleep 5
  web grep -q 'EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK' /var/log/centreon-engine/centengine.log
}

# An external command for the container-only poller goes through the gRPC
# API of its engine: there is no command pipe to write to.
grpc_command_reaches_engine() {
  local node
  node=$(sql "SELECT id FROM centreon.nagios_server WHERE name = 'engine'")
  gorgone_api POST "/v1/nodes/${node}/jobs" -H 'Content-Type: application/json' \
    -d "{\"engine_commands\":{\"commands\":[\"[$(date +%s)] DISABLE_NOTIFICATIONS\"]}}"
  sleep 3
  "${COMPOSE[@]}" logs engine | grep -q 'EXTERNAL COMMAND: DISABLE_NOTIFICATIONS'
}

no_centcore_file() {
  local files
  files=$(web sh -c 'find /var/lib/centreon/centcore /var/lib/centreon/centcore.cmd -type f 2>/dev/null')
  [[ -z "${files}" ]]
}

check "no Gorgone daemon runs in the web container" \
  bash -c "! ${COMPOSE[*]} exec -T web pgrep -f 'gorgoned|gorgone-server'"
check "central, poller and engine agents are connected to gorgone-server" all_nodes_connected
check "configuration was applied on the poller" \
  "${COMPOSE[@]}" exec -T poller test -s /etc/centreon-engine/.gorgone-engine.digest
check "the central sees the poller's engine running" instance_running poller
check "the central sees the engine container running" instance_running engine
check "a command from the centreon-web API reaches the central engine" web_command_reaches_engine
check "a command reaches the engine container over gRPC" grpc_command_reaches_engine
check "centreon-web wrote no centcore command file" no_centcore_file

echo
echo "Jobs handled by gorgone-server:"
sql "SELECT CONCAT('  node ', node_id, '  ', RPAD(kind, 16, ' '), ' ', state) FROM gorgone.jobs ORDER BY created_at" 2> /dev/null

exit "${failures}"
