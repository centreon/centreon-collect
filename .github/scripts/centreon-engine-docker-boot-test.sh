#!/bin/bash
#
# Boot test for the centreon-engine product Docker image.
# Starts the image with a minimal-but-valid monitoring configuration and
# checks that the container boots cleanly, runs as the expected non-root
# user, and does not crash. No plugins.json/custom-deps.json is seeded here
# on purpose - that's the config tier's job (see
# centreon-engine-docker-config-wiring-test.sh).
#
# Why a fixture config is required at all: centreon-engine does NOT run
# usefully out of the box. With nothing mounted, /etc/centreon-engine only
# has the empty templates baked in at build time, and the real product is
# designed to crash-loop until a Centreon Central pushes real configuration
# into that path - confirmed against the actual image while writing this
# test (see .github/docker/centreon-engine/fixtures/minimal-config/).
set -e

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=lib/centreon-docker-test-common.sh
source "$REPO_ROOT/.github/scripts/lib/centreon-docker-test-common.sh"

IMAGE="${IMAGE:?ERROR: IMAGE env var must be set to the image reference to test}"
PLATFORM="${PLATFORM:-}"
CONTAINER_NAME="centreon-engine-boot-test-$$"
PERSIST_VOLUME="centreon-engine-boot-test-cma-pki-$$"
PERSIST_HOSTNAME="centreon-engine-boot-test-host"
FIXTURE_DIR="$REPO_ROOT/.github/docker/centreon-engine/fixtures/minimal-config"
# QEMU-emulated arm64 (C++ startup, protobuf config parsing, apt-get update) is
# measured well under 10s natively on amd64, but give it a generous margin
# under emulation rather than risk a flaky timeout on that leg.
READY_TIMEOUT="${READY_TIMEOUT:-60}"

platform_args=()
if [ -n "$PLATFORM" ]; then
  platform_args=(--platform "$PLATFORM")
fi

cleanup() {
  local rc=$?
  docker logs "$CONTAINER_NAME" > /tmp/centreon-engine-boot-test.log 2>&1 || true
  docker rm -f "$CONTAINER_NAME" > /dev/null 2>&1 || true
  docker rm -f "${CONTAINER_NAME}-persist-1" "${CONTAINER_NAME}-persist-2" > /dev/null 2>&1 || true
  docker volume rm "$PERSIST_VOLUME" > /dev/null 2>&1 || true
  _summary_render "Boot test — centreon-engine${PLATFORM:+ ($PLATFORM)}" "$rc"
}
trap cleanup EXIT

# /tmp/docker.ready is touched by 99-logs.sh once all container.d/*.sh
# entrypoint scripts finished, right before centengine itself is exec'd.
wait_ready() {
  local container="$1" timeout="${2:-$READY_TIMEOUT}"
  for _ in $(seq 1 "$timeout"); do
    if docker exec "$container" test -f /tmp/docker.ready 2>/dev/null; then
      return 0
    fi
    sleep 1
  done
  echo "::error::$container did not report readiness (/tmp/docker.ready) within ${timeout}s"
  docker logs "$container" || true
  return 1
}

summary_step_start "Container starts"
echo "=== [boot] Starting $IMAGE ${PLATFORM:+(platform: $PLATFORM)} ==="
docker create --name "$CONTAINER_NAME" "${platform_args[@]}" "$IMAGE" > /dev/null
docker cp "$FIXTURE_DIR/engine/." "$CONTAINER_NAME:/etc/centreon-engine"
docker cp "$FIXTURE_DIR/broker/." "$CONTAINER_NAME:/etc/centreon-broker"
docker start "$CONTAINER_NAME" > /dev/null
summary_step_pass

summary_step_start "Reports readiness (/tmp/docker.ready)"
echo "=== [boot] Waiting for /tmp/docker.ready ==="
wait_ready "$CONTAINER_NAME" || exit 1
echo "Container is ready."
summary_step_pass

summary_step_start "Still running after startup"
echo "=== [boot] Checking container is still running ==="
running=$(docker inspect -f '{{.State.Running}}' "$CONTAINER_NAME")
if [ "$running" != "true" ]; then
  echo "::error::centreon-engine container is not running after startup"
  docker logs "$CONTAINER_NAME" || true
  exit 1
fi
summary_step_pass

summary_step_start "Runs as non-root uid 901 (centreon-engine)"
echo "=== [boot] Checking non-root user (expected uid 901, centreon-engine) ==="
uid=$(docker exec "$CONTAINER_NAME" id -u)
if [ "$uid" != "901" ]; then
  echo "::error::centreon-engine process runs as uid $uid, expected 901 (centreon-engine)"
  exit 1
