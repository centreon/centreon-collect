# centreon-engine image for the gorgone-rs proof of concept

`centreon-engine-trixie:develop` with `/usr/sbin/centengine` replaced by a build of this branch, which adds the `ProcessExternalCommand` RPC to the Engine gRPC API (`engine/enginerpc/engine.proto`). Nothing else changes: enginerpc is linked statically into `centengine`, and the binary keeps loading the shared objects of the image packages (`cbmod.so`, `libcentreon_clib.so`, `libcentreon_engine_conf.so`), so the branch must not diverge from `develop` in `common/`, `clib/`, `bbdo/` or `broker/neb/`.

## Build

All commands run from the repository root. The build tree lives in a docker volume, never in the working tree.

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

# 2. Unit tests of the gRPC API.
docker run --rm -v "$PWD":/src:ro -v collect-poc-build-trixie:/build -w /build "$PACKAGING" \
  tests/ut_engine --gtest_filter='EngineRpc.*'

# 3. Build context: the stripped binary alone, outside the working tree.
CTX=$(mktemp -d)
docker run --rm -v collect-poc-build-trixie:/build:ro -v "$CTX":/out "$PACKAGING" \
  sh -c 'strip -o /out/centengine /build/engine/centengine && chmod 755 /out/centengine'

# 4. Image.
docker build -t centreon-engine-trixie:gorgone-poc -f gorgone-rs/deploy/centreon/engine/Dockerfile "$CTX"
```

## Configuration and gRPC API

The image is configured like its base (`.github/docker/centreon-engine/trixie/`): it reads `/etc/centreon-engine/centengine.cfg` and the files that one lists, and `/etc/centreon-broker/` for `cbmod`. Copy or mount a configuration there; `.github/docker/centreon-engine/fixtures/minimal-config/` is a minimal one.

The gRPC API listens on `rpc_listen_address`:`grpc_port`. The entrypoint (`container.d/05-engine-config.sh`) forces `rpc_listen_address=0.0.0.0` at each start; `grpc_port` comes from `centengine.cfg` (`50155` in the packaged file and in the fixture). A sidecar on the same docker network reaches it at `<engine container>:50155`, in plain text and without authentication: do not publish the port.

```bash
FIX=.github/docker/centreon-engine/fixtures/minimal-config
docker network create engine-poc-net
docker create --name engine-poc --network engine-poc-net centreon-engine-trixie:gorgone-poc
docker cp "$FIX/engine/." engine-poc:/etc/centreon-engine
docker cp "$FIX/broker/." engine-poc:/etc/centreon-broker
docker start engine-poc

PROTO=$(mktemp -d)
cp engine/enginerpc/engine.proto common/process_stat/process_stat.proto "$PROTO/"
docker run --rm --network engine-poc-net -v "$PROTO":/proto:ro fullstorydev/grpcurl:v1.9.3 \
  -plaintext -import-path /proto -proto engine.proto \
  -d "{\"command\":\"[$(date +%s)] DISABLE_HOST_CHECK;test\"}" \
  engine-poc:50155 com.centreon.engine.Engine/ProcessExternalCommand
docker logs engine-poc 2>&1 | grep 'EXTERNAL COMMAND'

docker rm -f engine-poc && docker network rm engine-poc-net
```
