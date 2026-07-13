#!/bin/bash
#
# Config-contract and central-endpoint-wiring test scenarios for the
# centreon-gorgone product Docker image. Drives the service definitions in
# docker-compose.config-wiring-test.yml one scenario at a time so each can
# assert its own exit code / logs / files independently.
set -e

export GORGONE_IMAGE="${IMAGE:?ERROR: IMAGE env var must be set to the image reference to test}"

COMPOSE_FILE="$(dirname "$0")/../docker/centreon-gorgone/docker-compose.config-wiring-test.yml"
COMPOSE="docker compose -f $COMPOSE_FILE -p gorgone-config-wiring-test"

cleanup() {
  $COMPOSE down -v --remove-orphans > /dev/null 2>&1 || true
}
trap cleanup EXIT

# /tmp/docker.ready is touched once all container.d/*.sh entrypoint scripts
# finished, right before gorgoned itself is exec'd - so it confirms the
# container's setup completed but not that gorgoned has finished loading its
# modules yet (see wait_for_log for that).
wait_ready() {
  local service="$1" timeout="${2:-30}"
  for _ in $(seq 1 "$timeout"); do
    if $COMPOSE exec -T "$service" test -f /tmp/docker.ready 2>/dev/null; then
      return 0
    fi
    sleep 1
  done
  echo "::error::$service did not report readiness (/tmp/docker.ready) within ${timeout}s"
  $COMPOSE logs "$service" || true
  return 1
}

wait_for_log() {
  local service="$1" pattern="$2" timeout="${3:-15}"
  for _ in $(seq 1 "$timeout"); do
    if $COMPOSE logs "$service" 2>&1 | grep -qi "$pattern"; then
      return 0
    fi
    sleep 1
  done
  return 1
}

echo "=== [config:missing-secrets] TYPE=poller without APP_SECRET/SALT must fail fast ==="
set +e
output=$(timeout 20 $COMPOSE run --rm poller-missing-secrets 2>&1)
status=$?
set -e
echo "$output"
if [ "$status" -eq 0 ]; then
  echo "::error::poller-missing-secrets was expected to fail, but exited 0"
  exit 1
fi
if ! grep -qi "APP_SECRET" <<< "$output"; then
  echo "::error::poller-missing-secrets failure message does not mention APP_SECRET"
  exit 1
fi
echo "OK: container failed fast with a clear APP_SECRET error."
$COMPOSE rm -f poller-missing-secrets > /dev/null 2>&1 || true

echo "=== [config:engine-context] TYPE=poller with APP_SECRET/SALT writes engine-context.json ==="
$COMPOSE run --rm engine-context-init
$COMPOSE up -d poller-with-secrets
wait_ready poller-with-secrets || exit 1
mode=$($COMPOSE exec -T poller-with-secrets stat -c %a /etc/centreon-engine/engine-context.json)
if [ "$mode" != "640" ]; then
  echo "::error::engine-context.json mode is $mode, expected 640"
  exit 1
fi
content=$($COMPOSE exec -T poller-with-secrets cat /etc/centreon-engine/engine-context.json)
if ! grep -q "runtime-test-app-secret" <<< "$content" || ! grep -q "runtime-test-salt" <<< "$content"; then
  echo "::error::engine-context.json content does not match injected APP_SECRET/SALT: $content"
  exit 1
fi
echo "OK: engine-context.json created with mode 640 and expected content."
$COMPOSE down poller-with-secrets > /dev/null 2>&1 || true

echo "=== [config:env-override] Generic GORGONE__... env override is applied ==="
$COMPOSE up -d poller-env-override
wait_ready poller-env-override || exit 1
if ! wait_for_log poller-env-override "gorgone__gorgone__gorgonecore__id environment variable" 15; then
  echo "::error::generic env override GORGONE__GORGONE__GORGONECORE__ID was not applied (no matching debug log line)"
  $COMPOSE logs poller-env-override || true
  exit 1
fi
echo "OK: generic env override mechanism applied."
$COMPOSE down poller-env-override > /dev/null 2>&1 || true

echo "=== [wiring:central-endpoint] CENTRAL_HOST/CENTRAL_PORT/GORGONE_TOKEN wiring reaches pullwss ==="
$COMPOSE up -d central-stub
sleep 2
$COMPOSE up -d poller-central-contract
wait_ready poller-central-contract || exit 1
if ! wait_for_log central-stub "starting data transfer loop" 15; then
  echo "::error::central-stub never received an inbound connection: CENTRAL_HOST/CENTRAL_PORT env wiring is broken"
  $COMPOSE logs central-stub || true
  $COMPOSE logs poller-central-contract || true
  exit 1
fi
echo "OK: poller dialed out to CENTRAL_HOST:CENTRAL_PORT as configured via env vars."

echo "=== [config/wiring] PASSED ==="
