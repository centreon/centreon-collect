# agent/ — Centreon Monitoring Agent (CMA, binary `centagent`)

Runs checks on Linux and Windows hosts for a Centreon poller. Receives its check list from Engine over gRPC, schedules checks itself, sends results as OTLP metrics. Engine counterpart: `engine/modules/opentelemetry/src/centreon_agent/`.

## Read first

The ADRs of this area are in `docs/adr/cma/`; `docs/adr/cma/ARCHITECTURE.md` maps them onto components and flows. Read the ones of the scope you touch before changing code; a change that departs from an ADR updates the ADR in the same PR.

| Touching | ADR |
| --- | --- |
| CMake, new file, `native_*` | CMA-0001 |
| any callback, timer, thread | CMA-0002 |
| `bireactor`, `streaming_*` | CMA-0003 |
| `proto/agent.proto`, `agent_info` | CMA-0004 |
| metric/exemplar encoding in `scheduler.cc` | CMA-0005 |
| scheduling, soft/hard, timeperiods | CMA-0006 |
| `check`, `check_exec`, `default_check_builder` | CMA-0007 |
| a native check, filters | CMA-0008 |
| TLS, tokens, CA bootstrap, credentials, logs of command lines | CMA-0009 |
| `config*.cc`, installers, `conf/` | CMA-0010 |
| log lines | CMA-0011 |
| any C++ | CMA-0012, CMA-0013 |

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

## Invariants (review rejects a violation)

- One `io_context` thread. Other threads (gRPC, storage/files workers, event log callback) hand over with `asio::post`. No blocking call on the io thread. No mutex outside those boundaries.
- Async lambdas capture `shared_from_this()`; handlers return on `err`.
- `check::start_check` starts with `_start_check(timeout)`; completion is always asynchronous and carries the running index captured at start.
- `agent.proto`: add fields only, never renumber or reuse; zero value = previous behaviour; Engine side in the same PR.
- OTLP names (`status`, `host.name`, `service.name`, exemplar labels, perfdata labels) are a contract with Engine and the packs: never renamed.
- `src/`, `inc/`, `native_linux/` are C++17 (Linux host). C++20 only in `native_windows/`.
- Secrets (decrypted command line, token, key, salt) never logged nor put in outputs; a command line is logged only at TRACE; `decrypt_args` always followed by `clear_unencrypted_args`.
- A local config key exists in `config.cc`, `config_win.cc` and both installers, or nowhere.
- Every behaviour change has a unit test registered in `agent/test/CMakeLists.txt`.

## Stop and ask the owner before

- changing `agent.proto`, the OTLP encoding, the scheduler algorithm or `calcul_status_confirmation`;
- adding a thread, a dependency in `vcpkg.json`, or a link library;
- anything under CMA-0009 (security review by @centreon/owners-security);
- changing `common/**` (shared with Engine and Broker).

## Layout

`inc/com/centreon/agent/` + `src/` common core · `native_linux/` · `native_windows/` · `proto/` · `test/` (`ut_agent`) · `installer/` · `conf/` · `doc/agent-doc.md` (scheduler and filter design notes).
