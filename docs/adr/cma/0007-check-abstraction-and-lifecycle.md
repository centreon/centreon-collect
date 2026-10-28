# CMA-0007: Derive every check from `check` and complete it asynchronously

- **Status:** Proposed
- **Date:** 2026-10-28

## Context

Every check derives from `com::centreon::agent::check` (`enable_shared_from_this`). The base class owns the normal and retry schedules, the timeout timer, a running index, statistics, the HARD/SOFT state, a reference to its `Service` and a `shared_ptr` to the configuration message that holds that `Service`.

`scheduler::default_check_builder` turns a `Service.command_line` into a check:

1. A command line starting with `encrypt::`, with a decrypt key present, is decrypted; a failure gives a `check_dummy`.
2. A command line that parses as JSON is a native check, chosen by its `"check"` field: `cpu_percentage`, `health`, `custom`, and on Windows `uptime`, `storage`, `memory`, `service`, `counter`, `tasksched`, `files`, `eventlog_nscp`, `process_nscp`. `args.timeout` sets a per-check timeout. An unknown type or a constructor exception gives a `check_dummy` carrying the error.
3. Anything else is a `check_exec`: a plugin process run by `common::process`, with a process timeout of the check timeout + 100 ms so that the check's own timer fires first.

`check_dummy` reports configuration errors as a CRITICAL result. `check_custom` resolves `{"name": ..., "ARGn": ...}` against the local custom checks file (CMA-0010) and runs it as a `check_exec`.

The running index protects against late results: a job captures it when it starts, and `on_completion` ignores a result whose index is no longer current, for instance after a timeout.

Alternatives weighed:

- Drop a service whose configuration cannot be built, and only log it. It lost: the error would be visible in agent logs only, never in Centreon.
- Let checks call back synchronously when the result is immediate. It lost: the scheduler would re-enter its own loop from `start_check`, and the same check could complete before it is counted as started.

## Decision

Derive a check from `check`, or from `native_check_base<N>` (CMA-0008), with the standard constructor arguments `(io_context, logger, first_start_expected, serv, args or command_line, conf, handler, stat)`. Create it with `std::make_shared` or a static `load(...)` factory. Parse and validate its arguments in the constructor and throw on invalid input; the builder turns the exception into a `check_dummy`.

Start `start_check(timeout)` with `if (!_start_check(timeout)) return;`. Capture `_get_running_check_index()` when the job starts, and deliver the result through `check::on_completion(index, status, perfdata, outputs)` asynchronously, error paths included. Report a run-time failure as `unknown`, with an output naming the failed operation; report a configuration error as `critical` through `check_dummy`. Never let an exception escape `start_check`.

Register a new check type in `default_check_builder`, under its platform `#ifdef` when it is platform-specific, and print its `help()` from `--help` in `main.cc` and/or `main_win.cc`.

## Consequences

- The scheduler handles every check type the same way; adding a check touches only the builder.
- A broken service configuration shows in Centreon as a CRITICAL output.
- A check never keeps a pointer to the scheduler, to another check, or to a `Service` of another configuration. Its `Service&` stays valid because it holds `_conf`.
- A check that holds an OS resource across a timeout releases it or makes the late result harmless: `_on_timeout()` is the hook (no override exists today), the running index is the fallback.
- `check_exec` keeps its sequence: command line parsed once at construction, credentials encrypted in memory, decrypted for `start_process` only, then cleared (CMA-0009).
- Not yet followed: `agent-doc.md` § Native checks shows an outdated `check` constructor, and some Windows checks read the running index inside the posted lambda (`check_uptime::start_check`). Harmless today, not copied.

## When to Amend

- A check needs streaming results (several results per start), which the single `on_completion` call cannot express.
- Engine needs to tell a configuration error from a check failure by something other than the CRITICAL/UNKNOWN status.
