# Centreon Monitoring Agent architecture reference

Reference for writing and reviewing code of the Centreon Monitoring Agent (`agent/`). Each section states the rule, its severity, and the ADR that carries the reasoning (`docs/adr/cma/`). The severity levels are proposed with the ADRs and validated with them by `@centreon/owners-cpp`.

## Zones

| Zone | Path | Strictness |
|------|------|------------|
| **Contracts** | `agent/proto/agent.proto`, the OTLP encoding in `agent/src/scheduler.cc`, `engine/modules/opentelemetry/src/centreon_agent/` | Strictest. Any change that an older or newer peer cannot read blocks. Both sides change in the same PR |
| **Agent** | `agent/src/`, `agent/inc/`, `agent/native_linux/`, `agent/native_windows/` | Strict. Every CMA ADR applies; architectural violations block the merge |
| **Install and packaging** | `agent/installer/`, `agent/conf/`, `packaging/centreon-monitoring-agent/` | Configuration parity, correctness and security only; no architecture or style findings |
| **Shared libraries** | `common/grpc/`, `common/process/`, `common/crypto/`, `common/inc/`, `common/engine_conf/` | Correctness, CI and security block for every consumer (agent, Engine, Broker); agent-specific rules do not apply; architectural gaps are non-blocking tickets |
| **Tests** | `agent/test/`, `tests/broker-engine/cma*.robot`, `tests/resources/Agent.py` | Test rules of CMA-0013 |

**Dependency direction.** The agent depends on `common/` and third-party libraries only. It never includes Engine or Broker code. Engine depends on the agent through `agent.proto` only. **⟶ an agent include of `engine/` or `broker/` blocks.**

**Shared code, always ask who else is hit.** A change in `common/` used by the agent is also a change for Engine and Broker. Build and test them, and say so in the PR. **⟶ an untested consumer blocks.**

---

## Blocking criteria (by zone)

This table is the canonical severity reference; the `⟶` annotations below roll up here.

| What blocks | Contracts | Agent | Install / packaging | Shared libs |
|---|:---:|:---:|:---:|:---:|
| Runtime bug, crash, lost or duplicated check result, silent failure | ✓ | ✓ | ✓ | ✓ |
| Security problem, secret exposed in a log, an output or a file | ✓ | ✓ | ✓ | ✓ |
| CI failure (Linux or Windows build, `ut_agent`, robot suites) | ✓ | ✓ | ✓ | ✓ |
| Change an old or new peer cannot read (proto, OTLP names, exemplars) | ✓ |  |  |  |
| Threading violation (blocking the `io_context` thread, state shared across threads without a boundary) |  | ✓ |  | ✓ |
| Architectural violation (wrong platform tree, check outside the lifecycle, scheduler bypass) |  | ✓ |  |  |
| Configuration key missing from one of JSON, registry, installers |  | ✓ | ✓ |  |
| Missing test for a behaviour change | ✓ | ✓ |  |  |

Formatting and naming gaps never block: they are suggestions on changed lines, never a reason to reformat untouched code.

---

## Convention source of truth

The agent code in `centreon-collect` is the reference for agent conventions, and these ADRs are its written form. Engine and Broker are *siblings*, older than the agent: verify a pattern against this document and the surrounding `agent/` code before copying it from them. When a convention isn't covered here, the surrounding `agent/` code wins. A place listed in an ADR's Consequences as "not yet followed" or as a known defect is never a model.

---

## Components

| Component | Files | Role | ADR |
|---|---|---|---|
| Entry points | `src/main.cc`, `src/main_win.cc` | start-up, signals, Windows service, plain-text expiry | 0002, 0009, 0011 |
| Local configuration | `src/config.cc`, `src/config_win.cc`, `inc/.../config.hh` | singleton read once, custom checks hot reload | 0010 |
| CA bootstrap | `src/grpc_ca_bootstrap.cc` | fetch and pin the CA by fingerprint | 0009 |
| Transport | `bireactor`, `streaming_client`, `streaming_server` | one bidirectional stream, reconnect, token check | 0003 |
| Scheduler | `src/scheduler.cc` | rebuild on configuration, spreading, slots, concurrency, timeperiods, OTLP batching | 0006, 0005 |
| Check builder | `scheduler::default_check_builder` | decrypt, JSON → native, else exec, errors → `check_dummy` | 0007 |
| Check base | `check.hh`, `src/check.cc` | lifecycle, timeout, running index, HARD/SOFT | 0007, 0006 |
| Plugin execution | `src/check_exec.cc` | process spawn, output and perfdata parsing, credentials kept encrypted | 0007, 0009 |
| Native checks | `native_linux/`, `native_windows/`, `src/native_check_*`, `src/drive_size.cc`, `src/check_health.cc` | OS measures, thresholds, perfdata | 0008 |
| Filters | `src/filter.cc`, `src/filter_rules.cc` | NSClient++-like expressions (boost::parser) | 0008 |

