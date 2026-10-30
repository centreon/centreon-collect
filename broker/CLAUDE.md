# broker/ — Centreon Broker (`cbd`, `cbmod`)

BBDO contract: `bbdo/`. Engine side of `cbmod`: `engine/src/broker.cc` (see `docs/architecture/engine.md`). Database schema reference: `resources/centreon.sql`, `resources/centreon_storage.sql`. Testing rules: `docs/testing.md`.

## Commands

```bash
# configure once (Linux, from repo root; see README.md for vcpkg setup)
cmake -B build -DVCPKG_OVERLAY_TRIPLETS=custom-triplets -DVCPKG_TARGET_TRIPLET=x64-linux \
      -DVCPKG_OVERLAY_PORTS=overlays -DWITH_TESTING=On -DONLY_ROBOT_CMA=OFF -S .
cmake --build build --target cbd cbmod ut_broker
build/tests/ut_broker --gtest_filter='<suite>.*'   # SQL tests need -DWITH_SQL_TESTS=ON
.github/scripts/collect-ut.sh broker              # what CI runs, from the repo root
clang-format -i <changed files>                   # root .clang-format, Chromium
```

Robot: `tests/broker/`, `tests/broker-engine/`, `tests/bam/` (setup in `tests/README.md`, `./init-proto.sh`, then `./robot.sh <dir>/<suite>.robot` from `tests/`).

## Architecture

@../docs/architecture/broker.md

## Conventions

@../docs/conventions/broker.md
