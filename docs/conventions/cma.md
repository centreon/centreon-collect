# Centreon Monitoring Agent development conventions

Component file of [`global.md`](global.md), which applies first. Architecture: [`../architecture/cma.md`](../architecture/cma.md). Tests: [`../testing.md`](../testing.md#centreon-monitoring-agent).

These rules apply to the Agent and Contracts zones of the architecture file. In `common/`, only the global conventions apply.

## Language and platform

- `src/`, `inc/`, `native_linux/` are **C++17** (built on Linux, and on Windows for the core). C++20 (`std::format`, `<ranges>`, concepts) only in `native_windows/`.
- A Windows API (`windows.h`, `Pdh*`, `Evt*`) or a Linux-only API (`/proc`, `localtime_r`) only in a platform tree or an `#ifdef _WIN32` block. Do not extend the `#ifdef _WIN32` blocks of `src/grpc_ca_bootstrap.cc`: a new platform branch there moves to a per-platform file.
- List every new source in `SRC_COMMON`, `SRC_LINUX` or `SRC_WINDOWS` of `agent/CMakeLists.txt`, and every new test in `agent/test/CMakeLists.txt`. A new template base lives in its header or in a `.cc` included by the file that instantiates it.
- New third-party headers go to `precomp_inc/precomp.hh`. Keep the static runtime link on Linux.

## Naming and files

- Header guard: `#ifndef CENTREON_AGENT_<PATH_NAME>_HH`.
- Namespace `com::centreon::agent`; details in a nested `<name>_detail` namespace or an anonymous namespace in the `.cc`.
- Enum types prefixed `e_`.
- Declare `using pointer = std::shared_ptr<T>;` in the class, and a static `load(...)` factory when construction must be followed by a start.
- Include order: matching header, agent headers by quoted path, then `com/centreon/common/...`.
- Durations and time points cross agent classes as `std::chrono` types (aliases in `check.hh`), converted from proto integers at the boundary.
- Read-only strings as `std::string_view`; abseil or boost containers as already used; `std::map` only when order matters and the set is small.

## Threading and asynchrony

- Touch the scheduler, the checks and their timers from the `io_context` thread only. From any other thread, hand over with `asio::post(*_io_context, ...)` before touching agent state.
- Never block the `io_context` thread once `run()` has started: no `sleep_for` / `Sleep()`, no synchronous gRPC call, no `boost::process::wait`, no file-system call that can hang, no `future::get()`, no wait on a mutex held by a slow thread.
- A blocking OS call runs in a worker built like `drive_size_thread`: one static worker, a request queue under `absl::Mutex`, results posted back, `thread_kill()` called after `run()` returns. Create a thread only as such a worker. More parallelism comes from `max_concurrent_checks`, never from a second thread running `run()`.
- A mutex only at a thread boundary: gRPC reactors, worker queues, OS callbacks.
- Every lambda handed to an asynchronous operation captures `shared_from_this()` (or a `weak_ptr` locked at call time); `this` only next to it. A handler receiving an error code returns at once, and checks the owner's `_alive` flag when it has one.
- Flush logs with the asio timer of `init_log_flush_timer`. `spdlog::flush_every` is banned: its thread holds a spdlog mutex that a forked child inherits and can hang on.

## Transport

- Add a feature to both connection modes in the same change.
- Send `MessageFromAgent.init` (`AgentInfo`, from `fill_agent_info`) first on every stream. Start each scheduler with `scheduler::default_config()`; run checks only once the first `AgentConfiguration` arrives.
- Register every reactor with `register_stream()` before `StartCall` or before handing it to gRPC; release it only in the posted `OnDone` lambda. Never delete a reactor from a gRPC callback.
- Write through `bireactor::write()` only. State shared with gRPC threads (write queue, pending read, `_alive`) is accessed under `_protect`. Incoming requests are posted to the `io_context`, never passed to the scheduler from the gRPC thread.
- Channels, servers, TLS and keepalive through `common::grpc::grpc_client_base` / `grpc_server_base`. End a stream with `shutdown()`.

## Protocol and OTLP

`agent.proto`:

- Add fields with new numbers only. Never renumber, retype or reuse a field number; declare a removed field `reserved` (number and name).
- A new kind of message is a new member of the existing `oneof`, never a new RPC.
- A new field's zero value means the previous behaviour; make it `optional` when absent and zero differ. Comment it with its unit and default.
- A capability Engine must know before sending something new is announced in `AgentInfo` (pattern: `encryption_ready`). A new `AgentInfo` field is filled on both platforms (`native_linux/src/agent_info.cc`, `native_windows/src/agent_info.cc`), with OpenTelemetry semantic-convention names where one exists (`os.type`, `host.arch`).
- The Engine side (`engine/modules/opentelemetry/src/centreon_agent/`) changes in the same pull request.

OTLP encoding:

- Never rename the metric `status`, the attributes `host.name` / `service.name`, the exemplar labels or the perfdata type keys. New data is a new exemplar label or attribute, with the Engine side in the same change.
- At most one result per service in a request: flush before adding a second one.
- Emit an exemplar only when its value is set; keep the `<= numeric_limits<double>::max()` test that skips `NaN`.
- Pass every string placed in a message through `common::check_string_utf8`: protobuf rejects invalid UTF-8 and the whole request is lost.
- Keep the early-flush threshold below 4 MiB. Schedule exports on absolute time points.

## Scheduler

- Keep: no drift (next start from absolute time points), first starts spread over the smallest interval, one check per slot with collisions moved forward, `max_concurrent_checks` never exceeded, no result of a previous configuration stored or re-queued.
- Rebuild every check on each configuration; apply zero-value defaults in `update()`. The `check` constructor repeats the 300 s and 60 s defaults for tests: a changed default changes both.
- Re-queue a check only on completion (`_check_handler`) or on a timeperiod deferral; remove it from the queue before it starts.
- Evaluate timeperiods with the `common/` functions shared with Engine, never a local reimplementation.
- Do per-check expensive work (decrypt, parse) when building checks, not when starting them.

## Checks

- Derive from `check`, `native_check_base<N>` or `native_check_cpu<N>`, with the standard constructor arguments `(io_context, logger, first_start_expected, serv, args or command_line, conf, handler, stat)`. Create with `std::make_shared` or a static `load(...)`.
- Parse and validate arguments in the constructor and throw on invalid input; the builder turns the exception into a `check_dummy`.
- `start_check(timeout)` starts with `if (!_start_check(timeout)) return;`. Capture `_get_running_check_index()` when the job starts, and deliver the result through `on_completion(index, status, perfdata, outputs)` asynchronously, error paths included. Never let an exception escape `start_check`.
- A run-time failure is `unknown`, with an output naming the failed operation; a configuration error is `critical` through `check_dummy`.
- A check never keeps a pointer to the scheduler, to another check, or to a `Service` of another configuration. A check holding an OS resource across a timeout releases it in `_on_timeout()` or makes the late result harmless.
- Register a new check type in `default_check_builder` (under its platform `#ifdef` when platform-specific) and print its `help()` from `--help` in `main.cc` and/or `main_win.cc`.

## Native checks

- Arguments: read the JSON `args` with `rapidjson_helper`; accept numbers as JSON numbers or strings; give every argument a documented default; throw on an invalid value. Leave `timeout` to the builder.
- Argument names in kebab-case: `warning-<x>` / `critical-<x>`, `filter-<x>`, `<x>-syntax`. NSClient++-compatible checks keep the NSClient++ spellings.
- Thresholds with `common::threshold` only, never `std::stod`, a regex or string comparisons; reported with `set_pref_details_w` / `set_pref_details_c`.
- Status: CRITICAL over WARNING over OK; UNKNOWN when the indicator cannot be measured. The output starts with the status label (`check::status_label`), details on the following lines.
- New perfdata: `<object>.<measure>.<unit>`, lower case, ASCII, dot separators, instance prefixed with `#` (`<n>#core.cpu.utilization.percentage`); base units (`B`, `s`, `%`) and unscaled values, never `KB` or `MB`. A unit without base form uses a self-describing token (`exit_code`).
- Never rename an existing perfdata label or argument: installed packs depend on them. Tell the pack owners (CTOR) about a new one in the pull request.
- Use the Windows APIs already used in `native_windows` (`Pdh*`, `ntdll`, `Evt*`, Task Scheduler COM) before adding another; convert wide strings through `windows_util` (`lpwcstr_to_acp`).
- Every native check has a `static void help(std::ostream&)` (each argument, its default, a JSON example), a section in `agent/doc/agent-doc.md`, and a test (see the testing file).
- Filter grammars: boost::parser in `filter_rules.cc`, explicit whitespace tokens. A filter without a checker is disabled, not false.

## Local configuration

- A new local key is added, in the same change, to: the JSON schema and parser (`config.cc`), the registry reader (`config_win.cc`), the getter in `config.hh`, the Linux installer, the Windows installer and silent-install script, the default template when its value is not the default, and `agent-doc.md`. Same snake_case key everywhere.
- Every key has a default that keeps the previous behaviour; its unit is in its name or description. `endpoint` stays the only required key.
- JSON aliases are accepted forever and mutually exclusive by schema; a renamed key keeps the old name as an alias. The registry uses canonical names only.
- Start-up stops with a message naming the key when validation fails; what the schema cannot express is checked in code (as for `endpoint`).
- `config::instance()` is read-only after start-up. Custom checks reload all or nothing: parse into a temporary map, replace on success only, log each loaded entry at INFO. The constructor `config(bool)` is for tests only.

## Logging

- `SPDLOG_LOGGER_<LEVEL>(_logger, ...)` with the injected logger. Global `SPDLOG_<LEVEL>` only before `g_logger` exists; `std::cout` only for `--help`. Protobuf messages formatted with `common/fmt_protobuf.hh`.

| Level | Use |
|---|---|
| CRITICAL | The agent cannot continue, or trust material changed (`[SECURITY]`) |
| ERROR | A result or a connection is lost |
| WARN | Degraded but working |
| INFO | Lifecycle events, a few per configuration |
| DEBUG | Per check or per message events |
| TRACE | Payloads, full command lines |

- Nothing emitted per check or per message above DEBUG: a per-check INFO line floods hosts running hundreds of checks. Guard expensive TRACE formatting with `if (_logger->level() <= spdlog::level::trace)`.
- Name the subject: service description (with host and service ids when known), command name, peer. Pointer values (`{:p}`) for lifecycle traces. Keep `SPDLOG_ACTIVE_LEVEL` at TRACE.
- A log message is a support interface: before changing a wording, check that no `cma*.robot` test greps it.

## Security

- Keep the plain-text warning and the one-hour stop, with no way to disable them.
- TLS through `common::grpc::grpc_config` only. `insecure` stays an explicit configuration choice. `TLS_SKIP_VERIFY_CA` only in `grpc_ca_bootstrap.cc`, and the fetched CA is discarded unless its fingerprint matches. Never fall back from `full` to `insecure`.
- Plain-text credentials exist only for the duration of `start_process`: every `decrypt_args` is followed by `clear_unencrypted_args`, on every path including exceptions. Key material only in `common::crypto::aes256` objects.
- Never log or output a decrypted command line, a token, a key, a salt or a private key. `get_command_line()` is plain text for a non-encrypted command: never above TRACE, never in an output.
- Check tokens before creating any `server_reactor` or scheduler.
- Commands come only from the Engine configuration or the local custom checks file, split by `parse_cmd_line`, never through a shell (`system()`, `popen()`, `cmd.exe /c`, `/bin/sh -c`).
- Log trust changes at CRITICAL with `[SECURITY]`.
- A new `vcpkg.json` package or version bump not driven by a security fix gets a CVE check.

## Errors and comments

- Errors: `msg_fmt` naming the command or service (global rule). An exception that can reach `io_context::run()` ends the agent (`main` logs it as `unhandled exception`): keep every exception inside the handler.
- Do not bump the copyright year of existing files.

## Known deviations — do not copy

The code below predates these conventions. Do not use it as a model, and fix it only in a dedicated pull request.

| Place | Deviation |
|---|---|
| `check.cc`, `check_exec.cc`, `check_dummy`; `scheduler::_check_handler` (DEBUG), `check_exec::start_check` (ERROR) | Timeout and failure outputs and logs embed `get_command_line()`: for a non-encrypted command, plugin arguments (possibly passwords) reach Engine, the UI and the logs. The fix outputs the command name only |
| `scheduler::_add_metric_to_scope` | Picks `warn_le`/`warn_lt` with `perf.critical_mode()` instead of `perf.warning_mode()` |
| `scheduler::_store_result_in_metrics` | Empty stub (the `use_exemplar == false` path) with a commented-out body; to be removed, not extended |
| `agent.proto` `AgentConfiguration` | Field 2 removed in a comment only, field 8 is a placeholder `unused`: the next change replaces both with `reserved 2, 8;` |
| `agent.proto` `Service`, `AgentInfo` | Most fields have no unit or default comment; add one when touching a field |
| `config::read_custom_checks`; one error path of `main.cc` / `main_win.cc` | Global `SPDLOG_*` macros after `g_logger` exists |
| `conf/centagent.json.in` | `"encryption": false` while the schema expects `full`, `insecure` or `no` |
| `config.cc` schema | `endpoint` pattern differs from the code regex (the code is the reference); `"min"` used on `log_max_file_size`, `log_max_files`, `second_max_reconnect_backoff` is not a JSON-schema keyword |
| `native_check_base.hh`, `drive_size.hh`, `filter_rules.hh`, `check_health.hh`, `precomp.hh` | Header guards outside the `CENTREON_AGENT_` pattern |
| `bireactor` | Virtual method `on_incomming_request` misspelled |
| `scheduler::check_host_timezone` | Typos in messages (`retreive`, `form api`) |
| `check_uptime::start_check` and some Windows checks | Read the running index inside the posted lambda instead of at start |
| `agent/doc/agent-doc.md` § Native checks | Shows an outdated `check` constructor |
| Perfdata `uptime`, `interval`, `runtime`, `ok_count`, `critical-count`, `process.count`, `used_<mount point>` | Outside the naming rule; kept for pack compatibility, never renamed |
| `src/drive_size.cc` | In `SRC_COMMON` although the storage check is registered for Windows only |