fi
summary_step_pass

# 99-logs.sh execs centengine as PID 1 (replacing the shell, not forking), so
# checking /proc/1/comm confirms the real monitoring binary took over, not just
# that "some process" is running.
summary_step_start "centengine is PID 1"
echo "=== [boot] Checking centengine is PID 1 ==="
pid1_comm=$(docker exec "$CONTAINER_NAME" cat /proc/1/comm)
if [ "$pid1_comm" != "centengine" ]; then
  echo "::error::PID 1 is '$pid1_comm', expected 'centengine' - entrypoint did not hand off to the monitoring engine"
  exit 1
fi
summary_step_pass

# container.sh (the entrypoint) echoes exactly this string when a sourced
# container.d/*.sh script fails - most notably the regression this test guards
# against, a stray `exit` instead of `return` in a sourced script killing the
# whole entrypoint before centengine ever starts.
summary_step_start "No sourced entrypoint script failed"
echo "=== [boot] Checking no sourced entrypoint script failed ==="
if docker logs "$CONTAINER_NAME" 2>&1 | grep -q "Error executing"; then
  echo "::error::a container.d/*.sh entrypoint script failed, see logs above"
  docker logs "$CONTAINER_NAME" || true
  exit 1
fi
summary_step_pass

summary_step_start "No crash signature in logs"
echo "=== [boot] Scanning logs for unambiguous crash signatures ==="
if docker logs "$CONTAINER_NAME" 2>&1 | grep -Ei "Segmentation fault|core dumped|Aborted|Traceback \(most recent call last\)"; then
  echo "::error::centreon-engine logs contain a crash signature, see above"
  exit 1
fi
summary_step_pass

# Guards against the CMA/OpenTelemetry regression where /etc/pki/centreon-
# engine was non-traversable and the CA was never generated at all (MON-211450):
# libopentelemetry.so failed to load with no clear signal short of this file
# check (the "No crash signature" step above doesn't catch it - centengine
# keeps running fine, just without that one module).
summary_step_start "CMA CA generated with correct permissions and valid"
echo "=== [boot] Checking /etc/pki/centreon-engine/default_cma_ca.{crt,key} ==="
CA_DIR=/etc/pki/centreon-engine
if ! docker exec "$CONTAINER_NAME" test -f "$CA_DIR/default_cma_ca.crt"; then
  echo "::error::$CA_DIR/default_cma_ca.crt was not generated"
  exit 1
fi
if ! docker exec "$CONTAINER_NAME" test -f "$CA_DIR/default_cma_ca.key"; then
  echo "::error::$CA_DIR/default_cma_ca.key was not generated"
  exit 1
fi
crt_perms=$(docker exec "$CONTAINER_NAME" stat -c '%a' "$CA_DIR/default_cma_ca.crt")
if [ "$crt_perms" != "644" ]; then
  echo "::error::default_cma_ca.crt has mode $crt_perms, expected 644"
  exit 1
fi
key_perms=$(docker exec "$CONTAINER_NAME" stat -c '%a' "$CA_DIR/default_cma_ca.key")
if [ "$key_perms" != "600" ]; then
  echo "::error::default_cma_ca.key has mode $key_perms, expected 600"
  exit 1
fi
if ! docker exec "$CONTAINER_NAME" openssl x509 -in "$CA_DIR/default_cma_ca.crt" -noout -checkend 0 > /dev/null 2>&1; then
  echo "::error::default_cma_ca.crt is not a valid, currently-valid X.509 certificate"
  docker exec "$CONTAINER_NAME" openssl x509 -in "$CA_DIR/default_cma_ca.crt" -noout -text || true
  exit 1
fi
summary_step_pass

summary_step_start "Stops cleanly"
echo "=== [boot] Stopping container (validates entrypoint cleanup) ==="
if ! docker stop "$CONTAINER_NAME" > /dev/null; then
  echo "::error::centreon-engine container did not stop cleanly within the default timeout"
  exit 1
fi
summary_step_pass

# CMA agents pin the CA by fingerprint (sha256 of the DER-encoded cert) and
# verify the peer name against the CA's CN (centengine -k uses the container
# hostname as CN). Recreating the container without persisting
# /etc/pki/centreon-engine regenerates both, breaking every enrolled agent
# (MON-211450 follow-up). A named volume mounted fresh over that path is
# seeded from the image's own baked content (0775, uid 901) - a bind mount
# would be root-owned instead, and the pre-fix image baked 0664, so neither
# of those combinations would let 04-cma-ca.sh skip regeneration.
fingerprint() {
  docker exec "$1" openssl x509 -in /etc/pki/centreon-engine/default_cma_ca.crt -outform der 2>/dev/null \
    | openssl dgst -sha256 -binary | base64
}

