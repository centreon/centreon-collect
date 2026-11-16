#!/usr/bin/env bash
# Checks the integration bench of compose.yaml once it is up. Runs from the
# host: the web image is too small to carry test tools.
set -euo pipefail

# Nothing is built here, but compose interpolates the build context anyway.
export CENTREON_SRC="${CENTREON_SRC:-unused-by-verify}"
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
  "${COMPOSE[@]}" exec -T db mariadb --skip-ssl -uroot -pcentreon -N -e "$1"
}

web_token() {
  curl -s -X POST -H 'Content-Type: application/json' \
    -d '{"security":{"credentials":{"login":"admin","password":"Centreon!2021"}}}' \
    "${WEB_URL}/api/latest/login" \
    | python3 -I -c 'import json,sys; print(json.load(sys.stdin)["security"]["token"])'
}

# clapi <action> <values> [object]
clapi() {
  local object=""
  [[ -n "${3:-}" ]] && object="\"object\":\"$3\","
  curl -s -m 120 -X POST -H 'Content-Type: application/json' -H "centreon-auth-token: $(web_token)" \
    -d "{\"action\":\"$1\",${object}\"values\":\"$2\"}" \
    "${WEB_URL}/api/index.php?action=action&object=centreon_clapi"
}

node_of() {
  sql "SELECT id FROM centreon.nagios_server WHERE name = '$1'"
}

# The web container has no shell: its files are listed from outside. The
# list is read whole before grep, so an early grep exit cannot hide a match.
web_files_matching() {
  local files
  files=$(docker export "$("${COMPOSE[@]}" ps -q web)" | tar -t)
  grep -E "$1" <<< "${files}" || true
}

# The web image is a web server only: no shell, no package manager, and no
# collect, Gorgone, plugin or Perl file.
web_image_is_slim() {
  [[ -z "$(web_files_matching \
    '^(bin|usr/bin|usr/sbin)/(sh|bash|dnf|rpm|apt|perl|centengine|cbd|gorgoned)$|^usr/lib/centreon/plugins/|^usr/share/perl')" ]]
}

web_image_has_no_rrdtool() {
  [[ -z "$(web_files_matching '(^|/)rrdtool$|librrd')" ]]
}

