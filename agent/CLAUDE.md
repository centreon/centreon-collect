# agent/ — Centreon Monitoring Agent (CMA, binary `centagent`)

Engine counterpart: `engine/modules/opentelemetry/src/centreon_agent/`. Testing rules: `docs/testing.md` § Centreon Monitoring Agent.

## Commands

```bash
# configure once (Linux, from repo root; see README.md for vcpkg setup)
cmake -B build -DVCPKG_OVERLAY_TRIPLETS=custom-triplets -DVCPKG_TARGET_TRIPLET=x64-linux \
      -DVCPKG_OVERLAY_PORTS=overlays -DWITH_TESTING=On -DWITH_CREATE_FILES=OFF -DWITH_CONF=OFF -S .
cmake --build build --target centagent ut_agent
build/tests/ut_agent                              # all unit tests
build/tests/ut_agent --gtest_filter='scheduler_test.*'
clang-format -i <changed files>                   # root .clang-format, Chromium
```

Robot: `tests/broker-engine/cma.robot`, `cma2.robot` (setup in `tests/README.md`). Windows code compiles only in the Windows CI job (`.github/workflows/monitoring-agent.yml`).

## Architecture

@../docs/architecture/cma.md

## Conventions

@../docs/conventions/cma.md
