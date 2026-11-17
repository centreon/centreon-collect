#!/usr/bin/env bash
# Checks the integration bench of compose.yaml once it is up. Runs from the
# host: the web image is too small to carry test tools.
set -euo pipefail

# Nothing is built here, but compose interpolates the build context anyway.
export CENTREON_SRC="${CENTREON_SRC:-unused-by-verify}"
COMPOSE=(docker compose -f "$(dirname "$0")/compose.yaml")
GORGONE_URL=http://127.0.0.1:8085
# Second gorgone-server replica, same store; the agents reach either one.
GORGONE_B_URL=http://127.0.0.1:8086
GORGONE_AUTH=centreon-web:bench-only
WEB_URL=http://127.0.0.1:4000/centreon
# Second replica of centreon-web, same database, sessions and key.
WEB_B_URL=http://127.0.0.1:4001/centreon
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

# The broker container runs cbd and its agent only: no Python, no control
# API, no RRD output nor legacy SQL outputs.
broker_image_is_slim() {
  "${COMPOSE[@]}" exec -T broker sh -c '
    ! command -v python3 && [ ! -e /opt/centreon ] \
      && [ ! -e /usr/local/lib/centreon-broker/api_control.py ] \
      && cd /usr/share/centreon/lib/centreon-broker \
      && [ ! -e 70-rrd.so ] && [ ! -e 20-storage.so ] && [ ! -e 80-sql.so ]'
}

# Node 1 has two agents: the engine of central-engine and the broker of the
# broker container, each with its own session.
central_has_engine_and_broker_sessions() {
  gorgone_api GET /v1/nodes/1 | python3 -I -c '
import json, sys
node = json.load(sys.stdin)
components = {c["component"]: c for c in node["components"]}
broker, engine = components["broker"], components["engine"]
ok = (broker["connected"] and engine["connected"]
      and broker["session"]["session_id"] != engine["session"]["session_id"]
      and {"broker", "engine_context"} <= set(broker["targets"]))
sys.exit(0 if ok else 1)'
}

# Pid of the cbd of the broker container, which is its main process.
broker_cbd_pid() {
  "${COMPOSE[@]}" exec -T broker sh -c 'pid=$(cat /var/lib/centreon-broker/cbd.pid) && kill -0 "$pid" && echo "$pid"'
}

# A configuration push reaches cbd: the broker agent applied the same broker
# configuration as the engine agent (the deployment pushed both), and a
# broker reload, routed to the broker agent, has cbd read it again (SIGHUP,
# same process).
broker_push_reaches_cbd() {
  local pid reloads job
  gorgone_api GET /v1/nodes/1 | python3 -I -c '
import json, sys
node = json.load(sys.stdin)
applied = {c["component"]: c["applied"] for c in node["components"]}
digest = applied["broker"].get("broker")
ok = digest and digest == applied["engine"].get("broker") and applied["broker"].get("engine_context")
sys.exit(0 if ok else 1)' || return 1
  pid=$(broker_cbd_pid) || return 1
  reloads=$("${COMPOSE[@]}" logs broker | grep -c 'configuration update requested' || true)
  job=$(gorgone_api POST /v1/nodes/1/jobs -H 'Content-Type: application/json' \
    -d '{"service_control":{"service":"broker","operation":"reload"},"requires_latest_config":["broker"]}' \
    | python3 -I -c 'import json,sys; print(json.load(sys.stdin)["id"])') || return 1
  gorgone_api GET "/v1/jobs/${job}?wait=30" | python3 -I -c '
import json, sys
job = json.load(sys.stdin)
sys.exit(0 if job["state"] == "succeeded" and job["component"] == "broker" else 1)' || return 1
  for _ in $(seq 15); do
    if (($("${COMPOSE[@]}" logs broker | grep -c 'configuration update requested' || true) > reloads)); then
      [[ "$(broker_cbd_pid)" == "${pid}" ]]
      return
    fi
    sleep 1
  done
  return 1
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
  # Perl: the engine image has no Python. Exit 0 when nothing listens.
  "${COMPOSE[@]}" exec -T engine perl -MIO::Socket::INET -e '
exit(IO::Socket::INET->new(PeerAddr => "127.0.0.1", PeerPort => 50155, Timeout => 2) ? 1 : 0)'
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

# check runs these in a condition, where errexit does not apply: each step
# returns on failure.
#
# A session opened on one replica is valid on the other: the login sets the
# PHP session cookie on web, a legacy page answers with it on web-b (without
# it, the login page). The session itself is in Valkey.
session_shared_by_replicas() {
  local jar page
  jar=$(mktemp)
  curl -s -f -c "${jar}" -o /dev/null -X POST -H 'Content-Type: application/json' \
    -d '{"login":"admin","password":"Centreon!2021"}' \
    "${WEB_URL}/authentication/providers/configurations/local"
  page=$(curl -s -f -b "${jar}" "${WEB_B_URL}/main.get.php?p=60101")
  rm -f "${jar}"
  grep -q 'p=60101&host_id=14&o=u' <<< "${page}" || return 1
  "${COMPOSE[@]}" exec -T valkey valkey-cli --scan --pattern 'PHPREDIS_SESSION:*' | grep -q .
}

# Key file of a web container (no shell: copied out as a tar stream).
web_key() {
  docker cp "$("${COMPOSE[@]}" ps -q "$1"):/etc/centreon/engine-context.json" - | tar -xOf -
}

# Both replicas encrypt with the same key, and a configuration pushed through
# either of them gives the pollers that key.
key_shared_by_replicas() {
  local key node_key
  key=$(web_key web)
  [[ -n "${key}" && "$(web_key web-b)" == "${key}" ]] || return 1
  local WEB_URL=${WEB_B_URL}
  clapi POLLERGENERATE engine > /dev/null
  clapi CFGMOVE engine | grep -q 'engine: done' || return 1
  node_key=$("${COMPOSE[@]}" exec -T engine cat /etc/centreon-engine/engine-context.json)
  [[ "${node_key}" == "${key}" ]]
}

# The key reaches no log of the web containers.
key_not_logged() {
  local values logs value
  values=$(web_key web | python3 -I -c 'import json,sys; d=json.load(sys.stdin); print(d["app_secret"]); print(d["salt"])')
  logs=$("${COMPOSE[@]}" logs web web-b web-cron web-cron-b web-install 2>&1)
  [[ -n "${logs}" ]] || return 1
  while read -r value; do
    [[ -n "${value}" ]] || return 1
    ! grep -qF -- "${value}" <<< "${logs}" || return 1
  done <<< "${values}"
}

no_centcore_file() {
  local files
  files=$(web_files_matching '^var/lib/centreon/centcore(\.cmd$|/.+[^/]$)')
  [[ -z "${files}" ]]
}

# Minutes (UTC, from the container log timestamps) of the lines of both cron
# replicas that hold the text $1.
cron_log_minutes() {
  local logs
  logs=$("${COMPOSE[@]}" logs -t --no-log-prefix web-cron web-cron-b) || return 1
  grep -F -- "$1" <<< "${logs}" | cut -c1-16 || true
}

# With two cron replicas, each run of a job happens once: centAcl, due every
# minute, starts exactly once in each of the last three complete minutes, and
# the replica that loses a minute says so.
web_cron_runs_each_job_once() {
  local scheduled first now minute stamp count
  # The later replica to start schedules its first run the minute after.
  scheduled=$(cron_log_minutes 'jobs scheduled (UTC)' | sort | tail -1)
  [[ -n "${scheduled}" ]] || return 1
  first=$(($(date -u -d "${scheduled}" +%s) + 60))
  now=$(date +%s)
  # At most about 4 minutes: three complete minutes from the first one.
  if ((now < first + 185)); then
    sleep $((first + 185 - now))
  fi
  now=$(date +%s)
  for minute in 3 2 1; do
    stamp=$(date -u -d "@$((now / 60 * 60 - minute * 60))" +%Y-%m-%dT%H:%M)
    count=$(cron_log_minutes 'cron: centAcl started' | grep -c -x -F "${stamp}" || true)
    [[ "${count}" == 1 ]] || return 1
  done
  [[ -n "$(cron_log_minutes 'skipped: slot taken by another replica')" ]]
}

# Replica holding the session of node $1 ("" when not connected), as the
# replica at GORGONE_URL reports it.
session_replica() {
  gorgone_api GET "/v1/nodes/$1" | python3 -I -c '
import json, sys
node = json.load(sys.stdin)
print((node.get("session") or {}).get("replica_id", "") if node["connected"] else "")'
}

# REST URL of a replica, from its replica_id.
replica_url() {
  case $1 in
    gorgone-server) echo "${GORGONE_URL}" ;;
    gorgone-server-b) echo "${GORGONE_B_URL}" ;;
    *) return 1 ;;
  esac
}

