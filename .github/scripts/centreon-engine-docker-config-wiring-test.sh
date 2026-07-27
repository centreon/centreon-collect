#!/bin/bash
#
# Config-contract and wiring test scenarios for the centreon-engine product
# Docker image. Each scenario starts its own container (via `docker create` +
# `docker cp` to inject config files before the entrypoint ever runs, so no
# docker-compose/volumes are needed) so it can assert its own logs/state
# independently.
#
# Every scenario seeds the minimal-config fixture first: centreon-engine does
# not run usefully out of the box (empty templates only) and is designed to
# crash-loop until a Centreon Central pushes real configuration - confirmed
# against the actual image while writing this test. See
# .github/docker/centreon-engine/fixtures/minimal-config/ and
# centreon-engine-docker-boot-test.sh for the same rationale.
#
# Flakiness note: plugins.json/custom-deps.json installs run via `apt_install_pkgs`
# in the *background* (`&`), started *after* /tmp/docker.ready is touched by
# 99-logs.sh. Never treat readiness as "install finished" - always poll the
# actual package state (dpkg-query) with a timeout.
set -e

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=lib/centreon-docker-test-common.sh
source "$REPO_ROOT/.github/scripts/lib/centreon-docker-test-common.sh"

IMAGE="${IMAGE:?ERROR: IMAGE env var must be set to the image reference to test}"
FIXTURE_DIR="$REPO_ROOT/.github/docker/centreon-engine/fixtures/minimal-config"
LOG_FILE=/tmp/centreon-engine-config-wiring-test.log
: > "$LOG_FILE"
READY_TIMEOUT="${READY_TIMEOUT:-60}"

# buf (used as "buf curl") is a build tool for the *test runner*, not the
# image under test - same tool .github/docker/centreon-gorgone/trixie/Dockerfile
# uses for the same purpose (see rationale there: buf releases monthly with
# a current Go toolchain, unlike grpcurl's long-abandoned release, so its
# binary is downloaded rather than built - no compile cost either way here).
BUF_DIR="$(mktemp -d)"
case "$(uname -m)" in
  x86_64) BUF_ARCH="x86_64" ;;
  aarch64|arm64) BUF_ARCH="aarch64" ;;
  *) echo "::error::Unsupported runner architecture: $(uname -m)" && exit 1 ;;
esac
curl -sSL -o "$BUF_DIR/buf" \
  "https://github.com/bufbuild/buf/releases/download/v1.71.0/buf-Linux-${BUF_ARCH}"
chmod +x "$BUF_DIR/buf"
BUF_BIN="$BUF_DIR/buf"

# engine.proto's relative import ("process_stat.proto") only resolves when
# both files sit in the same directory - mirrors how the Dockerfile copies
# them flat into /usr/share/centreon-engine/proto in the shipped image.
ENGINE_PROTO_DIR="$(mktemp -d)"
cp "$REPO_ROOT/engine/enginerpc/engine.proto" "$REPO_ROOT/common/process_stat/process_stat.proto" "$ENGINE_PROTO_DIR/"

PLUGIN_PKG="centreon-plugin-applications-monitoring-centreon-poller"

declare -a CONTAINERS=()
declare -a TMPFILES=()

cleanup() {
  local rc=$?
  local c
  for c in "${CONTAINERS[@]}"; do
    { echo "=== docker logs $c ==="; docker logs "$c"; } >> "$LOG_FILE" 2>&1 || true
    docker rm -f "$c" > /dev/null 2>&1 || true
  done
  rm -f "${TMPFILES[@]}" 2>/dev/null || true
  _summary_render "Config/wiring test — centreon-engine" "$rc"
}
trap cleanup EXIT

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

wait_for_pkg_installed() {
  local container="$1" pkg="$2" timeout="${3:-90}"
  for _ in $(seq 1 "$timeout"); do
    if docker exec "$container" sh -c "dpkg-query -W -f='\${Status}' '$pkg' 2>/dev/null" | grep -q "^install ok installed$"; then
      return 0
    fi
    sleep 1
  done
  return 1
}

wait_for_log() {
  local container="$1" pattern="$2" timeout="${3:-90}"
  for _ in $(seq 1 "$timeout"); do
    if docker logs "$container" 2>&1 | grep -qi "$pattern"; then
      return 0
    fi
    sleep 1
  done
  return 1
}

