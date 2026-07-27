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

summary_step_start "Stops cleanly"
echo "=== [boot] Stopping container (validates entrypoint cleanup) ==="
if ! docker stop "$CONTAINER_NAME" > /dev/null; then
  echo "::error::centreon-engine container did not stop cleanly within the default timeout"
  exit 1
fi
summary_step_pass

echo "=== [boot] PASSED for $IMAGE ${PLATFORM:+(platform: $PLATFORM)} ==="
