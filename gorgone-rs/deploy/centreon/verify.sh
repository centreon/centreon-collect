#!/usr/bin/env bash
# Checks the integration bench of compose.yaml once it is up. Runs from the
# host: the web image is too small to carry test tools.
set -euo pipefail

COMPOSE=(docker compose -f "$(dirname "$0")/compose.yaml")
GORGONE_URL=http://127.0.0.1:8085
GORGONE_AUTH=centreon-web:bench-only
WEB_URL=http://127.0.0.1:4000/centreon
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

gorgone_api() {
  local method=$1 path=$2
  shift 2
  curl -s -u "${GORGONE_AUTH}" -X "${method}" "${GORGONE_URL}${path}" "$@"
}

sql() {
  "${COMPOSE[@]}" exec -T db mariadb -uroot -pcentreon -N -e "$1"
}

web_token() {
  curl -s -X POST -H 'Content-Type: application/json' \
    -d '{"security":{"credentials":{"login":"admin","password":"Centreon!2021"}}}' \
    "${WEB_URL}/api/latest/login" \
    | python3 -I -c 'import json,sys; print(json.load(sys.stdin)["security"]["token"])'
}

clapi() {
  curl -s -m 120 -X POST -H 'Content-Type: application/json' -H "centreon-auth-token: $(web_token)" \
    -d "{\"action\":\"$1\",\"values\":\"$2\"}" \
    "${WEB_URL}/api/index.php?action=action&object=centreon_clapi"
}

node_of() {
  sql "SELECT id FROM centreon.nagios_server WHERE name = '$1'"
}

# The web image holds PHP and Apache only: no collect, Gorgone or plugin
# package, and none of their processes.
web_image_is_slim() {
  ! "${COMPOSE[@]}" exec -T web sh -c \
    "rpm -qa | grep -iE '^centreon-(engine|broker|gorgone|poller|plugin|connector|clib|collect)'"
  ! "${COMPOSE[@]}" exec -T web sh -c "ls /usr/sbin/centengine /usr/sbin/cbd /usr/bin/gorgoned 2>/dev/null | grep ."
}

all_nodes_connected() {
  gorgone_api GET /v1/nodes \
    | python3 -I -c 'import json,sys; n=json.load(sys.stdin)["nodes"]; sys.exit(0 if len(n) >= 3 and all(x["connected"] for x in n) else 1)'
}

# Written by the central broker container, from what each engine sends it.
instance_running() {
  sql "SELECT running FROM centreon_storage.instances WHERE name = '$1'" | grep -qx 1
}

# A forced check asked through the centreon-web API must reach the engine of
# the Central poller: web -> Gorgone REST API -> agent -> engine API.
web_command_reaches_engine() {
  curl -s -f -X POST -H 'Content-Type: application/json' -H "X-AUTH-TOKEN: $(web_token)" \
    -d '{"check":{"is_forced":true},"resources":[{"type":"host","id":14,"parent":null}]}' \
    "${WEB_URL}/api/latest/monitoring/resources/check"
  sleep 5
  "${COMPOSE[@]}" logs central-engine | grep -q 'EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK'
}

# An external command for a container-only poller goes through the gRPC API
# of its engine, over the shared socket: there is no command pipe.
grpc_command_reaches_engine() {
  gorgone_api POST "/v1/nodes/$(node_of engine)/jobs" -H 'Content-Type: application/json' \
    -d "{\"engine_commands\":{\"commands\":[\"[$(date +%s)] DISABLE_NOTIFICATIONS\"]}}"
  sleep 3
  "${COMPOSE[@]}" logs engine | grep -q 'EXTERNAL COMMAND: DISABLE_NOTIFICATIONS'
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
  gorgone_api GET "/v1/nodes/$(node_of engine)" | python3 -I -c '
import json, sys
node = json.load(sys.stdin)
ok = node["connected"] and node["applied"].get("engine") and node["engine"]["reachable"]
sys.exit(0 if ok else 1)'
}

