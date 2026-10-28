# CMA-0007 — Check abstraction and lifecycle

- Status: Proposed
- Owner: @centreon/owners-cpp
- Scope: `agent/{inc,src}/check.*`, `check_exec.*`, `scheduler::default_check_builder`

## Context

Every check derives from `com::centreon::agent::check` (`enable_shared_from_this`). The base class owns: schedule (`time_step` normal/retry), timeout timer, running index, statistics, HARD/SOFT state, a reference to the `Service` and a `shared_ptr` to the configuration message that holds it.

`scheduler::default_check_builder` turns a `Service.command_line` into a check:

1. `encrypt::` prefix and a decrypt key present → decrypt the command line; failure → `check_dummy`.
2. Command line parses as JSON → native check, dispatched on `"check"` (`cpu_percentage`, `health`, `custom`, and on Windows `uptime`, `storage`, `memory`, `service`, `counter`, `tasksched`, `files`, `eventlog_nscp`, `process_nscp`); `args.timeout` sets a per-check timeout. Unknown type or constructor exception → `check_dummy` carrying the error.
3. Otherwise → `check_exec`, a plugin process run by `common::process`.

`check_dummy` reports configuration errors as a CRITICAL result instead of silently dropping the service. `check_custom` resolves `{"name": ..., "ARGn": ...}` against the local custom checks file (CMA-0010) and runs it as `check_exec`.

## Decision

1. A check class MUST:
   - derive from `check` (or `native_check_base<N>`, CMA-0008) and take the standard constructor arguments `(io_context, logger, first_start_expected, serv, [args|command_line], conf, handler, stat)`;
   - be created through `std::make_shared` or a static `load(...)` factory, never on the stack nor with `new`;
   - parse and validate its arguments in the constructor and throw (`exceptions::msg_fmt` / `std::runtime_error`) on invalid input; the builder turns the exception into a `check_dummy`.
2. `start_check(timeout)` MUST begin with `if (!_start_check(timeout)) return;`. That call arms the timeout (custom timeout first), detects re-entrance, records statistics.
3. Completion MUST go through `check::on_completion(running_index, status, perfdata, outputs)` with the index read by `_get_running_check_index()` when the job started, and MUST be asynchronous (`asio::post`, timer, process callback), including on error paths. A late completion with an old index is ignored by design: it is the timeout protection.
4. Status values are Nagios codes, `e_status` (`ok`=0, `warning`=1, `critical`=2, `unknown`=3). Errors at run time (exception, OS failure) MUST complete with `unknown` and an output that names the failed operation. `check_dummy` (configuration error) uses `critical`.
5. A check that holds an OS resource across a timeout (process, handle, worker request) MUST release it or make its late result harmless: override `_on_timeout()` (hook present, no override today) or rely on the running-index check of rule 3 and free the resource in the late callback.
6. A check MUST NOT keep a pointer to the scheduler, to another check, or to a `Service` from another configuration. Its `Service&` is valid because the check holds `_conf`.
7. A new native check type MUST be registered in `default_check_builder` under its platform `#ifdef` when platform-specific, with its `help()` printed by `main.cc` and/or `main_win.cc` `--help`.
8. `check_exec` MUST keep: command line parsed once at construction; credentials encrypted in memory and decrypted only for `start_process`, then cleared (CMA-0009); process timeout = check timeout + 100 ms, so the check's own timer fires first.

## Consequences

- The scheduler handles every check type the same way; adding a check never touches scheduling code beyond the builder.
- A broken service configuration is visible in Centreon as a CRITICAL output, not hidden in agent logs only.

## Guardrails

- `start_check` without `_start_check` first, or calling `on_completion` before `start_check` returns.
- `on_completion` called without the captured running index (e.g. `_get_running_check_index()` read inside the completion lambda instead of at start).
- An exception escaping `start_check`.
- A new `check` subclass with a public constructor used with `new`, or without `shared_from_this` captures in async lambdas.
- A new `"check"` keyword not added to `--help`, `agent/doc/agent-doc.md` and a test.

## Known deviations

- `agent/doc/agent-doc.md` § Native checks shows an outdated `check` constructor (string service name, command name). The code is the reference; the doc example must be refreshed.
- Some Windows checks capture `_get_running_check_index()` inside the posted lambda (`check_uptime::start_check`); harmless because the lambda runs before any other start, but new code captures it outside.

## References

- `agent/inc/com/centreon/agent/check.hh`, `agent/src/check.cc`
- `agent/inc/com/centreon/agent/check_exec.hh`, `agent/src/check_exec.cc`
- `agent/src/scheduler.cc` (`default_check_builder`)
- `agent/test/check_test.cc`, `agent/test/check_exec_test.cc`