summary_step_start "CMA CA persists across container recreation"
echo "=== [boot] Starting a container with a named volume on /etc/pki/centreon-engine ==="
docker create --name "${CONTAINER_NAME}-persist-1" "${platform_args[@]}" \
  --hostname "$PERSIST_HOSTNAME" \
  -v "$PERSIST_VOLUME:/etc/pki/centreon-engine" \
  "$IMAGE" > /dev/null
docker cp "$FIXTURE_DIR/engine/." "${CONTAINER_NAME}-persist-1:/etc/centreon-engine"
docker cp "$FIXTURE_DIR/broker/." "${CONTAINER_NAME}-persist-1:/etc/centreon-broker"
docker start "${CONTAINER_NAME}-persist-1" > /dev/null
wait_ready "${CONTAINER_NAME}-persist-1" || exit 1

crt_perms=$(docker exec "${CONTAINER_NAME}-persist-1" stat -c '%a' /etc/pki/centreon-engine/default_cma_ca.crt)
key_perms=$(docker exec "${CONTAINER_NAME}-persist-1" stat -c '%a' /etc/pki/centreon-engine/default_cma_ca.key)
owner=$(docker exec "${CONTAINER_NAME}-persist-1" stat -c '%U' /etc/pki/centreon-engine/default_cma_ca.crt)
cn=$(docker exec "${CONTAINER_NAME}-persist-1" openssl x509 -in /etc/pki/centreon-engine/default_cma_ca.crt -noout -subject)
mtime_1=$(docker exec "${CONTAINER_NAME}-persist-1" stat -c '%Y' /etc/pki/centreon-engine/default_cma_ca.crt)
fingerprint_1=$(fingerprint "${CONTAINER_NAME}-persist-1")

if [ "$crt_perms" != "644" ] || [ "$key_perms" != "600" ] || [ "$owner" != "centreon-engine" ]; then
  echo "::error::first boot: unexpected CA ownership/permissions (crt=$crt_perms key=$key_perms owner=$owner)"
  exit 1
fi
case "$cn" in
  *"CN=$PERSIST_HOSTNAME"*) ;;
  *)
    echo "::error::first boot: CA subject '$cn' does not contain CN=$PERSIST_HOSTNAME"
    exit 1
    ;;
esac

echo "=== [boot] Recreating the container with the same volume and hostname ==="
docker rm -f "${CONTAINER_NAME}-persist-1" > /dev/null
docker create --name "${CONTAINER_NAME}-persist-2" "${platform_args[@]}" \
  --hostname "$PERSIST_HOSTNAME" \
  -v "$PERSIST_VOLUME:/etc/pki/centreon-engine" \
  "$IMAGE" > /dev/null
docker cp "$FIXTURE_DIR/engine/." "${CONTAINER_NAME}-persist-2:/etc/centreon-engine"
docker cp "$FIXTURE_DIR/broker/." "${CONTAINER_NAME}-persist-2:/etc/centreon-broker"
docker start "${CONTAINER_NAME}-persist-2" > /dev/null
wait_ready "${CONTAINER_NAME}-persist-2" || exit 1

mtime_2=$(docker exec "${CONTAINER_NAME}-persist-2" stat -c '%Y' /etc/pki/centreon-engine/default_cma_ca.crt)
fingerprint_2=$(fingerprint "${CONTAINER_NAME}-persist-2")

if [ "$mtime_1" != "$mtime_2" ]; then
  echo "::error::default_cma_ca.crt was regenerated (mtime changed: $mtime_1 -> $mtime_2) - 04-cma-ca.sh should have skipped it"
  exit 1
fi
if [ "$fingerprint_1" != "$fingerprint_2" ]; then
  echo "::error::CA fingerprint changed across recreation ($fingerprint_1 -> $fingerprint_2) - every CMA agent pinned to it would break"
  exit 1
fi
if docker logs "${CONTAINER_NAME}-persist-2" 2>&1 | grep -qi "permission denied"; then
  echo "::error::'Permission denied' in logs after recreation with a persisted volume"
  docker logs "${CONTAINER_NAME}-persist-2" || true
  exit 1
fi
summary_step_pass

echo "=== [boot] PASSED for $IMAGE ${PLATFORM:+(platform: $PLATFORM)} ==="