# A reload asked on top of a failed configuration push must not be sent.
reload_refused_after_failed_push() {
  local node job
  node=$(node_of engine)
  # No agent has this target: the push is rejected.
  tar -cf - -C "$(dirname "$0")" verify.sh \
    | gorgone_api PUT "/v1/nodes/${node}/config/bench-missing" \
      -H 'Content-Type: application/x-tar' --data-binary @-
  job=$(gorgone_api POST "/v1/nodes/${node}/jobs" -H 'Content-Type: application/json' \
    -d '{"service_control":{"service":"engine","operation":"reload"},"requires_latest_config":["bench-missing"]}' \
    | python3 -I -c 'import json,sys; print(json.load(sys.stdin)["id"])')
  gorgone_api GET "/v1/jobs/${job}?wait=20" | python3 -I -c '
import json, sys
job = json.load(sys.stdin)
message = job["events"][-1]["data"].get("message", "")
sys.exit(0 if job["state"] == "failed" and "prerequisite" in message else 1)'
}

# centreon-web has the configuration of a poller verified by that poller's
# engine: the answer shown by CLAPI comes back through Gorgone.
web_validates_on_the_poller() {
  clapi POLLERTEST engine | grep -q 'Total Errors'
  [[ $(sql "SELECT COUNT(*) FROM gorgone.jobs WHERE kind = 'validate_config' AND state = 'succeeded'") -ge 1 ]]
}

# An invalid configuration must be refused by the poller, which keeps the
# one it runs.
invalid_config_is_not_applied() {
  local node work before job
  node=$(node_of engine)
  before=$(gorgone_api GET "/v1/nodes/${node}" \
    | python3 -I -c 'import json,sys; print(json.load(sys.stdin)["applied"]["engine"])')
  work=$(mktemp -d)
  "${COMPOSE[@]}" exec -T engine tar -cf - -C /etc/centreon-engine --exclude='.gorgone-*' . \
    | tar -xf - -C "${work}"
  echo 'define service { broken' >> "${work}/services.cfg"
  job=$(tar -cf - -C "${work}" . \
    | gorgone_api PUT "/v1/nodes/${node}/config/engine" \
      -H 'Content-Type: application/x-tar' --data-binary @- \
    | python3 -I -c 'import json,sys; print(json.load(sys.stdin)["id"])')
  rm -rf "${work}"
  gorgone_api GET "/v1/jobs/${job}?wait=90" \
    | python3 -I -c 'import json,sys; sys.exit(0 if json.load(sys.stdin)["state"] == "failed" else 1)'
  sleep 12
  [[ $(gorgone_api GET "/v1/nodes/${node}" \
    | python3 -I -c 'import json,sys; print(json.load(sys.stdin)["applied"]["engine"])') == "${before}" ]]
  ! "${COMPOSE[@]}" exec -T engine grep -q 'define service { broken' /etc/centreon-engine/services.cfg
}

# The RRD broker container writes the graph files that web reads through a
# shared volume.
web_sees_rrd_files() {
  local attempt
  for attempt in $(seq 1 36); do
    if "${COMPOSE[@]}" exec -T web sh -c 'ls /var/lib/centreon/metrics/*.rrd' > /dev/null 2>&1; then
      return 0
    fi
    sleep 5
  done
  return 1
}

no_centcore_file() {
  local files
  files=$("${COMPOSE[@]}" exec -T web sh -c \
    'find /var/lib/centreon/centcore /var/lib/centreon/centcore.cmd -type f 2>/dev/null')
  [[ -z "${files}" ]]
}

check "the web image carries no collect, Gorgone or plugin package" web_image_is_slim
check "the three agents are connected to gorgone-server" all_nodes_connected
check "the central broker sees the Central engine running" instance_running Central
check "the central broker sees the poller's engine running" instance_running poller
check "the central broker sees the engine container running" instance_running engine
check "configuration was applied on the poller" \
  "${COMPOSE[@]}" exec -T poller test -s /etc/centreon-engine/.gorgone-engine.digest
check "a command from the centreon-web API reaches the Central engine" web_command_reaches_engine
check "a command reaches the engine container over gRPC" grpc_command_reaches_engine
check "the engine API is not reachable over the network" engine_api_not_on_network
check "gorgone-server knows what the engine container runs" node_state_is_reported
check "a reload is refused after a failed configuration push" reload_refused_after_failed_push
check "centreon-web has a configuration verified by the poller engine" web_validates_on_the_poller
check "an invalid configuration is refused and the running one kept" invalid_config_is_not_applied
check "centreon-web reads the RRD files written by the broker container" web_sees_rrd_files
check "centreon-web wrote no centcore command file" no_centcore_file

echo
echo "Jobs handled by gorgone-server:"
sql "SELECT CONCAT('  node ', node_id, '  ', RPAD(kind, 16, ' '), ' ', state) FROM gorgone.jobs ORDER BY created_at" 2> /dev/null

exit "${failures}"
