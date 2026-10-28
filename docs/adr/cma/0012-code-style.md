# CMA-0012: Follow the collect C++ conventions for names, headers and errors

- **Status:** Proposed
- **Date:** 2026-10-28

## Context

The agent follows the conventions of the rest of centreon-collect, but they are written nowhere. Formatting is defined by the root `.clang-format` (`BasedOnStyle: Chromium`); no hook enforces it (the git hooks run gitleaks only).

What the code shows: 35 headers, all with `#ifndef` guards (mostly `CENTREON_AGENT_<NAME>_HH`), none with `#pragma once`; snake_case types (one CamelCase exception, gRPC's `ImmediateFinishReactor`); private members prefixed `_`; Doxygen `@brief` blocks; errors thrown mostly as `exceptions::msg_fmt` (about 80 of 140 throw sites); `duration` and `time_point` aliases in `check.hh`; `using namespace` limited to `com::centreon[::agent[::<x>_detail]]` and `std::literals` in `.cc` files; a precompiled header `precomp.hh` that no file includes explicitly.

Only conventions specific to this repository belong here. A generic C++ rule (RAII, const-correctness, no raw `new` for ownership) is a Centreon-wide rule, kept in the shared C++ rule set.

Alternatives weighed:

- Adopt the Google or LLVM naming style. It lost: the whole repository uses snake_case and `_` members, and a mixed style is worse than either.
- Reformat the module once with clang-format. It lost for now: it would bury the history of every file under one commit; formatting applies to changed lines.

## Decision

Format every changed `.cc` and `.hh` with the root `.clang-format`, and keep reformatting of untouched code out of functional changes. Start a new file with the Apache 2.0 header of existing files and `Copyright <year> Centreon`, the creation year; do not bump existing years.

Guard headers with `#ifndef CENTREON_AGENT_<PATH_NAME>_HH`. Put everything in `com::centreon::agent`, and details in a nested `<name>_detail` namespace or an anonymous namespace inside the `.cc`. Name types, functions and variables in snake_case, enum types with an `e_` prefix, private and protected members and private methods with a leading `_`. Declare `using pointer = std::shared_ptr<T>;` inside the class, and a static `load(...)` factory when construction must be followed by a start.

Include the matching header first, then agent headers by quoted path, then `com/centreon/common/...`. Keep `using namespace` out of headers. Throw `com::centreon::exceptions::msg_fmt` with a message naming the command or service, and catch `const std::exception&`. Keep every exception inside asynchronous handlers: it becomes a check result or a log line.

Pass durations and time points as `std::chrono` types between agent classes, converting proto integers at the boundary. Use `std::string_view` for read-only strings, abseil or boost containers as already used, `std::map` only when order matters and the set is small. Document public methods with `/** @brief ... */`; write comments that say why, with no ticket number. Leave no commented-out code.

## Consequences

- Ownership and visibility can be read from names alone.
- An exception that reaches `io_context::run()` ends the agent; `main` logs it as `unhandled exception`.
- Without a formatting hook, drift is caught in review only; a `clang-format --dry-run` CI step is the intended follow-up.
- Not yet followed: header guards of `native_check_base.hh` (`CENTREON_AGENT_NATIVE_CHECK_MEMORY_BASE_HH`), `drive_size.hh` (`NATIVE_DRIVE_SIZE_BASE_HH`), `filter_rules.hh` (`FILTER_PARSER_HH`), `check_health.hh` (`HEALTH_CHECK_HH`) and `precomp.hh` (`CA_PRECOMP_HH`). Harmless, not copied.
- Not yet followed: `scheduler::_store_result_in_metrics` is a commented-out body (CMA-0005).

## When to Amend

- The Linux build moves to C++20, which changes what "modern" constructs are allowed in the core.
- A repository-wide formatting pass or a clang-tidy configuration is adopted.
