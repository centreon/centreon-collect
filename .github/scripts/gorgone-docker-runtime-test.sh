#!/bin/bash
#
# T1 runtime test for the centreon-gorgone product Docker image.
# Starts the image with default (customer out-of-the-box) settings and checks
# that the container boots cleanly, runs as the expected non-root user, and
# does not crash. No CENTRAL_HOST/CENTRAL_PORT is configured here on purpose:
# pullwss is expected to log connection warnings in that case, this script
# tolerates that and only fails on unambiguous crash signatures.
set -e

IMAGE="${IMAGE:?ERROR: IMAGE env var must be set to the image reference to test}"
PLATFORM="${PLATFORM:-}"
CONTAINER_NAME="gorgone-runtime-test-$$"
READY_TIMEOUT="${READY_TIMEOUT:-30}"

platform_args=()
if [ -n "$PLATFORM" ]; then
  platform_args=(--platform "$PLATFORM")
fi

cleanup() {
  docker logs "$CONTAINER_NAME" > /tmp/gorgone-runtime-test.log 2>&1 || true
  docker rm -f "$CONTAINER_NAME" > /dev/null 2>&1 || true
}
trap cleanup EXIT

echo "=== [T1] Starting $IMAGE ${PLATFORM:+(platform: $PLATFORM)} ==="
docker run -d --name "$CONTAINER_NAME" "${platform_args[@]}" "$IMAGE"

echo "=== [T1] Waiting for /tmp/docker.ready (timeout: ${READY_TIMEOUT}s) ==="
ready=0
for _ in $(seq 1 "$READY_TIMEOUT"); do
  if docker exec "$CONTAINER_NAME" test -f /tmp/docker.ready 2>/dev/null; then
    ready=1
    break
  fi
  sleep 1
done
if [ "$ready" -ne 1 ]; then
  echo "::error::gorgone container did not report readiness (/tmp/docker.ready) within ${READY_TIMEOUT}s"
  docker logs "$CONTAINER_NAME" || true
  exit 1
fi
echo "Container is ready."

echo "=== [T1] Checking container is still running ==="
running=$(docker inspect -f '{{.State.Running}}' "$CONTAINER_NAME")
if [ "$running" != "true" ]; then
  echo "::error::gorgone container is not running after startup"
  docker logs "$CONTAINER_NAME" || true
  exit 1
fi

echo "=== [T1] Checking non-root user (expected uid 903, centreon-gorgone) ==="
uid=$(docker exec "$CONTAINER_NAME" id -u)
if [ "$uid" != "903" ]; then
  echo "::error::gorgone process runs as uid $uid, expected 903 (centreon-gorgone)"
  exit 1
fi

echo "=== [T1] Checking passwordless sudo ==="
if ! docker exec "$CONTAINER_NAME" sudo -n true; then
  echo "::error::sudo -n true failed inside the container: sudoers configuration is broken"
  exit 1
fi

echo "=== [T1] Scanning logs for unambiguous crash signatures ==="
if docker logs "$CONTAINER_NAME" 2>&1 | grep -Ei "Compilation failed|Can't locate|Segmentation fault|Out of memory"; then
  echo "::error::gorgone logs contain a crash signature, see above"
  exit 1
fi

echo "=== [T1] Stopping container (validates entrypoint cleanup trap) ==="
if ! docker stop "$CONTAINER_NAME" > /dev/null; then
  echo "::error::gorgone container did not stop cleanly within the default timeout"
  exit 1
fi

echo "=== [T1] PASSED for $IMAGE ${PLATFORM:+(platform: $PLATFORM)} ==="