Runtime flow: `main` loads the configuration, bootstraps the CA if needed, opens the stream and sends `AgentInfo`. Engine sends `AgentConfiguration`; the gRPC thread posts it to the scheduler, which rebuilds and spreads every check. Each check starts in its slot and completes asynchronously; the scheduler computes HARD/SOFT, stores OTLP data and re-queues the check at its next absolute time. Every `export_period`, or earlier when needed, the request is written to the stream.

---

## Platform split and build

Code that runs on both platforms lives in `src/` + `inc/com/centreon/agent/` and is **C++17** (Linux host). Platform code lives in `native_linux/` or `native_windows/`; C++20 (`std::format`, `<ranges>`, concepts) only in `native_windows/`. **⟶ a C++20 construct in the core blocks** (it breaks the Linux build). A Windows API (`windows.h`, `Pdh*`, `Evt*`) or a Linux-only API (`/proc`, `localtime_r`) outside a platform tree or an `#ifdef _WIN32` block **⟶ blocks**.

Every source and test file is listed by hand in `agent/CMakeLists.txt` / `agent/test/CMakeLists.txt`; a file missing from the list never builds or never runs. Generated `*.pb.*` files are never committed. New libraries come from the root `vcpkg.json`. The Linux link stays `-static-libgcc -static-libstdc++`.

ADR: CMA-0001.

---

## Threading model

One `io_context` thread runs the scheduler, the checks, the timers and the signal handlers. Other threads (gRPC callbacks, `drive_size_thread`, `check_files_thread`, the Event Log callback) hand over with `asio::post` before touching agent state. **⟶ blocks in Agent and Shared libs.**

- No blocking call on the `io_context` thread once `run()` has started: no `sleep_for` / `Sleep()`, no synchronous gRPC call, no `boost::process::wait`, no file-system call that can hang, no `future::get()`.
- A blocking OS call runs in a worker built like `drive_size_thread`: static worker, queue under `absl::Mutex`, results posted back, `thread_kill()` after `run()`.
- A mutex appears only at a thread boundary. A mutex in the scheduler or a check with no OS callback or worker is a design smell to challenge.
- Every async lambda captures `shared_from_this()` (or a locked `weak_ptr`); `this` only next to it. Every handler returns at once on an error code.
- `spdlog::flush_every` is banned (a forked child can hang on its mutex).

ADR: CMA-0002.

---

## Transport

One long-lived bidirectional gRPC stream per Engine, client (`AgentService.Export`) or server (`ReversedAgentService.Import`) depending on `reversed_grpc_streaming`. Both modes stay functionally identical: same first message (`AgentInfo`), same scheduler, same builder. **⟶ a feature added to one mode only blocks, unless the PR says why.**

- A reactor is registered with `register_stream()` before gRPC gets it, and released only by the posted `OnDone`. Deleting it from a gRPC callback crashes gRPC. **⟶ blocks.**
- Writes go through `bireactor::write()` (one in flight); state shared with gRPC threads is under `_protect`; incoming requests are posted to the `io_context`.
- Channels, servers, TLS and keepalive come from `common::grpc` base classes, never hand-built.
- A new message kind is a new `oneof` member, never a new RPC.

ADR: CMA-0003.

---

## Protocol contract

`agent.proto` evolves by addition only. **⟶ any of the following blocks:**

- a renumbered, retyped or reused field number; a removed field not declared `reserved`;
- a new field whose zero value does not mean the previous behaviour (`optional` when absent and zero differ);
- a new capability sent by Engine without being announced in `AgentInfo` (pattern: `encryption_ready`);
- an `AgentInfo` field filled on one platform only;
- a proto change without its Engine side, or without a robot case when behaviour changes.