# create+cp+start: seeds the minimal-config fixture, then overlays any extra
# config files given as "<local-path> <dest-filename>" pairs, before the
# entrypoint runs - without needing bind mounts. Extra -v args (e.g. for the
# custom-plugins-volume scenario) can be passed via CREATE_EXTRA_ARGS.
create_with_configs() {
  local container="$1"; shift
  # shellcheck disable=SC2086
  docker create --name "$container" ${CREATE_EXTRA_ARGS:-} "$IMAGE" > /dev/null
  CONTAINERS+=("$container")
  docker cp "$FIXTURE_DIR/engine/." "$container:/etc/centreon-engine"
  docker cp "$FIXTURE_DIR/broker/." "$container:/etc/centreon-broker"
  while [ "$#" -ge 2 ]; do
    # mktemp files are 0600 owned by the runner's uid; docker cp preserves
    # mode+uid, and check_custom_deps.py/check_plugins.py run as centreon-engine
    # (uid 901) - without this they'd hit a silent permission-denied read and
    # install nothing. Make it world-readable so 901 can read it regardless.
    chmod 644 "$1"
    docker cp "$1" "$container:/etc/centreon-engine/$2"
    shift 2
  done
  docker start "$container" > /dev/null
}

summary_step_start "custom-deps.json installs a plain valid apt package"
echo "=== [config:custom-deps-install] custom-deps.json installs a plain valid apt package ==="
deps_file=$(mktemp); TMPFILES+=("$deps_file")
echo '{"apt": ["cowsay"]}' > "$deps_file"
create_with_configs centreon-engine-cfg-install-$$ "$deps_file" custom-deps.json
wait_ready centreon-engine-cfg-install-$$ || exit 1
if ! wait_for_pkg_installed centreon-engine-cfg-install-$$ cowsay; then
  echo "::error::cowsay was not installed from custom-deps.json"
  exit 1
fi
echo "OK: cowsay installed from custom-deps.json."
summary_step_pass

summary_step_start "Mixed valid+invalid packages still installs the valid one"
echo "=== [config:custom-deps-isolate-invalid] a mix of valid+invalid packages still installs the valid one ==="
deps_file=$(mktemp); TMPFILES+=("$deps_file")
echo '{"apt": ["this-package-does-not-exist-xyz", "figlet"]}' > "$deps_file"
create_with_configs centreon-engine-cfg-mixed-$$ "$deps_file" custom-deps.json
wait_ready centreon-engine-cfg-mixed-$$ || exit 1
if ! wait_for_pkg_installed centreon-engine-cfg-mixed-$$ figlet; then
  echo "::error::figlet (the valid package in a mixed valid+invalid list) was not installed - the 'isolate unavailable apt packages' fix regressed"
  exit 1
fi
if ! wait_for_log centreon-engine-cfg-mixed-$$ "skipping unavailable package: this-package-does-not-exist-xyz" 15; then
  echo "::error::expected a 'skipping unavailable package' warning for this-package-does-not-exist-xyz"
  exit 1
fi
echo "OK: valid package installed and invalid one isolated/skipped, per-package retry fallback confirmed."
summary_step_pass

summary_step_start "custom-deps.json hot-reload triggers inotify install"
echo "=== [config:custom-deps-hot-reload] custom-deps.json changes at runtime trigger inotify install ==="
create_with_configs centreon-engine-cfg-reload-$$
wait_ready centreon-engine-cfg-reload-$$ || exit 1
docker exec centreon-engine-cfg-reload-$$ sh -c 'echo "{\"apt\": [\"sl\"]}" > /etc/centreon-engine/custom-deps.json'
if ! wait_for_pkg_installed centreon-engine-cfg-reload-$$ sl; then
  echo "::error::sl was not installed after custom-deps.json was updated at runtime - inotify hot-reload is broken"
  exit 1
fi
echo "OK: custom-deps.json hot-reload installed the new package without a restart."
summary_step_pass

summary_step_start "Concurrent apt installs (plugins.json + custom-deps.json) don't conflict"
echo "=== [config:concurrent-apt-callers] plugins.json + custom-deps.json installing at the same boot do not hit a dpkg lock conflict ==="
installed_version=$(docker run --rm --entrypoint dpkg-query "$IMAGE" -W -f='${Version}' "$PLUGIN_PKG" 2>/dev/null)
plugins_file=$(mktemp); TMPFILES+=("$plugins_file")
deps_file=$(mktemp); TMPFILES+=("$deps_file")
printf '{"%s": "0.0.0-force-reinstall"}' "$PLUGIN_PKG" > "$plugins_file"
echo '{"apt": ["cowsay"]}' > "$deps_file"
create_with_configs centreon-engine-cfg-concurrent-$$ "$plugins_file" plugins.json "$deps_file" custom-deps.json
wait_ready centreon-engine-cfg-concurrent-$$ || exit 1
if ! wait_for_pkg_installed centreon-engine-cfg-concurrent-$$ cowsay 120; then
  echo "::error::cowsay (from custom-deps.json) was not installed while plugins.json was installing concurrently"
  exit 1