# Each volume belongs to one container: configuration, keys and commands go
# over the network.
no_shared_volume() {
  local container
  for container in $("${COMPOSE[@]}" ps -aq); do
    docker inspect -f '{{range .Mounts}}{{if eq .Type "volume"}}{{.Name}}{{"\n"}}{{end}}{{end}}' "${container}"
  done | grep . | sort | uniq -d | grep -q . && return 1
  return 0
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

# Performance data sent to the Central engine goes engine -> broker ->
# VictoriaMetrics, with the labels the web reads by.
metrics_reach_victoria() {
  local now
  for _ in 1 2 3; do
    now=$(date +%s)
    gorgone_api POST "/v1/nodes/$(node_of Central)/jobs" -H 'Content-Type: application/json' \
      -d "{\"engine_commands\":{\"commands\":[\"[${now}] PROCESS_SERVICE_CHECK_RESULT;Centreon-Server;Ping;0;OK - rta 12ms|rta=1${now: -1}ms;200;400;0 pl=0%;20;50;0;100\"]}}"
    sleep 20
  done
  for _ in $(seq 1 30); do
    if curl -s -G http://127.0.0.1:8428/api/v1/series \
      --data-urlencode 'match[]=metric_val{name="rta",host_id="14"}' \
      | python3 -I -c 'import json,sys; sys.exit(0 if json.load(sys.stdin)["data"] else 1)'; then
      return 0
    fi
    sleep 5
  done
  return 1
}

# The API v2 graph of that service is drawn from VictoriaMetrics.
web_graph_from_victoria() {
  local start end
  start=$(date -u -d '-1 hour' +%Y-%m-%dT%H:%M:%SZ)
  end=$(date -u -d '+5 minutes' +%Y-%m-%dT%H:%M:%SZ)
  curl -s -f -H "X-AUTH-TOKEN: $(web_token)" \
    "${WEB_URL}/api/latest/monitoring/hosts/14/services/26/metrics/start/${start}/end/${end}" \
    | python3 -I -c '
import json, sys
graph = json.load(sys.stdin)
values = [v for m in graph["metrics"] if m["metric"] == "rta" for v in m["data"] if v is not None]
sys.exit(0 if values else 1)'
}

# Nothing writes or reads RRD files any more.
no_rrd_file() {
  local found
  found=$("${COMPOSE[@]}" exec -T broker sh -c "find / -xdev -name '*.rrd' 2>/dev/null | head -1")
  found+=$(web_files_matching '\.rrd$')
  [[ -z "${found}" ]]
}

# The engine target prunes stale files; the key of the engine_context target,
# in the same directory, must survive a push that prunes.
key_survives_pruning_push() {
  "${COMPOSE[@]}" exec -T engine sh -c 'echo old > /etc/centreon-engine/stale.cfg'
  clapi POLLERGENERATE engine > /dev/null
  clapi CFGMOVE engine | grep -q 'engine: done'
  ! "${COMPOSE[@]}" exec -T engine test -e /etc/centreon-engine/stale.cfg
  "${COMPOSE[@]}" exec -T engine test -s /etc/centreon-engine/engine-context.json
}

# A recurring downtime is scheduled by the engine of its host. The period
# starts in two minutes (UTC, the zone of the containers).
recurring_downtime_scheduled_by_engine() {
  local name start end id
  name="bench-recurring-$(date +%s)"
  start=$(date -u -d '+2 minutes' +%H:%M)
  end=$(date -u -d '+30 minutes' +%H:%M)
  [[ "${end}" > "${start}" ]] || return 0 # too close to midnight for a same-day period
  clapi ADD "${name};set by verify.sh" DOWNTIME > /dev/null
  clapi ADDWEEKLYPERIOD "${name};${start};${end};1;;1,2,3,4,5,6,7" DOWNTIME > /dev/null
  clapi ADDHOST "${name};Centreon-Server" DOWNTIME > /dev/null
  id=$(sql "SELECT dt_id FROM centreon.downtime WHERE dt_name = '${name}'")
  clapi POLLERGENERATE Central > /dev/null
  clapi CFGMOVE Central > /dev/null
  clapi POLLERRELOAD Central > /dev/null
  for _ in $(seq 60); do
    if [[ "$(sql "SELECT COUNT(*) FROM centreon_storage.downtimes WHERE host_id = 14 AND COALESCE(service_id, 0) = 0 AND comment_data = '[Downtime cycle #${id}]' AND deletion_time IS NULL")" == 1 ]]; then
      "${COMPOSE[@]}" logs central-engine | grep -q "Recurring downtime #${id}: host downtime"
      return
    fi
    sleep 5
  done
  return 1
}

# An operator cancels the host downtime: the next run (every 60 s) leaves it out.
cancelled_recurring_downtime_stays_cancelled() {
  local where="host_id = 14 AND COALESCE(service_id, 0) = 0 AND comment_data LIKE '[Downtime cycle #%' AND deletion_time IS NULL"
  local internal_id
  internal_id=$(sql "SELECT internal_id FROM centreon_storage.downtimes WHERE ${where} LIMIT 1")
  [[ -n "${internal_id}" ]] || return 1
  gorgone_api POST "/v1/nodes/1/jobs" -H 'Content-Type: application/json' \
    -d "{\"engine_commands\":{\"commands\":[\"[$(date +%s)] DEL_HOST_DOWNTIME;${internal_id}\"]}}"
  sleep 75 # one run of the recurring downtime event, every 60 s
  [[ "$(sql "SELECT COUNT(*) FROM centreon_storage.downtimes WHERE ${where}")" == 0 ]]
}

web_cron_leaves_recurring_downtimes_to_engine() {
  "${COMPOSE[@]}" exec -T web-cron /usr/local/bin/frankenphp php-cli /usr/share/centreon/cron/downtimeManager.php |
    grep -q 'scheduled by Centreon Engine'
}

no_centcore_file() {
  local files
  files=$(web_files_matching '^var/lib/centreon/centcore(\.cmd$|/.+[^/]$)')
  [[ -z "${files}" ]]
}

check "the web image is a web server only: no shell, collect, Gorgone or plugin" web_image_is_slim
check "the web image carries no rrdtool" web_image_has_no_rrdtool
check "no volume is shared between containers" no_shared_volume
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
check "performance data reaches VictoriaMetrics through the broker" metrics_reach_victoria
check "centreon-web draws a graph from VictoriaMetrics" web_graph_from_victoria
check "no RRD file is written nor read" no_rrd_file
check "the key survives the pruning engine pushes" key_survives_pruning_push
check "a recurring downtime is scheduled by the engine" recurring_downtime_scheduled_by_engine
check "a cancelled recurring downtime is not scheduled again" cancelled_recurring_downtime_stays_cancelled
check "the web cron leaves recurring downtimes to the engine" web_cron_leaves_recurring_downtimes_to_engine
check "centreon-web wrote no centcore command file" no_centcore_file

echo
echo "Jobs handled by gorgone-server:"
sql "SELECT CONCAT('  node ', node_id, '  ', RPAD(kind, 16, ' '), ' ', state) FROM gorgone.jobs ORDER BY created_at" 2> /dev/null

exit "${failures}"
