# centreon-engine image for the gorgone-rs proof of concept

`centreon-engine-trixie:develop` with `/usr/sbin/centengine` replaced by a build of this branch, which adds the `ProcessExternalCommand` and `VerifyConfiguration` RPCs to the Engine gRPC API (`engine/enginerpc/engine.proto`) and lets that API listen on a Unix domain socket, and with the entrypoint script `05-engine-config.sh` of this branch, which reads `ENGINE_RPC_LISTEN_ADDRESS`. Nothing else changes: enginerpc is linked statically into `centengine`, and the binary keeps loading the shared objects of the image packages (`cbmod.so`, `libcentreon_clib.so`, `libcentreon_engine_conf.so`), so the branch must not diverge from `develop` in `common/`, `clib/`, `bbdo/` or `broker/neb/`.

## Build

All commands run from the repository root. The build tree lives in a docker volume, never in the working tree.

The CMake configuration is the exception: it writes four git-ignored files into the source tree (`agent/src/agent.rc`, `agent/inc/com/centreon/agent/version.hh`, `agent/conf/centagent.json`, `agent/scripts/centagent.service`), so it fails on the read-only mount of step 1 ("Read-only file system"). Mount the sources read-write (`-v "$PWD":/src`) for the first run on a new volume and after a change to a `CMakeLists.txt`; the other runs only compile and keep `:ro`. A configuration that failed leaves `CMakeCache.txt` newer than `build.ninja` in the volume, and every later build then tries to configure again until one succeeds.

```bash
PACKAGING=docker.centreon.com/centreon/packaging-centreon-collect-vcpkg-trixie:f78f100f

# 1. Configure (once per volume) and build centengine, as the "unit test" job of
#    .github/workflows/collect.yml does, without sccache.
docker run --rm -v "$PWD":/src:ro -v collect-poc-build-trixie:/build -w /src "$PACKAGING" bash -ec '
  export VCPKG_ROOT=/vcpkg VCPKG_FORCE_SYSTEM_BINARIES=1 PATH=/vcpkg:/opt/cmake-4.3.3/bin:$PATH
  [ -f /build/CMakeCache.txt ] || CXXFLAGS="-Wall -Wextra" cmake -B /build -S /src -GNinja \
    -DVCPKG_OVERLAY_TRIPLETS=/custom-triplets -DVCPKG_TARGET_TRIPLET=x64-linux -DVCPKG_OVERLAY_PORTS=/overlays \
    -DDEBUG_ROBOT=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=On -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    "-DCMAKE_CXX_FLAGS_RELWITHDEBINFO=-O2 -g1 -DNDEBUG" "-DCMAKE_C_FLAGS_RELWITHDEBINFO=-O2 -g1 -DNDEBUG" \
    -DWITH_PREFIX=/usr -DWITH_PREFIX_BIN=/usr/sbin \
    -DWITH_USER_BROKER=centreon-broker -DWITH_USER_ENGINE=centreon-engine \
    -DWITH_GROUP_BROKER=centreon-broker -DWITH_GROUP_ENGINE=centreon-engine \
    -DWITH_TESTING=On -DWITH_PREFIX_MODULES=/usr/share/centreon/lib/centreon-broker \
    -DWITH_PREFIX_CONF_BROKER=/etc/centreon-broker -DWITH_PREFIX_LIB_BROKER=/usr/lib64/nagios \
    -DWITH_PREFIX_CONF_ENGINE=/etc/centreon-engine -DWITH_PREFIX_LIB_ENGINE=/usr/lib64/centreon-engine \
    -DWITH_PREFIX_LIB_CLIB=/usr/lib64/ -DWITH_RW_DIR=/var/lib/centreon-engine/rw \
    -DWITH_VAR_DIR=/var/log/centreon-engine -DWITH_MODULE_SIMU=On -DLEGACY_ENGINE=OFF -DONLY_ROBOT_CMA=OFF
  ninja -C /build -j "$(nproc)" centengine ut_engine rpc_client_engine bin_connector_test_run externalcmd'

# 2. Unit tests of the gRPC API. The VerifyConfiguration tests run the
#    engine/centengine built by step 1.
docker run --rm -v "$PWD":/src:ro -v collect-poc-build-trixie:/build -w /build "$PACKAGING" \
  tests/ut_engine --gtest_filter='EngineRpc.*:enginerpc_test.*:ApplierGlobal.*'

# 3. Build context, outside the working tree: the stripped binary and the
#    entrypoint script of this branch.
CTX=$(mktemp -d)
docker run --rm -v collect-poc-build-trixie:/build:ro -v "$CTX":/out "$PACKAGING" \
  sh -c 'strip -o /out/centengine /build/engine/centengine && chmod 755 /out/centengine'
cp .github/docker/centreon-engine/trixie/entrypoint/container.d/05-engine-config.sh "$CTX/"

# 4. Image.
docker build -t centreon-engine-trixie:gorgone-poc -f gorgone-rs/deploy/centreon/engine/Dockerfile "$CTX"
```

## Configuration and gRPC API

The image is configured like its base (`.github/docker/centreon-engine/trixie/`): it reads `/etc/centreon-engine/centengine.cfg` and the files that one lists, and `/etc/centreon-broker/` for `cbmod`. Copy or mount a configuration there; `.github/docker/centreon-engine/fixtures/minimal-config/` is a minimal one.

