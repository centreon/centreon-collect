# CMA-0008: Write native checks with shared argument, threshold and perfdata conventions

- **Status:** Proposed
- **Date:** 2026-10-28

## Context

Native checks replace external plugins for the most common OS indicators. The Monitoring Connectors (packs `OS-Linux-Centreon-Monitoring-Agent`, `OS-Windows-Centreon-Monitoring-Agent`) build their command lines as JSON, `{"check": "<type>", "args": {...}}`, and graphs and thresholds in the UI rely on the metric names. Several Windows checks (`eventlog_nscp`, `process_nscp`, `files`, `tasksched`, `counter`) reproduce NSClient++ options (`filter-*`, `warning-rules`, `*-syntax`, `verbose`) so that NSClient++ users can migrate.

Building blocks in use:

- `native_check_base<N>` with `snapshot<N>`, `measure_to_status<N>` and `metric_definition`: one snapshot of N counters compared to thresholds (memory, service);
- `native_check_cpu<N>` (file `native_check_cpu_base.hh`): two snapshots over time (cpu, Linux and Windows);
- `common::threshold`: Nagios range parsing (`extract_range`), unit multiplier, `is_triggered`, `set_pref_details_w` / `set_pref_details_c`;
- `common::rapidjson_helper`: typed arguments with defaults (`get_string`, `get_bool`, `get_string_or_int_as_string`);
- `filter` and `filter_rules.cc` (boost::parser): NSClient++-like expressions, parsed into a tree, then given checkers (`agent-doc.md` § About filters).

Existing perfdata names follow `<object>.<measure>.<unit>` (`memory.usage.bytes`, `swap.free.bytes`, `cpu.utilization.percentage`, per core `<n>#core.cpu.utilization.percentage`), with older exceptions. Wide strings from Windows APIs go through `windows_util` (`lpwcstr_to_acp`).

Alternatives weighed:

- Let each check define its own argument and threshold syntax. It lost: packs target Linux and Windows with the same option vocabulary, and Nagios ranges are what pack authors and users already write.
- Rename existing metrics to one clean scheme. It lost: renaming breaks graphs and thresholds of every installed pack.

## Decision

Read arguments from the JSON `args` object with `rapidjson_helper`, accept numbers both as JSON numbers and as strings, give every argument a documented default, and throw on an invalid value. Name new arguments in kebab-case: `warning-<x>` / `critical-<x>` for thresholds, `filter-<x>` for filters, `<x>-syntax` for output templates; keep NSClient++ spellings in NSClient++-compatible checks. Leave `timeout` to the builder (CMA-0007).

Parse thresholds with `common::threshold`, never with `std::stod`, a regex or string comparisons, and report them with `set_pref_details_w` / `set_pref_details_c`. Rank CRITICAL over WARNING over OK, and return UNKNOWN for an indicator that cannot be measured. Start the output with the status label (`check::status_label`), details on the following lines.

Name new perfdata `<object>.<measure>.<unit>`, lower case, ASCII, dots as separators, instance prefixed with `#`; use base units (`B`, `s`, `%`) and unscaled values, never `KB` or `MB`. Never rename an existing perfdata label or argument, and tell the pack owners (CTOR) about a new one in the pull request.

Run blocking OS calls in a worker thread (CMA-0002). Use the Windows APIs already in `native_windows` (`Pdh*`, `ntdll`, `Evt*`, Task Scheduler COM) before adding another, and convert strings through `windows_util`. Give every native check a `static void help(std::ostream&)` with each argument, its default and a JSON example, a section in `agent-doc.md`, and a test file `test/check_<name>_test.cc` (`check_linux_` / `check_windows_` when both platforms have their own code) that feeds fabricated measures to `compute(...)`. Write filter grammars with boost::parser in `filter_rules.cc`, with explicit whitespace tokens and tests in `filter_test.cc`.

## Consequences

- One option vocabulary across platforms and checks; a pack author can predict argument names.
- Metric names cannot be fixed for style; a rename needs a pack migration.
- A filter without a checker is disabled, not false.
- A unit without base form uses a self-describing token (`exit_code`).
- Existing labels outside the naming rule stay for pack compatibility: `uptime`, `interval`, `runtime`, `ok_count`, `critical-count`, `process.count`, `used_<mount point>` (Windows mount points contain `:` and `\`).
- Existing test files outside the naming rule (`check_windows_counter.cc`, `check_windows_sched.cc`, Windows-only `check_uptime_test.cc`, `check_process_test.cc`) are renamed in a dedicated change only.

## When to Amend

- The packs adopt OpenTelemetry semantic-convention metric names, which would justify a coordinated rename.
- NSClient++ compatibility is no longer a migration goal.