`AgentConfiguration` is a full snapshot; the agent ignores message content it does not understand.

ADR: CMA-0004.

---

## OTLP export contract

Engine parses check results by name. The metric `status`, the attributes `host.name` / `service.name`, the exemplar labels (`status_confirmed`, `current_attempt`, `crit_*`, `warn_*`, `min`, `max`) and the perfdata type keys never change. **⟶ a rename blocks.**

- One `ResourceMetrics` per service per request; a second result flushes the request first.
- An exemplar is emitted only when its value is set (`NaN` skipped by the `<= max` test).
- Every string placed in a message goes through `common::check_string_utf8`. **⟶ an unsanitised string blocks** (protobuf rejects invalid UTF-8 and the whole request is lost).
- The early-flush threshold (2 MiB) stays below the minimum `max_message_length` (4 MiB).

ADR: CMA-0005.

---

## Scheduler

Checks run on absolute time slots, rebuilt from each configuration. **⟶ any loss of these properties blocks:** no drift (next start from absolute time points, never "end + interval"); first starts spread over the smallest interval; one check per slot, collisions moved forward; `max_concurrent_checks` never exceeded; results of an old configuration never stored or re-queued.

- A check is re-queued only on completion or timeperiod deferral, and removed from the queue before it starts.
- Timeperiods use the `common/` functions shared with Engine, never a local reimplementation.
- `calcul_status_confirmation` changes only with new `nagios_confirmation_*` / `first_check_*` cases in `check_test.cc`: Engine trusts `status_confirmed` and `current_attempt` as sent.

ADR: CMA-0006.

---

## Check lifecycle

Every check derives from `check` (or `native_check_base<N>` / `native_check_cpu<N>`), is built with `make_shared` or a static `load()`, and validates its arguments in the constructor (an exception becomes a `check_dummy`, CRITICAL). **⟶ blocks in Agent:**

- `start_check` not starting with `if (!_start_check(timeout)) return;`;
- a result delivered synchronously from `start_check`, or without the running index captured at start;
- an exception escaping `start_check`;
- a run-time failure reported with another status than `unknown`.

A new check type is registered in `default_check_builder` (under its platform `#ifdef`) and printed by `--help`.

ADR: CMA-0007.

---

## Native checks

- Arguments: JSON `args` read with `rapidjson_helper`, numbers accepted as numbers or strings, documented defaults, kebab-case names (`warning-<x>`, `critical-<x>`, `filter-<x>`, `<x>-syntax`), NSClient++ spellings kept in NSClient++-compatible checks.
- Thresholds: `common::threshold` only, reported with `set_pref_details_w/_c`. **⟶ hand-written threshold parsing (`std::stod`, regex) blocks.**
- Output starts with the status label; CRITICAL > WARNING > OK; UNKNOWN when the indicator cannot be measured.
- Perfdata: new labels `<object>.<measure>.<unit>`, lower case, ASCII, `#` instance prefix, base units, unscaled values. **⟶ renaming an existing label or argument blocks** (it breaks installed packs); a new label is announced to the pack owners (CTOR).
- Every native check has `help()`, a section in `agent-doc.md` and a test feeding fabricated measures to `compute()`.

ADR: CMA-0008.

---

## Local configuration

A local key exists in all of: JSON schema and parser (`config.cc`), registry reader (`config_win.cc`), `config.hh` getter, Linux installer, Windows installer and silent-install script, or in none. **⟶ a key on one side only blocks in Agent and Install.**

- Every option has a default that keeps the previous behaviour; `endpoint` stays the only required key.
- JSON aliases (`ca` / `ca_certificate`, `ca_common_name` / `ca_name`) are permanent; a renamed key keeps the old name as alias.
- `config::instance()` is read-only after start-up; only custom checks reload, all or nothing.

ADR: CMA-0010.

---

## Logging

Log with `SPDLOG_LOGGER_<LEVEL>(_logger, ...)` and the injected logger; global `SPDLOG_*` only before `g_logger` exists. Level by meaning and frequency: anything emitted per check or per message is DEBUG or TRACE. **⟶ a per-check INFO line is a finding** (it floods hosts running hundreds of checks). Each message names its subject (service, command, peer). Secrets never appear (see Security).