The entrypoint (`container.d/05-engine-config.sh`) writes `rpc_listen_address` into `centengine.cfg` at each start, from the environment variable `ENGINE_RPC_LISTEN_ADDRESS` (default `0.0.0.0`). The API is plain text and has no authentication: whoever reaches it runs any external command.

### Unix domain socket (recommended for a sidecar)

With `ENGINE_RPC_LISTEN_ADDRESS=unix:/absolute/path`, Engine serves the API on that socket only: `grpc_port` is ignored and no TCP listener is opened. The socket is created with mode `0660`, owner and group of the engine process (`centreon-engine`, uid and gid 901 in the image), so the file permissions are the access control: the sidecar mounts the volume that holds the socket and runs with uid or gid 901. A socket left by a previous run is replaced at start, and removed on a clean stop.

```bash
FIX=.github/docker/centreon-engine/fixtures/minimal-config
docker create --name engine-poc -v engine-poc-rw:/var/lib/centreon-engine/rw \
  -e ENGINE_RPC_LISTEN_ADDRESS=unix:/var/lib/centreon-engine/rw/engine.sock \
  centreon-engine-trixie:gorgone-poc
docker cp "$FIX/engine/." engine-poc:/etc/centreon-engine
docker cp "$FIX/broker/." engine-poc:/etc/centreon-broker
docker start engine-poc

PROTO=$(mktemp -d)
cp engine/enginerpc/engine.proto common/process_stat/process_stat.proto "$PROTO/"
chmod -R a+rX "$PROTO"   # mktemp -d is 0700 and grpcurl runs as uid 901
docker run --rm --user 901:901 -v engine-poc-rw:/rw -v "$PROTO":/proto:ro fullstorydev/grpcurl:v1.9.3 \
  -plaintext -import-path /proto -proto engine.proto \
  -d "{\"command\":\"[$(date +%s)] DISABLE_HOST_CHECK;test\"}" \
  unix:///rw/engine.sock com.centreon.engine.Engine/ProcessExternalCommand
docker logs engine-poc 2>&1 | grep 'EXTERNAL COMMAND'

# VerifyConfiguration: exit code and output of `centengine -v <path>`.
docker run --rm --user 901:901 -v engine-poc-rw:/rw -v "$PROTO":/proto:ro fullstorydev/grpcurl:v1.9.3 \
  -plaintext -import-path /proto -proto engine.proto \
  -d '{"path":"/etc/centreon-engine/centengine.cfg"}' \
  unix:///rw/engine.sock com.centreon.engine.Engine/VerifyConfiguration

docker rm -f engine-poc && docker volume rm engine-poc-rw
```

### Verifying a configuration before it is applied

`VerifyConfiguration` takes the absolute path of a main configuration file that Engine can read and answers with `exit_code` (0 when the configuration is valid; grpcurl omits the field when it is 0) and `output`, the standard output and error of the verification, cut at 1 MiB with a final `[output truncated at 1048576 bytes]` line. An empty or relative path, a control character in it, or a path that is not an existing regular file is refused with `INVALID_ARGUMENT`; a verification that lasts more than 60 seconds is killed and answered with `DEADLINE_EXCEEDED`.

Engine runs the verification as a child process, its own executable with `-v`, so the running configuration is neither read nor changed and several verifications can run at once. In that mode Engine loads no module and opens none of the files named by `log_file`, `status_file`, `state_retention_file` and `command_file`: a staging copy may keep those settings as generated. The files the verification does read are `cfg_file`, `cfg_dir` and `resource_file`, which must point into the staging directory, then `/etc/centreon-engine/engine-context.json` and the files that objects name (anomaly detection thresholds). The broker module configuration is not verified.

### TCP (default)

Without the variable, the API listens on `0.0.0.0`:`grpc_port`; `grpc_port` comes from `centengine.cfg` (`50155` in the packaged file and in the fixture). A sidecar on the same docker network reaches it at `<engine container>:50155`, and so does anything else on that network: do not publish the port.

```bash
FIX=.github/docker/centreon-engine/fixtures/minimal-config
docker network create engine-poc-net
docker create --name engine-poc --network engine-poc-net centreon-engine-trixie:gorgone-poc
docker cp "$FIX/engine/." engine-poc:/etc/centreon-engine
docker cp "$FIX/broker/." engine-poc:/etc/centreon-broker
docker start engine-poc

PROTO=$(mktemp -d)
cp engine/enginerpc/engine.proto common/process_stat/process_stat.proto "$PROTO/"
chmod -R a+rX "$PROTO"
docker run --rm --network engine-poc-net -v "$PROTO":/proto:ro fullstorydev/grpcurl:v1.9.3 \
  -plaintext -import-path /proto -proto engine.proto \
  -d "{\"command\":\"[$(date +%s)] DISABLE_HOST_CHECK;test\"}" \
  engine-poc:50155 com.centreon.engine.Engine/ProcessExternalCommand
docker logs engine-poc 2>&1 | grep 'EXTERNAL COMMAND'

docker rm -f engine-poc && docker network rm engine-poc-net
```
