# CMA-0008 — Native check conventions

- Status: Proposed
- Owner: @centreon/owners-cpp
- Scope: `agent/native_linux/**`, `agent/native_windows/**`, `agent/src/native_check_*`, `agent/src/check_health.cc`, `agent/src/drive_size.cc`, `agent/{inc,src}/filter*`

## Context

Native checks replace external plugins for the most common OS indicators. The Centreon Monitoring Connectors (packs `OS-Linux-Centreon-Monitoring-Agent`, `OS-Windows-Centreon-Monitoring-Agent`) build their command lines as JSON (`{"check": "<type>", "args": {...}}`), and graphs and thresholds in the UI rely on metric names. Several Windows checks (`eventlog_nscp`, `process_nscp`, `files`, `tasksched`, `counter`) reproduce NSClient++ options (`filter-*`, `warning-rules`, `*-syntax`, `verbose`) so that NSClient++ users can migrate.

Building blocks:

| Block | Use |
| --- | --- |
| `native_check_base<N>` + `snapshot<N>` + `measure_to_status<N>` + `metric_definition` | checks that take one snapshot of N counters and compare ratios to thresholds (memory, service) |
| `native_check_cpu<N>` (file `native_check_cpu_base.hh`) | checks that need two snapshots over time (cpu, Linux and Windows) |
| `common::threshold` | Nagios range parsing (`extract_range`), unit multiplier, `is_triggered`, `set_pref_details_w/_c` to put thresholds in perfdata |
| `common::rapidjson_helper` | typed argument access with defaults (`get_string`, `get_bool`, `get_string_or_int_as_string`...) |
| `filter` / `filter_rules` (boost::parser) | NSClient++-like filter expressions, two steps: parse to a tree, then attach checkers (see `agent-doc.md` § About filters) |

## Decision

1. Arguments
   - MUST be read from the JSON `args` object with `rapidjson_helper`; a missing argument takes a documented default; an invalid one throws in the constructor.
   - Names are kebab-case, `warning-<x>` / `critical-<x>` for thresholds, `filter-<x>` for filters, `<x>-syntax` for output templates. NSClient++-compatible checks keep NSClient++ spellings. Existing snake-case names (`use_english`) stay; new ones are kebab-case.
   - Numeric values MUST be accepted both as JSON numbers and as strings (packs send strings): use `get_string_or_int_as_string`, `check::get_double`, `check::get_bool`.
   - `timeout` is reserved for the per-check timeout handled by the builder (CMA-0007).
2. Thresholds MUST use `common::threshold` (Nagios range syntax `[@]start:end`, empty = disabled) and MUST be reported in perfdata with `set_pref_details_w` / `set_pref_details_c`. No hand-written threshold parsing.
3. Status precedence: CRITICAL over WARNING over OK; an indicator that cannot be measured gives UNKNOWN.
4. Output MUST start with the status label (`OK: `, `WARNING: `, `CRITICAL: `, `UNKNOWN: `) followed by a human summary; details go to following lines. Use `check::status_label`.
5. Perfdata names are a public contract with the packs:
   - new labels use the format `<object>.<measure>.<unit>` in lower case, dots as separators (`memory.usage.bytes`, `swap.free.bytes`, `cpu.utilization.percentage`); per-instance metrics prefix the instance with `#` (`<instance>#core.cpu.utilization.percentage`) as `native_check_cpu` does; existing labels outside this format stay as they are (see Known deviations);
   - units are base units (`B`, `s`, `%`), or a self-describing token when none applies (`exit_code`); values are not pre-scaled;
   - an existing name MUST NOT change; a new name needs the pack owner (CTOR squad) informed in the PR.
6. A Windows check uses the OS APIs already used in `native_windows` (`Pdh*`, `ntdll`, `Evt*`, Task Scheduler COM) before adding another one; a new OS dependency is declared in `target_link_libraries`. Wide strings from Windows APIs are converted with the existing `windows_util` helpers (`lpwcstr_to_acp`), not with ad-hoc code; the output line is then sanitised to UTF-8 by the scheduler (CMA-0005). New perfdata labels MUST be ASCII.
7. A blocking OS call (file system walk, remote drive, event log query) MUST run in a worker thread (CMA-0002), never in `start_check`.
8. Every native check MUST provide `static void help(std::ostream&)` listing each argument, its default, and a JSON example; `--help` prints it (CMA-0007 rule 7).
9. Filter grammars MUST be written with boost::parser rules in `filter_rules.cc` (shared with `ut_agent`), keep explicit whitespace tokens (`bp::ws`), and come with parsing tests in `filter_test.cc`. A filter without a checker is disabled, not false.
10. Every new native check MUST have a unit test file named `test/check_<name>_test.cc` (`check_windows_<name>_test.cc` / `check_linux_<name>_test.cc` when the check exists on both platforms with different code) that covers argument parsing, each status, output and perfdata, using injected measures (`compute(...)` on a fake snapshot) rather than the real OS.

## Consequences

- A pack can target Linux and Windows with the same option vocabulary.
- Metric names cannot be "fixed" for style; renaming needs a pack migration.

## Guardrails

- A threshold parsed with `std::stod`, regex or string compare instead of `common::threshold`.
- A new perfdata label with spaces, upper case, or a unit pre-applied (`KB`, `MB`).
- A renamed perfdata label or argument.
- A native check without `help()`, without a test, or not in `agent-doc.md`.
- `std::wstring` or `wchar_t*` reaching `outputs` or perfdata without conversion through `windows_util`.

## Known deviations

- Existing labels outside rule 5 format, kept for pack compatibility: `uptime`, `interval`, `runtime`, `ok_count`, `critical-count`, `process.count`, `used_<mount point>` (Windows mount points contain `:` and `\`).
- Test files without the `_test` suffix (`check_windows_counter.cc`, `check_windows_sched.cc`) or without platform tag although Windows-only (`check_uptime_test.cc`, `check_process_test.cc`). Rename only in a dedicated PR.

## References

- `agent/inc/com/centreon/agent/native_check_base.hh`, `agent/src/native_check_base.cc`
- `agent/inc/com/centreon/agent/native_check_cpu_base.hh`, `agent/src/native_check_cpu_base.cc`
- `agent/native_windows/src/check_memory.cc` (metric definitions), `check_uptime.cc` (simple pattern)
- `agent/inc/com/centreon/agent/filter.hh`, `agent/src/filter.cc`, `agent/src/filter_rules.cc`
- `common/inc/com/centreon/common/threshold.hh`, `common/inc/com/centreon/common/perfdata.hh`
- `agent/doc/agent-doc.md` § Native checks
