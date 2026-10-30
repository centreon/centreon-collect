# Automated tests in centreon-collect

How to write the automated tests that CI runs, for every component. Architecture and CI workflows: [`ARCHITECTURE.md`](../ARCHITECTURE.md). Code conventions: [`conventions/global.md`](conventions/global.md).

## What a change ships with

| Change | Unit test | Robot test |
|---|---|---|
| Behaviour change or new feature in C++ code | Required | When it crosses a process boundary (see below) |
| Bug fix | Required: the case that reproduces the bug, failing before the fix | When the bug was only visible end to end |
| Contract change (`bbdo/*.proto`, `agent.proto`, OTLP encoding, gRPC APIs) | Required on both sides | Required |
| Configuration option (Engine, Broker or CMA) | Required (parsing, default) | Required when the option changes runtime behaviour |
| Security behaviour (TLS, tokens, credentials) | Required | Required, with the failure path |
| Refactoring without behaviour change | Existing tests stay green, none removed | — |
| Packaging or installer | — | Installer tests of the component |

A test is added in the same pull request as the change. A test is never deleted or weakened to make CI green; a test that has to change says why in the pull request.

## Test levels

| Level | Where | Run by |
|---|---|---|
| Unit (GoogleTest) | `common/tests/` → `ut_common`, `engine/tests/` → `ut_engine`, `broker/**/test/` → `ut_broker`, `clib/test/` → `ut_clib`, `connectors/*/test/` → `ut_connector`, `agent/test/` → `ut_agent` | `collect.yml` (Linux, `.github/scripts/collect-ut.sh <component>`), `monitoring-agent.yml` (`ut_agent`, Linux and Windows) |
| Integration (Robot Framework) | `tests/<area>/*.robot`, keywords in `tests/resources/` | `robot-test.yml`, called by `collect.yml` and `monitoring-agent.yml` |
| Windows agent end to end | `tests/broker-engine/cma*.robot` against a Windows agent | `windows-agent-robot-test.yml` |
| Installers | `agent/installer/scripts_linux/tests/`, `.github/scripts/agent_installer_test.ps1` | `monitoring-agent.yml` |
| Gorgone | `gorgone/tests/unit/` (Perl), `gorgone/tests/robot/` | `gorgone.yml`, `robot-test-gorgone.yml` |

Prefer the unit level: it runs in minutes and pinpoints the failure. Robot tests start real Engine, Broker, agent and database processes; they are the only check of what crosses a process boundary (protocols, database content, files written, reloads, restarts).

## Unit tests

### Writing

- GoogleTest, one test file per production file or feature, next to the existing tests of the component. Name tests like their neighbours (`TEST(<topic>_test, <behaviour>)` in snake_case for new code; existing Engine fixtures in CamelCase stay as they are).
- **Register the file** in the component's test `CMakeLists.txt`. A file missing from it never builds, never runs, and CI stays green.
- One behaviour per test, named after it. Arrange the input in the test, call the code, assert on the result.
- Feed fabricated data: no assertion on values measured on the machine (CPU, memory, disk, running processes, hostname, current time). Read the real system only to check that a value is well formed.
- Asynchronous code: wait on a condition, bounded in time (loop with a deadline, `std::condition_variable::wait_for`), never a bare `sleep` followed by an assertion. Time assertions carry a tolerance: CI runners are loaded.
- Temporary files under a name unique to the test, removed at the end. Each binary gets a private `/tmp` in CI (`collect-ut.sh`), but tests of one binary share it.
- No network access outside `localhost`, no dependency on test order, no global state left behind (reset singletons and configuration in `TearDown`).
- CI compiles with `-Wall -Wextra`: a test introduces no warning.

### Running locally

```bash
cmake -B build ... -DWITH_TESTING=On -S .      # see ARCHITECTURE.md § Build
cmake --build build --target ut_<component>
build/tests/ut_<component> --gtest_filter='<suite>.*'
```

## Robot tests

### Layout and CI dispatch

- A suite is a `.robot` file under `tests/<area>/` (`broker/`, `engine/`, `broker-engine/`, `bam/`, `ccc/`, `connector_perl/`, `connector_ssh/`...). Each suite becomes **one CI job**: `robot-test.yml` lists every `.robot` file containing `*** Test Cases ***`. Split a suite that grows long rather than slowing one job.
- Suites whose path contains `cma` run only in `monitoring-agent.yml` (pattern `cma.*\.robot`) and are excluded from `collect.yml` (pattern `^((?!cma).)*$`). A CMA suite is named `cma*.robot`; any other suite never contains `cma` in its path.
- A suite runs on MariaDB by default. An empty companion file `<suite>.robot.mysql` makes it run on MySQL too; add it when the suite checks database content.
- CI runs `robot -e unstable`: a case tagged `unstable` is skipped, and a suite whose path contains `unstable` is not run at all. Tagging a case `unstable` hides it from CI; it is a temporary measure with a Jira ticket, never a way to merge a failing test.
- `tests/README.md` lists the tests; it is generated from the `[Documentation]` of each case by `tests/update-doc.py` (run from `tests/`).