other_replica() {
  case $1 in
    gorgone-server) echo gorgone-server-b ;;
    gorgone-server-b) echo gorgone-server ;;
    *) return 1 ;;
  esac
}

# job_succeeds_via <REST URL> <node>: a cheap engine command submitted through
# that replica ends succeeded (the host does not need to exist).
job_succeeds_via() {
  local GORGONE_URL=$1 node=$2 job
  job=$(gorgone_api POST "/v1/nodes/${node}/jobs" -H 'Content-Type: application/json' \
    -d "{\"engine_commands\":{\"commands\":[\"[$(date +%s)] SCHEDULE_FORCED_HOST_CHECK;bench-dispatch-check;$(date +%s)\"]}}" \
    | python3 -I -c 'import json,sys; print(json.load(sys.stdin)["id"])') || return 1
  gorgone_api GET "/v1/jobs/${job}?wait=30" \
    | python3 -I -c 'import json,sys; sys.exit(0 if json.load(sys.stdin)["state"] == "succeeded" else 1)'
}

# A job submitted to the replica that does not hold the session of its node
# is handed to the one that does: for each connected node, through the other.
jobs_cross_replicas() {
  local nodes node replica url count=0
  nodes=$(gorgone_api GET /v1/nodes \
    | python3 -I -c 'import json,sys; print("\n".join(n["id"] for n in json.load(sys.stdin)["nodes"]))') || return 1
  while read -r node; do
    replica=$(session_replica "${node}") || return 1
    url=$(replica_url "$(other_replica "${replica}")") || return 1
    job_succeeds_via "${url}" "${node}" || return 1
    count=$((count + 1))
  done <<< "${nodes}"
  ((count >= 3))
}

# wait_replica_ready <replica>: its REST API answers ready within 60 s.
wait_replica_ready() {
  local url
  url=$(replica_url "$1") || return 1
  for _ in $(seq 30); do
    curl -s -f -o /dev/null "${url}/readyz" && return 0
    sleep 2
  done
  return 1
}

# The replica holding the session of the engine container stops: its agent
# reconnects to the other one through the shared name "gorgone", and a new
# job reaches it. The stopped replica is started again whatever the outcome.
failover_to_other_replica() {
  local node holder survivor survivor_url replica="" result=1
  node=$(node_of engine)
  holder=$(session_replica "${node}")
  survivor=$(other_replica "${holder}") || return 1
  survivor_url=$(replica_url "${survivor}") || return 1
  "${COMPOSE[@]}" stop "${holder}" || return 1
  for _ in $(seq 60); do
    replica=$(GORGONE_URL=${survivor_url} session_replica "${node}") || replica=""
    [[ "${replica}" == "${survivor}" ]] && break
    sleep 2
  done
  if [[ "${replica}" == "${survivor}" ]] && job_succeeds_via "${survivor_url}" "${node}"; then
    result=0
  fi
  "${COMPOSE[@]}" start "${holder}" || return 1
  wait_replica_ready "${holder}" || return 1
  return "${result}"
}

# The centreon-engine container runs the centreon-engine program, and no
# gorgone-agent, centengine nor cbd: it is the whole poller.
centreon_engine_is_the_whole_poller() {
  "${COMPOSE[@]}" exec -T centreon-engine sh -c '
    command -v centreon-engine \
      && ! command -v gorgone-agent && ! command -v centengine && ! command -v cbd \
      && [ ! -e /usr/lib64/nagios/cbmod.so ] && [ ! -e /usr/share/centreon/lib/centreon-broker ] \
      && ls -l /proc/*/exe 2>/dev/null | grep -q "/usr/bin/centreon-engine$" \
      && ! ls -l /proc/*/exe 2>/dev/null | grep -qE "gorgone-agent|centengine|cbd"'
}

# Digest of the last structured configuration job of the centreon-engine
# poller, and of what it applied.
last_engine_configuration() {
  sql "SELECT blob_digest FROM gorgone.jobs WHERE node_id = '$(node_of centreon-engine)'
       AND kind = 'apply_engine_configuration' AND state = 'succeeded'
       ORDER BY seq DESC LIMIT 1"
}

applied_engine_digest() {
  gorgone_api GET "/v1/nodes/$(node_of centreon-engine)" \
    | python3 -I -c 'import json,sys; print(json.load(sys.stdin)["applied"].get("engine") or "")'
}

# No .cfg file in the configuration and state directories: the
# configuration came as a structured document; the node reports the digest
# of the last job, which is the one the engine keeps.
centreon_engine_has_no_cfg_file() {
  local last kept files
  files=$("${COMPOSE[@]}" exec -T centreon-engine \
    find /etc/centreon-engine /var/lib/centreon-engine -name '*.cfg') || return 1
  [[ -z "${files}" ]] || return 1
  last=$(last_engine_configuration) || return 1
  [[ -n "${last}" && $(applied_engine_digest) == "${last}" ]] || return 1
  kept=$("${COMPOSE[@]}" exec -T centreon-engine head -n 1 \
    /var/lib/centreon-engine/engine-configuration/configuration) || return 1
  [[ "${kept}" == "${last}" ]]
}

