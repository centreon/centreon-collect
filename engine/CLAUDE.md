# engine/ — Centreon Engine (binary `centengine`)

Configuration model: `common/engine_conf/`. Broker bridge: `engine/src/broker.cc` with `broker/neb/` (`cbmod`, Broker code, see `docs/architecture/broker.md`). CMA side of the OpenTelemetry module (`modules/opentelemetry/**/centreon_agent/`): `agent/CLAUDE.md` and the CMA files. Testing rules: `docs/testing.md`.

## Commands

```bash
# configure once (Linux, from repo root; see README.md for vcpkg setup)
cmake -B build -DVCPKG_OVERLAY_TRIPLETS=custom-triplets -DVCPKG_TARGET_TRIPLET=x64-linux \
      -DVCPKG_OVERLAY_PORTS=overlays -DWITH_TESTING=On -DONLY_ROBOT_CMA=OFF -S .
cmake --build build --target centengine externalcmd opentelemetry ut_engine bin_connector_test_run
cd build && tests/ut_engine --gtest_filter='<suite>.*'   # from build/: tests use tests/bin_connector_test_run
.github/scripts/collect-ut.sh engine                     # what CI runs, from the repo root
clang-format -i <changed files>                          # root .clang-format, Chromium
```

Robot: `tests/engine/`, `tests/broker-engine/` (setup in `tests/README.md`, then `./robot.sh <dir>/<suite>.robot` from `tests/`).

## Architecture

@../docs/architecture/engine.md

## Conventions

@../docs/conventions/engine.md