fi
if ! wait_for_log centreon-engine-cfg-concurrent-$$ "installed: ${PLUGIN_PKG}" 120; then
  echo "::error::$PLUGIN_PKG (from plugins.json, forced reinstall from $installed_version) was not (re)installed while custom-deps.json was installing concurrently"
  exit 1
fi
if docker logs centreon-engine-cfg-concurrent-$$ 2>&1 | grep -Ei "could not get lock|dpkg was interrupted|Unable to lock"; then
  echo "::error::a dpkg/apt lock conflict was logged - the flock serialization between concurrent apt callers is broken"
  exit 1
fi
echo "OK: plugins.json and custom-deps.json installed concurrently with no dpkg lock conflict."
summary_step_pass

summary_step_start "plugins.json no-op when already up to date"
echo "=== [config:plugins-up-to-date-skip] plugins.json requesting the already-installed version is a no-op ==="
plugins_file=$(mktemp); TMPFILES+=("$plugins_file")
printf '{"%s": "%s"}' "$PLUGIN_PKG" "$installed_version" > "$plugins_file"
create_with_configs centreon-engine-cfg-skip-$$ "$plugins_file" plugins.json
wait_ready centreon-engine-cfg-skip-$$ || exit 1
sleep 3
if docker logs centreon-engine-cfg-skip-$$ 2>&1 | grep -q "Installing plugins from plugins.json"; then
  echo "::error::plugins.json triggered an install even though the requested version ($installed_version) was already installed"
  exit 1
fi
echo "OK: plugins.json correctly skipped an already up-to-date package."
summary_step_pass

summary_step_start "Custom plugin volume mount is usable"
echo "=== [config:custom-plugins-volume] a script mounted at /usr/lib/nagios/plugins/custom is usable ==="
plugins_dir=$(mktemp -d); TMPFILES+=("$plugins_dir")
cat > "$plugins_dir/check_dummy.sh" <<'SCRIPT'
#!/bin/sh
echo "OK - dummy check"
exit 0
SCRIPT
# mktemp -d defaults to 0700, unreadable by centreon-engine (uid 901) inside
# the container - the file's own 755 isn't enough if the directory itself
# blocks traversal for non-owners.
chmod 755 "$plugins_dir" "$plugins_dir/check_dummy.sh"
CREATE_EXTRA_ARGS="-v $plugins_dir:/usr/lib/nagios/plugins/custom:ro" \
  create_with_configs centreon-engine-cfg-customvol-$$
wait_ready centreon-engine-cfg-customvol-$$ || exit 1
output=$(docker exec centreon-engine-cfg-customvol-$$ /usr/lib/nagios/plugins/custom/check_dummy.sh)
if [ "$output" != "OK - dummy check" ]; then
  echo "::error::mounted custom plugin did not run as expected, got: $output"
  exit 1
fi
echo "OK: user script mounted at /usr/lib/nagios/plugins/custom ran successfully as centreon-engine."
summary_step_pass

summary_step_start "gRPC GetVersion answers on port 50155"
echo "=== [wiring:grpc-get-version] the engine gRPC management API (port 50155) actually answers ==="
CREATE_EXTRA_ARGS="-p 50155:50155" create_with_configs centreon-engine-wiring-$$
wait_ready centreon-engine-wiring-$$ || exit 1
# rpc_listen_address is forced to 0.0.0.0 by 05-engine-config.sh, but centengine
# still needs a moment after /tmp/docker.ready (set before centengine is exec'd)
# to actually bind the gRPC listener.
grpc_output=""
for _ in $(seq 1 15); do
  grpc_output=$("$BUF_BIN" curl --schema "$ENGINE_PROTO_DIR" --protocol grpc --http2-prior-knowledge \
    http://127.0.0.1:50155/com.centreon.engine.Engine/GetVersion 2>&1) && break
  sleep 1
done
echo "$grpc_output"
if ! echo "$grpc_output" | grep -q '"major"'; then
  echo "::error::GetVersion did not return a version payload over gRPC on port 50155:"
  echo "$grpc_output"
  exit 1
fi
echo "OK: engine gRPC management API answered GetVersion on port 50155."
summary_step_pass

echo "=== [config/wiring] PASSED ==="