# One service changed through CLAPI, generated, pushed and reloaded: the
# engine reports one change only, the central stores it, and no object of
# the poller is disabled meanwhile (Engine's reload, not the full resync of
# an Engine restart, which disables every object then enables them again).
centreon_engine_incremental_reload() {
  local before job report result=1
  before=$(last_engine_configuration) || return 1
  sql "CREATE TABLE IF NOT EXISTS centreon_storage.bench_disabled
         (kind VARCHAR(16), id BIGINT, parent_id BIGINT, poller_id BIGINT);
       DELETE FROM centreon_storage.bench_disabled;
       DROP TRIGGER IF EXISTS centreon_storage.bench_resource_disabled;
       CREATE TRIGGER centreon_storage.bench_resource_disabled AFTER UPDATE
         ON centreon_storage.resources FOR EACH ROW
         INSERT INTO centreon_storage.bench_disabled
         SELECT 'resource', NEW.id, NEW.parent_id, NEW.poller_id FROM DUAL
         WHERE OLD.enabled = 1 AND NEW.enabled = 0;
       DROP TRIGGER IF EXISTS centreon_storage.bench_service_disabled;
       CREATE TRIGGER centreon_storage.bench_service_disabled AFTER UPDATE
         ON centreon_storage.services FOR EACH ROW
         INSERT INTO centreon_storage.bench_disabled
         SELECT 'service', NEW.service_id, NEW.host_id, 0 FROM DUAL
         WHERE OLD.enabled = 1 AND NEW.enabled = 0" || return 1
  if clapi SETPARAM 'rust-h2;rust-warn;normal_check_interval;2' SERVICE > /dev/null \
    && clapi POLLERGENERATE centreon-engine > /dev/null \
    && clapi CFGMOVE centreon-engine | grep -q 'centreon-engine: done' \
    && clapi POLLERRELOAD centreon-engine > /dev/null; then
    for _ in $(seq 1 30); do
      job=$(sql "SELECT id FROM gorgone.jobs WHERE node_id = '$(node_of centreon-engine)'
                 AND kind = 'apply_engine_configuration' AND state = 'succeeded'
                 ORDER BY seq DESC LIMIT 1")
      if [[ -n "${job}" && $(last_engine_configuration) != "${before}" \
        && $(rust_id s.check_interval rust-h2 rust-warn) == 2 ]]; then
        # The reload that followed the push changed nothing more.
        sleep 5
        report=$(gorgone_api GET "/v1/jobs/${job}")
        if grep -q '0 added, 1 changed, 0 removed' <<< "${report}" \
          && [[ $(sql "SELECT COUNT(*) FROM centreon_storage.bench_disabled d
                  LEFT JOIN centreon_storage.hosts h ON h.host_id = d.parent_id
                  LEFT JOIN centreon_storage.instances i ON i.instance_id = h.instance_id
                  WHERE i.name = 'centreon-engine'
                     OR d.poller_id = $(node_of centreon-engine)") == 0 ]] \
          && [[ $(applied_engine_digest) == $(last_engine_configuration) ]]; then
          result=0
        fi
        break
      fi
      sleep 2
    done
  fi
  sql "DROP TRIGGER IF EXISTS centreon_storage.bench_resource_disabled;
       DROP TRIGGER IF EXISTS centreon_storage.bench_service_disabled;
       DROP TABLE IF EXISTS centreon_storage.bench_disabled"
  return "${result}"
}

# A restart of the container keeps the configuration: the engine loads the
# document it kept, reports the same digest, and the central sees it running
# with its services, without any new push.
centreon_engine_survives_a_restart() {
  local before jobs_before
  before=$(applied_engine_digest)
  [[ -n "${before}" ]] || return 1
  jobs_before=$(sql "SELECT COUNT(*) FROM gorgone.jobs WHERE node_id = '$(node_of centreon-engine)'
                     AND kind = 'apply_engine_configuration'")
  "${COMPOSE[@]}" restart centreon-engine > /dev/null 2>&1 || return 1
  for _ in $(seq 1 60); do
    if gorgone_api GET "/v1/nodes/$(node_of centreon-engine)" | python3 -I -c '
import json, sys
node = json.load(sys.stdin)
sys.exit(0 if node["connected"] and (node["engine"] or {}).get("reachable") else 1)'; then
      break
    fi
    sleep 2
  done
  [[ $(applied_engine_digest) == "${before}" ]] || return 1
  [[ $(sql "SELECT COUNT(*) FROM gorgone.jobs WHERE node_id = '$(node_of centreon-engine)'
            AND kind = 'apply_engine_configuration'") == "${jobs_before}" ]] || return 1
  instance_running centreon-engine || return 1
  [[ $(sql "SELECT COUNT(*) FROM centreon_storage.services s
            JOIN centreon_storage.hosts h ON h.host_id = s.host_id
            JOIN centreon_storage.instances i ON i.instance_id = h.instance_id
            WHERE i.name = 'centreon-engine' AND s.enabled = 1 AND h.enabled = 1") == 9 ]]
}

# centreon_engine_status <host> <service> <state> <type>: the state the central
# stored for that service of the centreon-engine, in services and resources
# (type 1 = HARD). Waits for it: SOFT then HARD takes about two minutes.
centreon_engine_status() {
  local host=$1 service=$2 state=$3 type=$4
  for _ in $(seq 1 48); do
    if [[ $(sql "SELECT CONCAT(s.state, ' ', s.state_type, ' ', r.status, ' ', r.status_confirmed)
          FROM centreon_storage.services s
          JOIN centreon_storage.hosts h ON h.host_id = s.host_id
          JOIN centreon_storage.instances i ON i.instance_id = h.instance_id
          JOIN centreon_storage.resources r ON r.id = s.service_id AND r.parent_id = s.host_id
            AND r.type = 0
          WHERE i.name = 'centreon-engine' AND h.name = '${host}' AND s.description = '${service}'
            AND s.enabled = 1 AND h.enabled = 1 AND r.enabled = 1 AND s.checked = 1") \
        == "${state} ${type} ${state} ${type}" ]]; then
      return 0
    fi
    sleep 5
  done
  return 1
}

# Both hosts of the centreon-engine are UP and HARD, and are resources.
centreon_engine_hosts_up() {
  [[ $(sql "SELECT COUNT(*) FROM centreon_storage.hosts h
        JOIN centreon_storage.instances i ON i.instance_id = h.instance_id
        JOIN centreon_storage.resources r ON r.id = h.host_id AND r.type = 1 AND r.enabled = 1
        WHERE i.name = 'centreon-engine' AND h.enabled = 1 AND h.checked = 1
          AND h.state = 0 AND h.state_type = 1 AND h.name LIKE 'rust-h%'") == 2 ]]
}

rust_id() {
  sql "SELECT $1 FROM centreon_storage.services s JOIN centreon_storage.hosts h
       ON h.host_id = s.host_id WHERE h.name = '$2' AND s.description = '$3' AND s.enabled = 1"
}

# The perfdata of a centreon-engine service reach VictoriaMetrics through the
# central broker, labelled with its ids.
centreon_engine_metric_in_victoria() {
  local host_id
  host_id=$(rust_id s.host_id rust-h1 rust-crit) || return 1
  [[ -n "${host_id}" ]] || return 1
  for _ in $(seq 1 30); do
    if curl -s -G http://127.0.0.1:8428/api/v1/series \
      --data-urlencode "match[]=metric_val{name=\"value\",host_id=\"${host_id}\"}" \
      | python3 -I -c 'import json,sys; sys.exit(0 if json.load(sys.stdin)["data"] else 1)'; then
      return 0
    fi
    sleep 5
  done
  return 1
}

# A forced check asked through the centreon-web API on a centreon-engine service:
# web -> Gorgone REST API -> agent -> built-in scheduler, which runs it now.
centreon_engine_forced_check() {
  local host_id service_id node before after since handled
  host_id=$(rust_id s.host_id rust-h1 rust-ok) || return 1
  service_id=$(rust_id s.service_id rust-h1 rust-ok) || return 1
  node=$(node_of centreon-engine) || return 1
  before=$(rust_id s.last_check rust-h1 rust-ok) || return 1
  # last_check has a one-second resolution.
  sleep 2
  since=$(($(date +%s) * 1000))
  curl -s -f -X POST -H 'Content-Type: application/json' -H "X-AUTH-TOKEN: $(web_token)" \
    -d "{\"check\":{\"is_forced\":true},\"resources\":[{\"type\":\"service\",\"id\":${service_id},\"parent\":{\"id\":${host_id}}}]}" \
    "${WEB_URL}/api/latest/monitoring/resources/check" || return 1
  # The scheduler said it planned the check, and the check ran: well before
  # the next regular one, a minute later.
  for _ in $(seq 1 15); do
    handled=$(sql "SELECT COUNT(*) FROM gorgone.job_events e JOIN gorgone.jobs j ON j.id = e.job_id
                   WHERE j.node_id = '${node}' AND e.time_ms >= ${since}
                     AND e.data LIKE '%check scheduled%'") || return 1
    after=$(rust_id s.last_check rust-h1 rust-ok) || return 1
    if ((handled > 0 && after > before)); then
      return 0
    fi
    sleep 1
  done
  return 1
}

# gorgone-server sees the scheduler as the engine of the centreon-engine.
centreon_engine_state_is_reported() {
  gorgone_api GET "/v1/nodes/$(node_of centreon-engine)" | python3 -I -c '
import json, sys
node = json.load(sys.stdin)
ok = (node["connected"] and node["applied"].get("engine") and node["engine"]["reachable"]
      and node["engine"]["version"].startswith("centreon-engine "))
sys.exit(0 if ok else 1)'
}

# Bench value, not a real secret: the value of the encrypted host macro.
RUST_SECRET=bench-s3cr3t

rust_host_id() {
  sql "SELECT host_id FROM centreon_storage.hosts WHERE name = '$1' AND enabled = 1"
}

# Waits until the last structured configuration job of the centreon-engine
# poller differs from $1 and succeeded.
wait_new_engine_configuration() {
  local before=$1
  for _ in $(seq 1 60); do
    if [[ -n "$(last_engine_configuration)" && "$(last_engine_configuration)" != "${before}" ]]; then
      return 0
    fi
    sleep 2
  done
  return 1
}

# One push for the next checks: a timeperiod that excludes the current
# time (one hour a day, twelve hours from now, UTC) on rust-period, an
# encrypted host macro used by rust-secret, and a recurring downtime on
# rust-h1 starting in two minutes.
centreon_engine_v2_configuration() {
  local hour range before start end zone
  hour=$(( (10#$(date -u +%H) + 12) % 24 ))
  range=$(printf '%02d:00-%02d:00' "${hour}" "$((hour + 1))")
  before=$(last_engine_configuration)
  clapi ADD 'rust-later;Twelve hours later' TP > /dev/null
  for day in sunday monday tuesday wednesday thursday friday saturday; do
    clapi SETPARAM "rust-later;${day};${range}" TP > /dev/null
  done
  clapi SETPARAM 'rust-h2;rust-period;check_period;rust-later' SERVICE > /dev/null
  clapi SETMACRO "rust-h1;SECRET;${RUST_SECRET};1;bench secret" HOST > /dev/null
  RUST_RECURRING="bench-rust-recurring-$(date +%s)"
  # The periods are in the zone of the poller (use_timezone of the document
  # it runs), as for Engine; UTC when it has none.
  zone=$("${COMPOSE[@]}" exec -T centreon-engine cat \
    /var/lib/centreon-engine/engine-configuration/configuration \
    | python3 -I -c 'import json,sys; sys.stdin.readline(); print(json.load(sys.stdin).get("settings", {}).get("use_timezone") or "UTC")')
  start=$(TZ="${zone#:}" date -d '+2 minutes' +%H:%M)
  end=$(TZ="${zone#:}" date -d '+30 minutes' +%H:%M)
  if [[ "${end}" > "${start}" ]]; then
    clapi ADD "${RUST_RECURRING};set by verify.sh" DOWNTIME > /dev/null
    clapi ADDWEEKLYPERIOD "${RUST_RECURRING};${start};${end};1;;1,2,3,4,5,6,7" DOWNTIME > /dev/null
    clapi ADDHOST "${RUST_RECURRING};rust-h1" DOWNTIME > /dev/null
  fi
  clapi POLLERGENERATE centreon-engine > /dev/null || return 1
  clapi CFGMOVE centreon-engine | grep -q 'centreon-engine: done' || return 1
  clapi POLLERRELOAD centreon-engine > /dev/null || return 1
  wait_new_engine_configuration "${before}"
}

# The encrypted macro: the check output proves the plugin got the
# decrypted value (its length and the start of its SHA-256, never the
# value); the document kept by the engine and the one centreon-web sent
# hold it encrypted, and no log of the engine has the value.
centreon_engine_decrypts_secrets() {
  local expected kept sent
  expected="secret-length ${#RUST_SECRET} sha $(printf %s "${RUST_SECRET}" | sha256sum | cut -c1-12)"
  # The central stores the output followed by a new line and the long output.
  for _ in $(seq 1 36); do
    [[ "$(rust_id s.output rust-h1 rust-secret)" == "${expected}"* ]] && break
    sleep 5
  done
  [[ "$(rust_id s.output rust-h1 rust-secret)" == "${expected}"* ]] || return 1
  kept=$("${COMPOSE[@]}" exec -T centreon-engine cat \
    /var/lib/centreon-engine/engine-configuration/configuration) || return 1
  grep -q '"SECRET": "encrypt::' <<< "${kept}" || return 1
  ! grep -qF -- "${RUST_SECRET}" <<< "${kept}" || return 1
  sent=$(docker cp "$("${COMPOSE[@]}" ps -q web):/var/cache/centreon/config/engine/$(node_of centreon-engine)/engine-configuration.json" - | tar -xOf -) || return 1
  grep -q '"SECRET": "encrypt::' <<< "${sent}" || return 1
  ! grep -qF -- "${RUST_SECRET}" <<< "${sent}" || return 1
  ! "${COMPOSE[@]}" logs centreon-engine 2>&1 | grep -qF -- "${RUST_SECRET}"
}

# rust-period is outside its check period: its last check does not move
# for more than two check intervals, and its next check is hours away.
centreon_engine_check_period() {
  local before
  before=$(rust_id s.last_check rust-h2 rust-period)
  sleep 150
  [[ "$(rust_id s.last_check rust-h2 rust-period)" == "${before}" ]] || return 1
  (( $(rust_id s.next_check rust-h2 rust-period) > $(date +%s) + 6 * 3600 ))
}

# The recurring downtime of rust-h1 is scheduled by centreon-engine.
centreon_engine_recurring_downtime() {
  local id host_id
  [[ -n "${RUST_RECURRING:-}" ]] || return 1
  id=$(sql "SELECT dt_id FROM centreon.downtime WHERE dt_name = '${RUST_RECURRING}'")
  host_id=$(rust_host_id rust-h1)
  [[ -n "${id}" && -n "${host_id}" ]] || return 1
  for _ in $(seq 1 36); do
    if [[ "$(sql "SELECT COUNT(*) FROM centreon_storage.downtimes WHERE host_id = ${host_id}
          AND COALESCE(service_id, 0) = 0 AND comment_data = '[Downtime cycle #${id}]'
          AND deletion_time IS NULL")" == 1 ]]; then
      "${COMPOSE[@]}" logs centreon-engine | grep -q "Recurring downtime #${id}: host downtime"
      return
    fi
    sleep 5
  done
  return 1
}

# A downtime set through the centreon-web API on rust-h2: in
# centreon_storage.downtimes, started, and the host is in downtime.
centreon_engine_api_downtime() {
  local host_id start end
  host_id=$(rust_host_id rust-h2) || return 1
  start=$(date -u +%Y-%m-%dT%H:%M:%SZ)
  end=$(date -u -d '+2 hours' +%Y-%m-%dT%H:%M:%SZ)
  curl -s -f -X POST -H 'Content-Type: application/json' -H "X-AUTH-TOKEN: $(web_token)" \
    -d "{\"start_time\":\"${start}\",\"end_time\":\"${end}\",\"is_fixed\":true,\"duration\":7200,\"comment\":\"bench downtime\"}" \
    "${WEB_URL}/api/latest/monitoring/hosts/${host_id}/downtimes" > /dev/null || return 1
  for _ in $(seq 1 30); do
    if [[ "$(sql "SELECT CONCAT(d.started, ' ', d.cancelled, ' ', h.scheduled_downtime_depth, ' ', r.in_downtime)
          FROM centreon_storage.downtimes d
          JOIN centreon_storage.hosts h ON h.host_id = d.host_id
          JOIN centreon_storage.resources r ON r.id = h.host_id AND r.type = 1
          WHERE d.host_id = ${host_id} AND d.comment_data = 'bench downtime'
            AND d.deletion_time IS NULL")" == "1 0 1 1" ]]; then
      return 0
    fi
    sleep 2
  done
  return 1
}

# A sticky acknowledgement through the centreon-web API on rust-crit.
centreon_engine_api_acknowledgement() {
  local host_id service_id
  host_id=$(rust_id s.host_id rust-h1 rust-crit) || return 1
  service_id=$(rust_id s.service_id rust-h1 rust-crit) || return 1
  curl -s -f -X POST -H 'Content-Type: application/json' -H "X-AUTH-TOKEN: $(web_token)" \
    -d "{\"acknowledgement\":{\"comment\":\"bench ack\",\"is_sticky\":true,\"is_persistent_comment\":true,\"is_notify_contacts\":false},\"resources\":[{\"type\":\"service\",\"id\":${service_id},\"parent\":{\"id\":${host_id}}}]}" \
    "${WEB_URL}/api/latest/monitoring/resources/acknowledge" > /dev/null || return 1
  for _ in $(seq 1 30); do
    if [[ "$(sql "SELECT CONCAT(s.acknowledged, ' ', r.acknowledged, ' ', COUNT(a.acknowledgement_id))
          FROM centreon_storage.services s
          JOIN centreon_storage.resources r ON r.id = s.service_id AND r.parent_id = s.host_id AND r.type = 0
          LEFT JOIN centreon_storage.acknowledgements a ON a.host_id = s.host_id
            AND a.service_id = s.service_id AND a.deletion_time IS NULL AND a.comment_data = 'bench ack'
          WHERE s.host_id = ${host_id} AND s.service_id = ${service_id}
          GROUP BY s.acknowledged, r.acknowledged")" == "1 1 1" ]]; then
      return 0
    fi
    sleep 2
  done
  return 1
}

# A comment through the centreon-web API on rust-h1.
centreon_engine_api_comment() {
  local host_id
  host_id=$(rust_host_id rust-h1) || return 1
  curl -s -f -X POST -H 'Content-Type: application/json' -H "X-AUTH-TOKEN: $(web_token)" \
    -d '{"comment":"bench comment","date":null}' \
    "${WEB_URL}/api/latest/monitoring/hosts/${host_id}/comments" > /dev/null || return 1
  for _ in $(seq 1 30); do
    if [[ "$(sql "SELECT COUNT(*) FROM centreon_storage.comments WHERE host_id = ${host_id}
          AND data = 'bench comment' AND deletion_time IS NULL")" == 1 ]]; then
      return 0
    fi
    sleep 2
  done
  return 1
}

# State of the services of the centreon-engine poller as the central
# stores them: description, checked, state, type, attempt, acknowledged.
# rust-period and rust-dependent may never be checked.
rust_states() {
  sql "SELECT CONCAT(s.description, ' ', s.checked, ' ', s.state, ' ', s.state_type, ' ',
              s.check_attempt, ' ', s.acknowledged)
       FROM centreon_storage.services s JOIN centreon_storage.hosts h ON h.host_id = s.host_id
       JOIN centreon_storage.instances i ON i.instance_id = h.instance_id
       WHERE i.name = 'centreon-engine' AND s.enabled = 1
         AND s.description NOT IN (${RUST_MOVING})
       ORDER BY s.description"
}

# rust-sev becomes a severity (a level; CLAPI would ask an icon), pushed
# with the groups, category, parent and dependency of objects.clapi.
centreon_engine_v3_configuration() {
  local before
  before=$(last_engine_configuration)
  sql "UPDATE centreon.hostcategories SET level = 1 WHERE hc_name = 'rust-sev'" || return 1
  clapi POLLERGENERATE centreon-engine > /dev/null || return 1
  clapi CFGMOVE centreon-engine | grep -q 'centreon-engine: done' || return 1
  clapi POLLERRELOAD centreon-engine > /dev/null || return 1
  wait_new_engine_configuration "${before}"
}

# The host and service groups in centreon_storage, as Engine's group and
# member events make them.
centreon_engine_groups() {
  for _ in $(seq 1 30); do
    if [[ "$(sql "SELECT GROUP_CONCAT(h.name ORDER BY h.name)
          FROM centreon_storage.hostgroups g
          JOIN centreon_storage.hosts_hostgroups hg ON hg.hostgroup_id = g.hostgroup_id
          JOIN centreon_storage.hosts h ON h.host_id = hg.host_id
          WHERE g.name = 'rust-hg'")" == "rust-h1,rust-h2" \
      && "$(sql "SELECT GROUP_CONCAT(s.description)
          FROM centreon_storage.servicegroups g
          JOIN centreon_storage.services_servicegroups sg ON sg.servicegroup_id = g.servicegroup_id
          JOIN centreon_storage.services s ON s.host_id = sg.host_id AND s.service_id = sg.service_id
          WHERE g.name = 'rust-sg'")" == "rust-ok" ]]; then
      return 0
    fi
    sleep 2
  done
  return 1
}

# The severity and the tags of the resources: severity rust-sev on
# rust-h1, its category and host group tags, the service group tag of
# rust-ok.
centreon_engine_severity_and_tags() {
  [[ "$(sql "SELECT CONCAT(sv.name, ' ', sv.level, ' ', sv.type)
        FROM centreon_storage.resources r
        JOIN centreon_storage.severities sv ON sv.severity_id = r.severity_id
        WHERE r.type = 1 AND r.name = 'rust-h1' AND r.enabled = 1")" == "rust-sev 1 1" ]] || return 1
  [[ "$(sql "SELECT GROUP_CONCAT(CONCAT(t.name, ':', t.type) ORDER BY t.name)
        FROM centreon_storage.resources r
        JOIN centreon_storage.resources_tags rt ON rt.resource_id = r.resource_id
        JOIN centreon_storage.tags t ON t.tag_id = rt.tag_id
        WHERE r.type = 1 AND r.name = 'rust-h1' AND r.enabled = 1")" == "rust-cat:3,rust-hg:1" ]] \
    || return 1
  [[ "$(sql "SELECT GROUP_CONCAT(CONCAT(t.name, ':', t.type))
        FROM centreon_storage.resources r
        JOIN centreon_storage.resources_tags rt ON rt.resource_id = r.resource_id
        JOIN centreon_storage.tags t ON t.tag_id = rt.tag_id
        WHERE r.type = 0 AND r.name = 'rust-ok' AND r.enabled = 1")" == "rust-sg:0" ]]
}

# resource_names <filter> <values JSON>: names the resources API of
# centreon-web returns for one filter, sorted.
resource_names() {
  curl -s -G -H "X-AUTH-TOKEN: $(web_token)" --data-urlencode "$1=$2" \
    --data-urlencode 'limit=100' "${WEB_URL}/api/latest/monitoring/resources" \
    | python3 -I -c 'import json,sys; print(",".join(sorted({r["name"] for r in json.load(sys.stdin)["result"]})))'
}

# The resources filters of the web find the objects by group, category,
# severity and service group.
centreon_engine_resources_filters() {
  [[ "$(resource_names hostgroup_names '["rust-hg"]')" == *"rust-h1,rust-h2"* ]] || return 1
  [[ "$(resource_names host_category_names '["rust-cat"]')" == *"rust-h1"* ]] || return 1
  [[ "$(resource_names host_severity_names '["rust-sev"]')" == *"rust-h1"* ]] || return 1
  [[ "$(resource_names servicegroup_names '["rust-sg"]')" == "rust-ok" ]]
}

# The event log: initial states and a HARD SERVICE ALERT of rust-crit as
# cbmod parses Engine's lines into logs, and the timeline of the web API
# shows the alert.
centreon_engine_event_log() {
  local host_id service_id
  host_id=$(rust_id s.host_id rust-h1 rust-crit) || return 1
  service_id=$(rust_id s.service_id rust-h1 rust-crit) || return 1
  [[ "$(sql "SELECT COUNT(*) FROM centreon_storage.logs WHERE instance_name = 'centreon-engine'
        AND msg_type = 0 AND host_name = 'rust-h1' AND service_description = 'rust-crit'
        AND host_id = ${host_id} AND service_id = ${service_id} AND status = 2 AND type = 1
        AND output LIKE '%broken%'")" -ge 1 ]] || return 1
  [[ "$(sql "SELECT COUNT(DISTINCT msg_type) FROM centreon_storage.logs
        WHERE instance_name = 'centreon-engine' AND msg_type IN (8, 9)
        AND host_name = 'rust-h1'")" == 2 ]] || return 1
  curl -s -H "X-AUTH-TOKEN: $(web_token)" \
    "${WEB_URL}/api/latest/monitoring/hosts/${host_id}/services/${service_id}/timeline?limit=100" \
    | python3 -I -c '
import json, sys
events = json.load(sys.stdin)["result"]
sys.exit(0 if any(e["type"] == "event" and (e.get("status") or {}).get("name") == "CRITICAL"
                  and "broken" in (e.get("content") or "") for e in events) else 1)'
}

# rust-parent is DOWN, rust-child UNREACHABLE (state 2) and HARD, in hosts
# and resources, and the central knows the parent.
centreon_engine_unreachable_child() {
  for _ in $(seq 1 36); do
    if [[ "$(sql "SELECT CONCAT(h.name, ' ', h.state, ' ', h.state_type, ' ', r.status)
          FROM centreon_storage.hosts h
          JOIN centreon_storage.resources r ON r.id = h.host_id AND r.type = 1
          WHERE h.name IN ('rust-parent', 'rust-child') AND h.enabled = 1
          ORDER BY h.name DESC" | tr '\n' ';')" == "rust-parent 1 1 1;rust-child 2 1 2;" ]]; then
      [[ "$(sql "SELECT COUNT(*) FROM centreon_storage.hosts_hosts_parents p
            JOIN centreon_storage.hosts c ON c.host_id = p.child_id
            JOIN centreon_storage.hosts pa ON pa.host_id = p.parent_id
            WHERE c.name = 'rust-child' AND pa.name = 'rust-parent'")" == 1 ]]
      return
    fi
    sleep 5
  done
  return 1
}

# rust-dependent is not checked while its master rust-crit is CRITICAL
# HARD (execution_failure_criteria c): its last check does not move for
# more than two check intervals.
centreon_engine_dependency_suppresses_checks() {
  local before master="CONCAT(s.state, ' ', s.state_type)"
  [[ "$(rust_id "${master}" rust-h1 rust-crit)" == "2 1" ]] || return 1
  before=$(rust_id s.last_check rust-h1 rust-dependent)
  sleep 150
  [[ "$(rust_id s.last_check rust-h1 rust-dependent)" == "${before}" ]] || return 1
  [[ "$(rust_id "${master}" rust-h1 rust-crit)" == "2 1" ]]
}

# rust-notify, rust-flap and rust-fresh change state on purpose.
RUST_MOVING="'rust-period', 'rust-dependent', 'rust-notify', 'rust-flap', 'rust-fresh'"

MAILPIT_URL=http://127.0.0.1:8025

# mails <subject part>: "subject | recipients | snippet" of each mail
# Mailpit received whose subject holds it, newest first.
mails() {
  curl -s "${MAILPIT_URL}/api/v1/messages?limit=500" | python3 -I -c '
import json, sys
for m in json.load(sys.stdin)["messages"]:
    if sys.argv[1] in m["Subject"]:
        print(m["Subject"], "|", ",".join(t["Address"] for t in m["To"]), "|", m["Snippet"])' "$1"
}

# A version 4 document: the contact, the notification settings, flap
# detection, the notification and runtime lines in the event log (Engine's
# defaults keep them out).
centreon_engine_v4_configuration() {
  local before
  before=$(last_engine_configuration)
  sql "UPDATE centreon.cfg_nagios c JOIN centreon.nagios_server n ON n.id = c.nagios_server_id
       SET c.enable_flap_detection = '1', c.check_service_freshness = '1'
       WHERE n.name = 'centreon-engine'" || return 1
  sql "UPDATE centreon.cfg_nagios_logger l JOIN centreon.cfg_nagios c ON c.nagios_id = l.cfg_nagios_id
       JOIN centreon.nagios_server n ON n.id = c.nagios_server_id
       SET l.log_level_notifications = 'info', l.log_level_runtime = 'warning'
       WHERE n.name = 'centreon-engine'" || return 1
  clapi POLLERGENERATE centreon-engine > /dev/null || return 1
  clapi CFGMOVE centreon-engine | grep -q 'centreon-engine: done' || return 1
  clapi POLLERRELOAD centreon-engine > /dev/null || return 1
  wait_new_engine_configuration "${before}" || return 1
  "${COMPOSE[@]}" exec -T centreon-engine cat \
    /var/lib/centreon-engine/engine-configuration/configuration | python3 -I -c '
import json, sys
sys.stdin.readline()
d = json.load(sys.stdin)
notify = [s for s in d["services"] if s["service_description"] == "rust-notify"][0]
ok = (d["version"] == 4 and d["settings"].get("enable_flap_detection") is True
      and d["settings"].get("check_service_freshness") is True
      and any(c.get("contact_name") == "rust-admin" and c.get("email") == "rust-admin@bench.local"
              for c in d.get("contacts", []))
      and notify.get("contacts") == ["rust-admin"] and notify.get("notification_interval") == 1
      and notify.get("event_handler") == "rust-handler"
      and d["settings"].get("log_level_runtime") == "warning")
sys.exit(0 if ok else 1)'
}

# rust-notify, CRITICAL HARD: one mail to its contact, the macros resolved
# (type, number, contact, date, output).
centreon_engine_notifies_problem() {
  local first
  for _ in $(seq 1 36); do
    first=$(mails 'PROBLEM Service Alert: rust-notif/rust-notify is CRITICAL' | grep -F '| number 1,')
    if [[ -n "${first}" ]]; then
      [[ $(wc -l <<< "${first}") == 1 ]] || return 1
      grep -qE '^\*\* PROBLEM Service Alert: rust-notif/rust-notify is CRITICAL \*\* \| rust-admin@bench.local \| number 1, contact rust-admin, date [A-Z][a-z]{2} [A-Z][a-z]{2,3} [0-9]+ [0-9:]{8} [A-Z+0-9-]+ 20[0-9]{2}, output: CRITICAL: notify me' \
        <<< "${first}"
      return
    fi
    sleep 5
  done
  return 1
}

# notification_interval 1: the next notification a minute later, number 2;
# the central stores the notification line as a log entry of its contact.
centreon_engine_renotifies() {
  for _ in $(seq 1 30); do
    if mails 'PROBLEM Service Alert: rust-notif/rust-notify is CRITICAL' | grep -qF '| number 2,'; then
      break
    fi
    sleep 5
  done
  mails 'PROBLEM Service Alert: rust-notif/rust-notify is CRITICAL' | grep -qF '| number 2,' || return 1
  for _ in $(seq 1 30); do
    [[ "$(sql "SELECT COUNT(*) FROM centreon_storage.logs WHERE instance_name = 'centreon-engine'
          AND msg_type = 2 AND notification_contact = 'rust-admin'
          AND notification_cmd = 'rust-notify-mail' AND service_description = 'rust-notify'
          AND status = 2")" -ge 1 ]] && return 0
    sleep 5
  done
  return 1
}

# submit_service <host> <service> <status> <output>: a passive result
# through the centreon-web API.
submit_service() {
  local host_id service_id
  host_id=$(rust_id s.host_id "$1" "$2") || return 1
  service_id=$(rust_id s.service_id "$1" "$2") || return 1
  curl -s -f -X POST -H 'Content-Type: application/json' -H "X-AUTH-TOKEN: $(web_token)" \
    -d "{\"status\":$3,\"output\":\"$4\",\"performance_data\":\"\"}" \
    "${WEB_URL}/api/latest/monitoring/hosts/${host_id}/services/${service_id}/submit" > /dev/null
}

# An OK result: the recovery is mailed to the contact.
centreon_engine_notifies_recovery() {
  submit_service rust-notif rust-notify 0 'back to normal' || return 1
  for _ in $(seq 1 30); do
    mails 'RECOVERY Service Alert: rust-notif/rust-notify is OK' | grep -qF 'output: back to normal' \
      && return 0
    sleep 2
  done
  return 1
}

# A downtime on rust-notify, set through the centreon-web API: its start is
# mailed (option s), then the problem that follows is not notified.
centreon_engine_downtime_suppresses_notifications() {
  local host_id service_id start end problems
  host_id=$(rust_id s.host_id rust-notif rust-notify) || return 1
  service_id=$(rust_id s.service_id rust-notif rust-notify) || return 1
  start=$(date -u +%Y-%m-%dT%H:%M:%SZ)
  end=$(date -u -d '+2 hours' +%Y-%m-%dT%H:%M:%SZ)
  curl -s -f -X POST -H 'Content-Type: application/json' -H "X-AUTH-TOKEN: $(web_token)" \
    -d "{\"start_time\":\"${start}\",\"end_time\":\"${end}\",\"is_fixed\":true,\"duration\":7200,\"comment\":\"bench quiet\"}" \
    "${WEB_URL}/api/latest/monitoring/hosts/${host_id}/services/${service_id}/downtimes" > /dev/null \
    || return 1
  for _ in $(seq 1 30); do
    mails 'DOWNTIMESTART Service Alert: rust-notif/rust-notify' | grep -q . && break
    sleep 2
  done
  mails 'DOWNTIMESTART Service Alert: rust-notif/rust-notify' | grep -q . || return 1
  problems=$(mails 'PROBLEM Service Alert: rust-notif/rust-notify' | wc -l)
  # Its active check is CRITICAL again: SOFT, then HARD a minute later.
  sleep 150
  [[ "$(rust_id "CONCAT(s.state, ' ', s.state_type, ' ', s.scheduled_downtime_depth)" rust-notif rust-notify)" \
    == "2 1 1" ]] || return 1
  [[ "$(mails 'PROBLEM Service Alert: rust-notif/rust-notify' | wc -l)" == "${problems}" ]]
}

# The event handler of rust-notify ran on its state changes: what it wrote
# in the container, and the lines of the event log.
centreon_engine_event_handler() {
  local handled
  handled=$("${COMPOSE[@]}" exec -T centreon-engine cat /var/lib/centreon-engine/handled) || return 1
  grep -qx 'rust-notif rust-notify CRITICAL SOFT 1' <<< "${handled}" || return 1
  grep -qx 'rust-notif rust-notify CRITICAL HARD 2' <<< "${handled}" || return 1
  grep -qx 'rust-notif rust-notify OK HARD 1' <<< "${handled}" || return 1
  [[ "$(sql "SELECT COUNT(*) FROM centreon_storage.logs WHERE instance_name = 'centreon-engine'
        AND msg_type = 12 AND service_description = 'rust-notify' AND output = 'rust-handler'")" -ge 3 ]]
}

# rust-flap gets alternating passive results: it is flapping in services,
# with the alert in the event log and the comment of the engine.
centreon_engine_flapping() {
  local i
  for i in $(seq 0 12); do
    submit_service rust-notif rust-flap $(( i % 2 == 0 ? 2 : 0 )) "flap ${i}" || return 1
    sleep 1
  done
  for _ in $(seq 1 30); do
    if [[ "$(rust_id "CONCAT(s.flapping, ' ', s.percent_state_change > 30)" rust-notif rust-flap)" == "1 1" ]]; then
      [[ "$(sql "SELECT COUNT(*) FROM centreon_storage.logs WHERE instance_name = 'centreon-engine'
            AND output LIKE 'SERVICE FLAPPING ALERT: rust-notif;rust-flap;STARTED; Service appears to have started flapping (%'")" -ge 1 ]] \
        || return 1
      [[ "$(sql "SELECT COUNT(*) FROM centreon_storage.comments c
            JOIN centreon_storage.services s ON s.host_id = c.host_id AND s.service_id = c.service_id
            WHERE s.description = 'rust-flap' AND c.entry_type = 3 AND c.deletion_time IS NULL
              AND c.author = '(Centreon Engine Process)'")" == 1 ]]
      return
    fi
    sleep 2
  done
  return 1
}

# rust-fresh, passive only: once its passive result is older than its
# freshness threshold (30 s), the engine runs its check (active result),
# with the stale warning in the event log (cbmod keeps a warning without its
# "Warning: " prefix).
centreon_engine_freshness() {
  submit_service rust-notif rust-fresh 0 'passive result' || return 1
  for _ in $(seq 1 30); do
    [[ "$(rust_id s.output rust-notif rust-fresh)" == 'passive result'* ]] && break
    sleep 2
  done
  for _ in $(seq 1 60); do
    if [[ "$(rust_id "CONCAT(s.check_type, ' ', s.output)" rust-notif rust-fresh)" == '0 OK: refreshed by the engine'* ]]; then
      [[ "$(sql "SELECT COUNT(*) FROM centreon_storage.logs WHERE instance_name = 'centreon-engine'
            AND msg_type = 4
            AND output LIKE 'The results of service ''rust-fresh'' on host ''rust-notif'' are stale by %'")" -ge 1 ]]
      return
    fi
    sleep 3
  done
  return 1
}

# A restart of the container keeps the states (none pending, same state,
# type, attempt, acknowledgement), the downtime and the acknowledgement:
# the engine reads its retention file before its first dump.
centreon_engine_restart_keeps_states() {
  local before host_id
  before=$(rust_states) || return 1
  [[ -n "${before}" ]] && ! grep -q ' 0 [0-9]* [0-9]* [0-9]* [0-9]*$' <<< "${before}" || return 1
  "${COMPOSE[@]}" restart centreon-engine > /dev/null 2>&1 || return 1
  for _ in $(seq 1 60); do
    instance_running centreon-engine && break
    sleep 2
  done
  "${COMPOSE[@]}" exec -T centreon-engine test -s /var/lib/centreon-engine/retention.json || return 1
  sleep 10
  [[ "$(rust_states)" == "${before}" ]] || return 1
  host_id=$(rust_host_id rust-h2) || return 1
  [[ "$(sql "SELECT CONCAT(d.started, ' ', d.cancelled, ' ', h.scheduled_downtime_depth)
        FROM centreon_storage.downtimes d JOIN centreon_storage.hosts h ON h.host_id = d.host_id
        WHERE d.host_id = ${host_id} AND d.comment_data = 'bench downtime'
          AND d.deletion_time IS NULL")" == "1 0 1" ]]
}

check "the web image is a web server only: no shell, collect, Gorgone or plugin" web_image_is_slim
check "the web image carries no rrdtool" web_image_has_no_rrdtool
check "no volume is shared between containers" no_shared_volume
check "the three agents are connected to gorgone-server" all_nodes_connected
check "the broker image carries no Python, control API, RRD nor legacy SQL output" broker_image_is_slim
check "the Central poller has an engine and a broker session" central_has_engine_and_broker_sessions
check "a broker configuration push and reload reach cbd through its agent" broker_push_reaches_cbd
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
check "a session opened on one web replica is valid on the other" session_shared_by_replicas
check "both web replicas push the same encryption key" key_shared_by_replicas
check "the encryption key is in no log of the web containers" key_not_logged
check "two web cron replicas run each job once per minute" web_cron_runs_each_job_once
check "the centreon-engine container runs centreon-engine, no gorgone-agent, centengine nor cbd" \
  centreon_engine_is_the_whole_poller
check "the central broker sees the centreon-engine running" instance_running centreon-engine
check "gorgone-server sees centreon-engine as the engine of its poller" \
  centreon_engine_state_is_reported
check "centreon-engine: a CRITICAL service goes HARD after its attempts" \
  centreon_engine_status rust-h1 rust-crit 2 1
check "centreon-engine: a WARNING service goes HARD after its attempts" \
  centreon_engine_status rust-h2 rust-warn 1 1
check "centreon-engine: an OK service is stored OK and HARD" centreon_engine_status rust-h1 rust-ok 0 1
check "centreon-engine: both hosts are UP, HARD and resources" centreon_engine_hosts_up
check "centreon-engine: a metric reaches VictoriaMetrics" centreon_engine_metric_in_victoria
check "centreon-engine: a forced check from the centreon-web API is run" centreon_engine_forced_check
check "centreon-engine: no .cfg file, the applied digest is the last configuration job's" \
  centreon_engine_has_no_cfg_file
check "centreon-engine: a changed service is reloaded alone, nothing is disabled meanwhile" \
  centreon_engine_incremental_reload
check "centreon-engine: a timeperiod, an encrypted macro and a recurring downtime are pushed" \
  centreon_engine_v2_configuration
check "centreon-engine: an encrypt:: macro is decrypted by the engine and nowhere in clear" \
  centreon_engine_decrypts_secrets
check "centreon-engine: the recurring downtime of a host is scheduled by the engine" \
  centreon_engine_recurring_downtime
check "centreon-engine: a downtime from the centreon-web API is started, the host in downtime" \
  centreon_engine_api_downtime
check "centreon-engine: an acknowledgement from the centreon-web API is stored" \
  centreon_engine_api_acknowledgement
check "centreon-engine: a comment from the centreon-web API is stored" centreon_engine_api_comment
check "centreon-engine: groups, a category, a severity, parents and a dependency are pushed" \
  centreon_engine_v3_configuration
check "centreon-engine: the host and service groups are in centreon_storage" centreon_engine_groups
check "centreon-engine: the resources carry their severity and tags" \
  centreon_engine_severity_and_tags
check "centreon-engine: the resources API filters by group, category and severity" \
  centreon_engine_resources_filters
check "centreon-engine: alerts and initial states reach logs and the timeline API" \
  centreon_engine_event_log
check "centreon-engine: the child of a DOWN parent is UNREACHABLE" centreon_engine_unreachable_child
check "centreon-engine: a service is not checked while its master fails" \
  centreon_engine_dependency_suppresses_checks
check "centreon-engine: a service outside its check period is not checked" \
  centreon_engine_check_period
check "centreon-engine: contacts, notification, flapping and freshness settings are pushed (version 4)" \
  centreon_engine_v4_configuration
check "centreon-engine: a CRITICAL HARD service mails its contact once, macros resolved" \
  centreon_engine_notifies_problem
check "centreon-engine: the notification is sent again after notification_interval" \
  centreon_engine_renotifies
check "centreon-engine: a recovery is mailed to the contact" centreon_engine_notifies_recovery
check "centreon-engine: a downtime from the web API is mailed, then suppresses notifications" \
  centreon_engine_downtime_suppresses_notifications
check "centreon-engine: the event handler runs on state changes" centreon_engine_event_handler
check "centreon-engine: a flapping service is flapping, with its alert and comment" \
  centreon_engine_flapping
check "centreon-engine: a stale passive service gets an active check" centreon_engine_freshness
check "centreon-engine: a restart keeps states, attempts, downtime and acknowledgement" \
  centreon_engine_restart_keeps_states
check "centreon-engine: the configuration survives a container restart" \
  centreon_engine_survives_a_restart
check "a job submitted to either gorgone-server replica reaches every node" jobs_cross_replicas
# Last: the agents move between the gorgone-server replicas.
check "an agent fails over to the other gorgone-server replica" failover_to_other_replica

echo
echo "Jobs handled by gorgone-server:"
sql "SELECT CONCAT('  node ', node_id, '  ', RPAD(kind, 16, ' '), ' ', state) FROM gorgone.jobs ORDER BY created_at" 2> /dev/null

exit "${failures}"