### Writing a suite

```robot
*** Settings ***
Documentation       <What the suite covers>

Resource            ../resources/import.resource
Library             ../resources/Agent.py           # extra keyword libraries if needed

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Stop Engine Broker And Save Logs

*** Test Cases ***
<SUITE_PREFIX><NUMBER_OR_NAME>
    [Documentation]    Given <state>
    ...    When <action>
    ...    Then <expected result>
    [Tags]    <component>    <feature>    MON-12345
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    ...
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    <message naming what was expected>
```

- Copy the `*** Settings ***` of a neighbouring suite of the same area: setup and teardown stop the processes and save their logs for the CI report.
- Test case name: upper case, with the prefix used by the suite, unique in the repository.
- `[Documentation]` in Given / When / Then form. `[Tags]`: the components involved, the feature, and the Jira key of the change.
- Each case configures what it needs from scratch (`Ctn Config ...`, `Ctn Clear Retention`); a case never depends on the previous one.
- Wait on a condition with a timeout: `Ctn Find In Log With Timeout`, `Ctn Check ... With Timeout`, `Ctn Wait For ...`. Use `Sleep` only when the product itself defines a delay that must elapse, with a comment saying which.
- Every assertion carries a message that says what was expected (`Should Be True    ${result}    <message>`).
- Do not grep a log line that is not stable on purpose; when a test greps a log message, changing that message changes the test in the same pull request.

### Keywords

- Python keywords live in `tests/resources/<Component>.py` (`Common.py`, `Engine.py`, `Broker.py`, `Agent.py`...). A function `ctn_<name>` is called `Ctn <Name>` from Robot. Every new keyword has the `ctn_` prefix and a docstring.
- Look for an existing keyword before writing one; extend it with an optional argument rather than duplicating it.
- A keyword that waits takes a `timeout` argument in seconds and returns a boolean; the test asserts on it.
- Robot-level shared keywords and variables go to `tests/resources/*.resource`.
- Formatting: `robottidy --overwrite tests` for Robot, autopep8 for Python.

### Running locally

Setup (Robot Framework, Python modules, `opentelemetry-proto`, `init-proto.sh`, `init-sql.sh`): `tests/README.md`. Then, from `tests/`:

```bash
./robot.sh broker-engine/cma.robot            # one suite, TRACE logs, results in a dated directory
robot -t BEOTEL_CENTREON_AGENT_CHECK_HOST broker-engine/cma.robot   # one case
```

## Centreon Monitoring Agent

Architecture: [`architecture/cma.md`](architecture/cma.md). Conventions: [`conventions/cma.md`](conventions/cma.md).

### `ut_agent`

- `agent/test/test_main.cc` runs the shared `g_io_context` on a dedicated thread for the whole run, sets `TZ=:Europe/Paris` on Linux (timeperiod tests depend on it) and loads `ntdll` on Windows.
- Reach checks and the scheduler through `g_io_context` (`asio::post`) or the scheduler API, never from the gtest thread while the io thread runs.
- Scheduler tests inject a fake check (`tempo_check`) through the `check_builder` parameter of `scheduler::load`. Reuse the timing tolerances already in `scheduler_test.cc`.
- Native checks are tested by feeding fabricated snapshots to `compute(...)`. Test file `test/check_<name>_test.cc`, or `check_linux_<name>_test.cc` / `check_windows_<name>_test.cc` when both platforms have their own code; platform-only files are listed under the platform condition of `agent/test/CMakeLists.txt`.
- A change to `check::calcul_status_confirmation` adds `nagios_confirmation_*` / `first_check_*` cases to `check_test.cc`.
- A change to OTLP names or exemplars updates `scheduler_test.cc` (`correct_output_examplar`).
- Filter grammars are tested in `filter_test.cc`.
- Windows native checks compile and run only on the Windows runner (`monitoring-agent.yml`). A change in `src/`, `inc/`, `native_windows/`, `installer/` or `proto/` waits for the Windows job before being marked ready.

### `cma*.robot`

- Suites `tests/broker-engine/cma.robot` and `cma2.robot` run Engine, Broker and the agent together; keywords in `tests/resources/Agent.py`.
- A robot case is required for a change to `agent.proto`, the OTLP encoding, the connection modes (direct and reversed: cover both), security (wrong token, wrong fingerprint, plain-text expiry) or a local configuration key.
- `windows-agent-robot-test.yml` runs a fixed list of suites against a Windows agent (`robot-files: [cma.robot, cma2.robot]`). A new CMA suite that must also cover Windows is added to that list.

### Before marking a CMA pull request ready

1. `centagent` and `ut_agent` build on Linux; the Windows job is green when Windows-compiled code changed.
2. `ut_agent` passes; the impacted `cma*.robot` suites pass.
3. Every new test file is registered in `agent/test/CMakeLists.txt`.
4. User-visible check options are documented in `agent/doc/agent-doc.md` and `--help`.
5. A proto or OTLP change includes the Engine side and its tests.
