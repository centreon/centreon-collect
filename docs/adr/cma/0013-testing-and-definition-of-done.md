# CMA-0013 — Testing strategy and definition of done

- Status: Proposed
- Owner: @centreon/owners-cpp (unit), @centreon/owners-robot-e2e (robot)
- Scope: `agent/test/**`, `tests/broker-engine/cma*.robot`, `tests/resources/Agent.py`, every PR touching `agent/`

## Context

Three test levels exist:

| Level | Where | Runs |
| --- | --- | --- |
| Unit (gtest) | `agent/test/*.cc` → binary `ut_agent` (`build/tests/ut_agent`) | Linux CI (`.github/scripts/collect-ut.sh agent`), Windows CI (`monitoring-agent.yml`, step "Agent test") |
| Integration Engine + Broker + agent (Robot Framework) | `tests/broker-engine/cma.robot`, `cma2.robot` (62 cases), keywords in `tests/resources/` | Linux robot jobs |
| Windows agent against a Linux poller | `.github/workflows/windows-agent-robot-test.yml` | Windows robot job |
| Installers | `agent/installer/scripts_linux/tests/` (containers per distribution), `.github/scripts/agent_installer_test.ps1` | manual / CI |

`ut_agent` specifics: `test_main.cc` runs the shared `g_io_context` on a dedicated thread for the whole run, sets `TZ=:Europe/Paris` on Linux (timeperiod tests depend on it), loads `ntdll` on Windows. Platform-only test files are listed per platform in `agent/test/CMakeLists.txt`. Scheduler tests inject a fake check (`tempo_check`) through the `check_builder` parameter of `scheduler::load`; native checks are tested by calling `compute(...)` on fabricated measures.

## Decision

1. Every behaviour change in `agent/` MUST come with a unit test that fails before the change and passes after it. Bug fixes add the reproducing case.
2. Unit tests MUST NOT depend on the real OS state they measure (CPU load, free memory, running services, event log content) for their assertions. They inject snapshots or use `compute()`; reading the real OS is allowed only to check "does not throw / returns a well-formed result".
3. Unit tests MUST NOT sleep to wait for asynchronous results when a condition can be awaited; when waiting is unavoidable, bound it and assert on a state, not on elapsed time alone. Time-sensitive scheduler tests assert with tolerances already used in `scheduler_test.cc`.
4. Tests touching the scheduler or checks MUST post work to `g_io_context` (or use the scheduler API) rather than mutate objects from the gtest thread while the io thread runs (CMA-0002).
5. A change in the protocol (CMA-0004), the OTLP encoding (CMA-0005), connection modes (CMA-0003), security (CMA-0009) or configuration keys (CMA-0010) MUST add or update a robot case in `tests/broker-engine/cma*.robot`, tagged with the Jira key (`[Tags] ... MON-xxxxx`), with `[Documentation]` in Given/When/Then form as existing cases.
6. Test naming: `TEST(<file_topic>_test, <behaviour>)` / `TEST_F(<fixture>, <behaviour>)`, snake_case; one test file per production file or check.
7. A new test file MUST be added to `agent/test/CMakeLists.txt` in the right platform list.

## Definition of done for a PR on `agent/` (human or Claude)

- [ ] Builds on Linux (`cmake --build build --target centagent ut_agent`), and on Windows when `src/`, `inc/`, `native_windows/`, `installer/` or `proto/` changed (Windows CI job green).
- [ ] `build/tests/ut_agent` passes; new tests present per rule 1.
- [ ] Robot case added or updated when rule 5 applies; impacted `cma*.robot` suites run.
- [ ] `clang-format` applied to changed lines only (CMA-0012).
- [ ] Each ADR guardrail of the touched scope checked; an intentional departure updates the ADR in the same PR.
- [ ] `agent/doc/agent-doc.md` and `--help` updated for user-visible check options (CMA-0008).
- [ ] Proto or OTLP change: Engine side changed in the same PR (CMA-0004, CMA-0005).
- [ ] One change per PR, opened as draft, self-reviewed before ready; commit title `type(agent): summary` or `type(cma): summary` with the Jira key in the PR.
- [ ] No construct the author cannot explain in review.

## Consequences

- Most regressions are caught by `ut_agent` in minutes; the robot suites remain the only check of the Engine contract.
- Native Windows checks are only tested on the Windows runner: a Linux-only contributor cannot validate them locally.

## Guardrails

- A PR without a test change while production behaviour changes.
- A test asserting on the host's real CPU/memory values or on wall-clock durations without tolerance.
- `std::this_thread::sleep_for` as the only synchronisation of a new test.
- A robot case without tag or documentation.
- A test file not registered in CMake (it silently never runs).

## References

- `agent/test/CMakeLists.txt`, `agent/test/test_main.cc`, `agent/test/scheduler_test.cc`
- `tests/broker-engine/cma.robot`, `tests/broker-engine/cma2.robot`, `tests/resources/Agent.py`, `tests/README.md`
- `.github/workflows/monitoring-agent.yml`, `.github/workflows/windows-agent-robot-test.yml`, `.github/workflows/collect.yml`
- `agent/installer/scripts_linux/tests/`