ADR: CMA-0011.

---

## Error handling and code conventions

- Errors are `com::centreon::exceptions::msg_fmt` with a message naming the command or service; catch `const std::exception&`. **⟶ an exception that can reach `io_context::run()` blocks** (it ends the agent).
- Durations and time points cross agent classes as `std::chrono` types, never raw integers.
- Names snake_case, `e_` enum types, `_` prefix for private members and methods, `#ifndef CENTREON_AGENT_<NAME>_HH` guards, `com::centreon::agent` namespace, no `using namespace` in headers.
- Format changed lines with the root `.clang-format` (Chromium); no unrelated reformatting in a functional diff.
- No commented-out code, no ticket number in comments.

ADR: CMA-0012.

---

## Security

Every PR is potentially a security regression. The agent runs commands sent by a remote poller on customer hosts. The common vectors:

- **Secret exposure.** A decrypted command line, a token, a key, a salt or a private key in a log, a check output or a file. `get_command_line()` is plain text for non-encrypted commands: never log it above TRACE, never put it in an output.
- **Credential lifetime.** Every `decrypt_args` is followed by `clear_unencrypted_args`, on every path including exceptions.
- **Transport downgrade.** `insecure` stays an explicit choice; `TLS_SKIP_VERIFY_CA` appears only in `grpc_ca_bootstrap.cc`, and its CA is discarded unless the fingerprint matches. The one-hour plain-text stop is never removed.
- **Peer authentication.** Tokens are checked before any `server_reactor` or scheduler exists; token values are never logged.
- **Command injection.** Commands come from the Engine configuration or the local custom checks file only, split by `parse_cmd_line`, never through a shell (`system()`, `popen()`, `cmd.exe /c`, `/bin/sh -c`).
- **Dependency changes.** A new `vcpkg.json` package or version bump not driven by a security fix warrants a CVE check.

**⟶ security findings block in every zone, a reviewer from `@centreon/owners-security` is required, and the robot case for the failure path ships in the same PR.**

ADR: CMA-0009.

---

## CI-enforced vs review-only

CI enforces: the Linux build and `ut_agent` (`.github/scripts/collect-ut.sh`), the Windows build, `ut_agent` and installer test (`monitoring-agent.yml`), the robot suites, secret and dependency scans (`secu-secret-scan.yml`, `secu-dependency-scan.yml`). Gitleaks also runs in the git hooks when they are enabled locally.

Not enforced by CI, so catch them in review:

- **Formatting.** No `clang-format` check runs.
- **Windows compilation from a Linux-only change.** A change in `src/` that builds on Linux can break Windows; check the Windows job before merging.
- **Unregistered test files.** A test missing from `agent/test/CMakeLists.txt` never runs and CI stays green.
- **Configuration parity.** Nothing checks that JSON, registry and installers agree.
- **OTLP name stability.** Only `scheduler_test.cc` (`correct_output_examplar`) and the robot suites would notice a renamed exemplar, and only for the cases they cover.

---

## Tests

| Suite | What | When |
|-------|------|------|
| Unit (`ut_agent`, gtest) | Scheduler with a fake check builder, check logic, native checks on fabricated measures through `compute()`. Shared `g_io_context` on its own thread, `TZ=:Europe/Paris` on Linux | Every behaviour change |
| Robot (`tests/broker-engine/cma*.robot`) | Engine + Broker + agent end to end, Jira key in `[Tags]`, Given/When/Then `[Documentation]` | Protocol, OTLP, transport, security, configuration keys |
| Windows robot (`windows-agent-robot-test.yml`) | Windows agent against a Linux poller | Windows native checks, installer |
| Installer (`installer/scripts_linux/tests/`, `agent_installer_test.ps1`) | Install and upgrade per distribution | Installer or packaging changes |

A bug fix ships with the case that reproduces it. Tests never assert on the real OS values they measure, and never wait on wall-clock time without tolerance. Tests reach checks and the scheduler through `g_io_context` or the scheduler API, never from the gtest thread while the io thread runs.

ADR: CMA-0013.
