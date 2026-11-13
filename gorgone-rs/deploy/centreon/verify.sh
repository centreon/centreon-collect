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
  node=$(engine_node)
  gorgone_api POST "/v1/nodes/${node}/jobs" -H 'Content-Type: application/json' \
    -d "{\"engine_commands\":{\"commands\":[\"[$(date +%s)] DISABLE_NOTIFICATIONS\"]}}"
  sleep 3
  "${COMPOSE[@]}" logs engine | grep -q 'EXTERNAL COMMAND: DISABLE_NOTIFICATIONS'
}

engine_node() {
  sql "SELECT id FROM centreon.nagios_server WHERE name = 'engine'"
}

# The engine API must only be reachable through the socket shared with the
# sidecar.
engine_api_not_on_network() {
  "${COMPOSE[@]}" exec -T engine python3 -c '
import socket, sys
try:
    socket.create_connection(("127.0.0.1", 50155), timeout=2)
except OSError:
    sys.exit(0)
sys.exit(1)'
}

# What the agent reports must match what was pushed: applied configuration
# and a reachable engine, as served by gorgone-server.
node_state_is_reported() {
  gorgone_api GET "/v1/nodes/$(engine_node)" | python3 -I -c '
import json, sys
node = json.load(sys.stdin)
ok = node["connected"] and node["applied"].get("engine") and node["engine"]["reachable"]
sys.exit(0 if ok else 1)'
}

# A reload asked on top of a failed configuration push must not be sent.
reload_refused_after_failed_push() {
  local node archive job
  node=$(engine_node)
  archive=$(mktemp)
  tar -cf "${archive}" -C "$(dirname "$0")" verify.sh
  # No agent has this target: the push is rejected.
  web curl -s -u "${GORGONE_AUTH}" -X PUT -H 'Content-Type: application/x-tar' \
    --data-binary @- "http://gorgone-server:8085/v1/nodes/${node}/config/bench-missing" < "${archive}"
  rm -f "${archive}"
  job=$(gorgone_api POST "/v1/nodes/${node}/jobs" -H 'Content-Type: application/json' \
    -d '{"service_control":{"service":"engine","operation":"reload"},"requires_latest_config":["bench-missing"]}' \
    | python3 -I -c 'import json,sys; print(json.load(sys.stdin)["id"])')
  gorgone_api GET "/v1/jobs/${job}?wait=20" | python3 -I -c '
import json, sys
job = json.load(sys.stdin)
message = job["events"][-1]["data"].get("message", "")
sys.exit(0 if job["state"] == "failed" and "prerequisite" in message else 1)'
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
check "the engine API is not reachable over the network" engine_api_not_on_network
check "gorgone-server knows what the engine container runs" node_state_is_reported
check "a reload is refused after a failed configuration push" reload_refused_after_failed_push
check "centreon-web wrote no centcore command file" no_centcore_file

echo
echo "Jobs handled by gorgone-server:"
sql "SELECT CONCAT('  node ', node_id, '  ', RPAD(kind, 16, ' '), ' ', state) FROM gorgone.jobs ORDER BY created_at" 2> /dev/null

exit "${failures}"
