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
ENGINE_RW_CONTAINER="gorgone-runtime-test-enginerw-$$"
ENGINE_RW_VOLUME="gorgone-runtime-test-enginerw-vol-$$"
READY_TIMEOUT="${READY_TIMEOUT:-30}"

platform_args=()
if [ -n "$PLATFORM" ]; then
  platform_args=(--platform "$PLATFORM")
fi

cleanup() {
  docker logs "$CONTAINER_NAME" > /tmp/gorgone-runtime-test.log 2>&1 || true
  docker rm -f "$CONTAINER_NAME" "$ENGINE_RW_CONTAINER" > /dev/null 2>&1 || true
  docker volume rm "$ENGINE_RW_VOLUME" > /dev/null 2>&1 || true
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
# /etc/sudoers.d only whitelists a handful of exact commands (apt, apt-get,
# chown, gorgone_install_plugins.pl) - "sudo -n true" is NOT among them and
# would always fail, so exercise one of the actually-whitelisted commands.
if ! docker exec "$CONTAINER_NAME" sh -c "sudo -n /usr/bin/apt-get --version > /dev/null"; then
  echo "::error::sudo -n /usr/bin/apt-get failed inside the container: sudoers configuration is broken"
  exit 1
fi

echo "=== [T1] Scanning logs for unambiguous crash signatures ==="
if docker logs "$CONTAINER_NAME" 2>&1 | grep -Ei "Compilation failed|Can't locate|Segmentation fault|Out of memory"; then
  echo "::error::gorgone logs contain a crash signature, see above"
  exit 1
fi

# grpcurl is downloaded from a per-arch release tarball (amd64 vs arm64) and the
# copied .proto files must resolve for gRPC-based centengine management to work.
echo "=== [T1] Checking grpcurl runs for this architecture ==="
if ! docker exec "$CONTAINER_NAME" grpcurl -version; then
  echo "::error::grpcurl -version failed inside the container (binary/arch mismatch?)"
  exit 1
fi

echo "=== [T1] Checking engine.proto resolves with grpcurl ==="
# engine.proto has a relative import ("process_stat.proto") copied alongside
# it - grpcurl only resolves that when invoked from within the same
# directory (a "-import-path" pointing at that same directory does not
# work here), so cd into it rather than passing an absolute -proto path.
proto_check=$(docker exec "$CONTAINER_NAME" sh -c \
  "cd /usr/share/centreon-engine/proto && grpcurl -plaintext -connect-timeout 2 -proto engine.proto 127.0.0.1:1 list" 2>&1) || true
if ! echo "$proto_check" | grep -q "com.centreon.engine.Engine"; then
  echo "::error::engine.proto failed to resolve with grpcurl:"
  echo "$proto_check"
  exit 1
fi

# Docker named volumes can come back root:root-owned (e.g. reused from a previous
# root-run container, or certain orchestrator volume plugins) regardless of the
# ownership baked into the image by the Dockerfile. container.d/00-init.sh is
# supposed to fix this up for /var/lib/centreon-engine/rw on every boot.
echo "=== [T1] Checking /var/lib/centreon-engine/rw ownership fix-up on a root-owned volume ==="
docker volume create "$ENGINE_RW_VOLUME" > /dev/null
docker run --rm -v "$ENGINE_RW_VOLUME:/vol" "${platform_args[@]}" alpine:3.20 chown 0:0 /vol > /dev/null
docker run -d --name "$ENGINE_RW_CONTAINER" "${platform_args[@]}" \
  -v "$ENGINE_RW_VOLUME:/var/lib/centreon-engine/rw" "$IMAGE" > /dev/null

engine_rw_ready=0
for _ in $(seq 1 "$READY_TIMEOUT"); do
  if docker exec "$ENGINE_RW_CONTAINER" test -f /tmp/docker.ready 2>/dev/null; then
    engine_rw_ready=1
    break
  fi
  sleep 1
done
if [ "$engine_rw_ready" -ne 1 ]; then
  echo "::error::gorgone container with a root-owned /var/lib/centreon-engine/rw volume did not report readiness within ${READY_TIMEOUT}s"
  docker logs "$ENGINE_RW_CONTAINER" || true
  exit 1
fi
rw_uid=$(docker exec "$ENGINE_RW_CONTAINER" stat -c %u /var/lib/centreon-engine/rw)
if [ "$rw_uid" != "901" ]; then
  echo "::error::/var/lib/centreon-engine/rw is owned by uid $rw_uid after boot, expected 901 (centreon-engine): the 00-init.sh ownership fix-up is broken"
  exit 1
fi

echo "=== [T1] Stopping container (validates entrypoint cleanup trap) ==="
if ! docker stop "$CONTAINER_NAME" > /dev/null; then
  echo "::error::gorgone container did not stop cleanly within the default timeout"
  exit 1
fi

echo "=== [T1] PASSED for $IMAGE ${PLATFORM:+(platform: $PLATFORM)} ==="
