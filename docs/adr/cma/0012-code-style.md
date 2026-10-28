# CMA-0012 — C++ code style

- Status: Proposed
- Owner: @centreon/owners-cpp
- Scope: `agent/**/*.cc`, `agent/**/*.hh`

## Context

The agent follows the conventions of the rest of centreon-collect. Formatting is defined by the root `.clang-format` (`BasedOnStyle: Chromium`); no hook enforces it (the git hooks run gitleaks only). Facts observed in `agent/`: 35 headers, all with `#ifndef` guards (mostly `CENTREON_AGENT_<NAME>_HH`), none with `#pragma once`; snake_case types (one CamelCase exception, gRPC's `ImmediateFinishReactor`); private members prefixed `_`; Doxygen `@brief` blocks; errors thrown mostly as `exceptions::msg_fmt` (about 80 of 140 throw sites).

These are the conventions specific to this repository. A generic C++ rule (RAII, const-correctness, no raw `new` for ownership) is a Centreon-wide rule and belongs in the shared C++ rule set, not here.

## Decision

1. Formatting: `clang-format` with the root `.clang-format` on every changed `.cc` / `.hh`. A PR mixing reformatting of untouched code with a functional change is rejected.
2. File header: the Apache 2.0 license block of existing files, `Copyright <year> Centreon` with the creation year for new files. Existing years are not bumped.
3. Header guards: `#ifndef CENTREON_AGENT_<PATH_NAME>_HH` / `#define` / `#endif`. No `#pragma once`.
4. Namespace: everything in `com::centreon::agent`; implementation details in a nested `<name>_detail` namespace (`native_check_detail`, `check_drive_size_detail`) or in an anonymous namespace / `static` function inside the `.cc`.
5. Naming:
   - types, functions, variables: `snake_case`; enum values `snake_case` (`e_status::ok`); enum types prefixed `e_`;
   - private and protected data members and private methods: leading `_` (`_logger`, `_start_check`);
   - shared pointer alias inside the class: `using pointer = std::shared_ptr<T>;`
   - factory: `static std::shared_ptr<T> load(...)` when construction must be followed by a start (`scheduler::load`, `streaming_client::load`).
6. Includes: precompiled header first by build (`precomp.hh`, do not include it explicitly), then the matching header, then agent headers by quoted path, then `com/centreon/common/...`. No `using namespace` in headers; in `.cc` files, `using namespace` is limited to `com::centreon[::agent[::<x>_detail]]` and `std::literals`.
7. Errors: throw `com::centreon::exceptions::msg_fmt` with a message naming the command or service; catch `const std::exception&`. Async paths turn exceptions into a check result or a log line; nothing propagates to `io_context::run()` (an escaping exception ends the agent, `main` logs it as `unhandled exception`).
8. Types: `std::chrono` types for durations and time points (`duration`, `time_point` aliases of `check.hh`), never raw integer seconds inside the agent (proto integers are converted at the boundary). `std::string_view` for read-only string parameters; `""sv` literals for string comparisons.
9. Containers: abseil (`flat_hash_map`, `btree_map`) or boost (`flat_map`, `multi_index`) as already used; `std::map` only when order matters and the set is small.
10. Comments: Doxygen `/** @brief ... */` on every public method and every non-obvious private one; a comment says why, not what. No ticket number (MON-xxxx) in code comments: it belongs to the commit message.
11. Dead code: no commented-out blocks in new code.

## Consequences

- A reader can tell ownership and visibility from names alone.
- Without a formatting hook, formatting drift is caught only in review; adding a `clang-format --dry-run` CI step is the intended follow-up.

## Guardrails

- `#pragma once`, CamelCase type, member without `_` prefix, `using namespace` in a header.
- Raw integer durations (`unsigned timeout_s`) passed between agent classes.
- New commented-out code, ticket reference in a comment.
- Exception allowed to escape an asio handler.
- Unrelated reformatting in a functional diff.

## Known deviations

- `scheduler::_store_result_in_metrics` is a commented-out body (CMA-0005).
- Guards not following the pattern: `native_check_base.hh` (`CENTREON_AGENT_NATIVE_CHECK_MEMORY_BASE_HH`), `drive_size.hh` (`NATIVE_DRIVE_SIZE_BASE_HH`), `filter_rules.hh` (`FILTER_PARSER_HH`), `check_health.hh` (`HEALTH_CHECK_HH`), `precomp.hh` (`CA_PRECOMP_HH`). Harmless; do not copy.

## References

- `.clang-format`, `.githooks/`
- `agent/inc/com/centreon/agent/check.hh` (aliases, Doxygen style)
- `agent/precomp_inc/precomp.hh`
