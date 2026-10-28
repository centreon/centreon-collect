# CMA-0013: Ship every behaviour change with a test, and contract changes with a robot case

- **Status:** Proposed
- **Date:** 2026-10-28

## Context

The agent is tested at four levels:

- unit tests (gtest) in `agent/test/*.cc`, built as `ut_agent` (`build/tests/ut_agent`), run on Linux by `.github/scripts/collect-ut.sh agent` and on Windows by the "Agent test" step of `.github/workflows/monitoring-agent.yml`;
- Engine + Broker + agent integration tests (Robot Framework) in `tests/broker-engine/cma.robot` and `cma2.robot`, 62 cases, keywords in `tests/resources/` (`Agent.py`);
- a Windows agent against a Linux poller, `.github/workflows/windows-agent-robot-test.yml`;
- installer tests: `agent/installer/scripts_linux/tests/` (one container per distribution) and `.github/scripts/agent_installer_test.ps1`.

In `ut_agent`, `test_main.cc` runs the shared `g_io_context` on a dedicated thread for the whole run, sets `TZ=:Europe/Paris` on Linux (timeperiod tests depend on it) and loads `ntdll` on Windows. Platform-only test files are listed per platform in `agent/test/CMakeLists.txt`. Scheduler tests inject a fake check (`tempo_check`) through the `check_builder` parameter of `scheduler::load`; native checks are tested by calling `compute(...)` on fabricated measures. Robot cases carry the Jira key in `[Tags]` and a Given/When/Then `[Documentation]`.

Alternatives weighed:

- Rely on robot tests only. It lost: they need a full poller, run for minutes, and cannot reach Windows native checks from the Linux runners.
- Test native checks against the real OS. It lost: assertions on CPU load, free memory or running services are not reproducible on CI runners.

## Decision

Ship every behaviour change with a unit test that fails before the change and passes after it; a bug fix adds the reproducing case. Feed native checks fabricated snapshots through `compute()`, and read the real OS only to check that a measure is well formed. Wait for asynchronous results on a condition, bounded in time, with the tolerances already used in `scheduler_test.cc`. Reach checks and the scheduler through `g_io_context` or the scheduler API, never from the gtest thread while the io thread runs. Name tests `TEST(<topic>_test, <behaviour>)` or `TEST_F(<fixture>, <behaviour>)` in snake_case, one test file per production file or check, and register it in `agent/test/CMakeLists.txt`.

Add or update a robot case in `tests/broker-engine/cma*.robot`, tagged with the Jira key and documented in Given/When/Then form, for any change to the protocol (CMA-0004), the OTLP encoding (CMA-0005), the connection modes (CMA-0003), security (CMA-0009) or the local configuration keys (CMA-0010).

Before marking a pull request on `agent/` ready, human- or Claude-produced: build `centagent` and `ut_agent` on Linux, and get the Windows job green when `src/`, `inc/`, `native_windows/`, `installer/` or `proto/` changed; run `ut_agent` and the impacted `cma*.robot` suites; format the changed lines; check the change against the Decision of every ADR of the touched scope, and update an ADR in the same pull request when departing from it; update `agent-doc.md` and `--help` for user-visible check options; change the Engine side together with a proto or OTLP change. Keep one change per pull request, opened as a draft, titled `type(agent): summary` or `type(cma): summary`, and do not merge a construct the author cannot explain.

## Consequences

- Most regressions are caught by `ut_agent` in minutes; robot suites remain the only check of the Engine contract.
- Windows native checks are tested on the Windows runner only; a Linux-only contributor cannot validate them locally.
- A test file missing from `agent/test/CMakeLists.txt` silently never runs.
- Time-based assertions without tolerance flake on loaded CI runners, which is why they are excluded.

## When to Amend

- A local Windows test path (container, cross-compilation, emulation) becomes available to Linux contributors.
- The robot suites move to another framework, or the agent gains a contract test against Engine that replaces robot cases.
