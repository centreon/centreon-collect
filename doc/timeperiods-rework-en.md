# Timeperiods — extraction into a shared library and implementation choice

## Table of contents

  - [1. Context and goal](#1-context-and-goal)
  - [2. The timeperiod concept](#2-the-timeperiod-concept)
  - [3. State of the current code](#3-state-of-the-current-code)
  - [4. Coupling points with engine](#4-coupling-points-with-engine)
  - [5. Comparison of implementation options](#5-comparison-of-implementation-options)
  - [6. Per-option summary](#6-per-option-summary)
  - [7. Recommendation](#7-recommendation)
  - [8. Extraction plan](#8-extraction-plan)
  - [9. Open leads](#9-open-leads)

This document analyses extracting engine's *timeperiods* code into a shared
library reusable by broker (notably for evaluating notification time windows on
the Broker side, cf. branch `MON-187019-broker-notification`), and compares three
implementation options should they be rebuilt from scratch: the current code,
Boost.Date_Time and Abseil.

## 1. Context and goal

Timeperiod computation (monitoring and notification time windows) is currently
confined to engine. We want to make it **independent of engine** so it can also be
used in broker.

Work started by moving files into `engine/src/timeperiods/` with a
`CMakeLists.txt` producing a static `timeperiods` library:

```
engine/src/timeperiods/
  ├── CMakeLists.txt        # add_library(timeperiods STATIC ...)
  ├── timeperiod.{cc,hh}
  ├── daterange.{cc,hh}
  ├── timerange.{cc,hh}
  └── timeperiod_types.hh
```

The headers were moved out of `engine/inc/com/centreon/engine/`; consumer
includes now use the `engine/src/timeperiods/...` path.

## 2. The timeperiod concept

**Key point: a timeperiod is not an interval, it is a recurrence engine.** It
describes a repeating pattern:

- per-weekday ranges (`monday 09:00-17:00`);
- calendar dates, month dates, **month-week-day** ("the 3rd Monday of each
  month");
- exceptions (dateranges) with a **skip interval** ("every N days");
- **exclusions** (one timeperiod excluding another);
- DST handling;
- and above all the core operation `get_next_valid_time` /
  `check_time_against_period` ("from this instant, what is the next covered
  moment").

The conceptual analogue is the iCalendar `RRULE`, **not** a date-library interval
type. This is fundamental: **no** date library provides this concept. Whatever the
choice, the recurrence engine stays hand-written code; what changes is the quality
of the underlying calendar/timezone primitives.

## 3. State of the current code

- ~1600 LOC total (`timeperiod.cc` ~1219, `daterange.cc` ~314,
  `timerange.cc` ~79), Nagios heritage.
- Implementation entirely built on `struct tm` + `mktime` + `localtime_r`, with
  hand-rolled calendar arithmetic (year/month/weekday).
- DST handled manually via the `tm_isdst = -1` pattern then re-normalisation after
  `mktime` (cf. comments "There was a DST shift in between"). The tests
  `engine/tests/timeperiod/get_next_valid_time/dst_forward.cc` and
  `dst_backward.cc` exist precisely because this is delicate.
- **Timezone is handled via process-global state**:
  `engine/src/timezone_manager.cc` does `setenv("TZ", …)` then `tzset()` around
  the computations. This is the Nagios-inherited approach — a global side effect,
  **thread-unsafe**.

## 4. Coupling points with engine

For the library to be genuinely reusable in broker, the following must be broken:

1. **The global static registry** `static timeperiod_map timeperiods;` owned by
   the class. Broker cannot share engine's global map; exclusion resolution
   (`resolve()` → `timeperiod::timeperiods.find(...)`) must receive the collection
   as a parameter rather than read it statically.
2. **`config_logger`** (via `globals.hh`) — engine global logger; replace with an
   injected `std::shared_ptr<spdlog::logger>`.
3. **`engine_error`** (via `exceptions/error.hh`) — replace with
   `com::centreon::exceptions::msg_fmt` (already in `common/`).
4. **`contains_illegal_object_chars`** (via `shared.hh`) — small util to move into
   the library or into `common/`.
5. **`daterange` / `timerange`** are part of the same cluster and must lose their
   dependency on `engine/common.hh` (only `DATERANGE_TYPES`, `time_t` and a few
   constants are actually needed).
6. Dead includes to remove: `broker.hh` and the `using namespace ...::applier;` in
   `timeperiod.cc` (no symbol used).

Eventually the library should live under `common/` (not `engine/src/`) to avoid an
inverted broker → engine dependency, and change namespace
(`com::centreon::common::timeperiods` rather than `com::centreon::engine`).

## 5. Comparison of implementation options

Reminder: all three options require writing the recurrence engine yourself. The
difference is in the calendar and timezone primitives.

| Criterion | Current code (`struct tm`/`mktime`) | Boost.Date_Time | Abseil `absl/time` |
|---|---|---|---|
| Recurrence concept provided | Yes (it *is* your engine) | No | No |
| Calendar primitives | Raw `struct tm`, manual math | Rich: `nth_kday_of_month`, date iterators | `CivilDay`, `GetWeekday`, `NextWeekday`/`PrevWeekday` |
| "3rd Monday of month" | ~30 manual lines | **1 call** (`nth_day_of_the_week_in_month`) | ~5 composed lines |
| DST correctness | Manual, fragile (`tm_isdst=-1`) | Correct via `local_time` | **Excellent**: `TimeInfo` exposes skipped/ambiguous instants |
| Per-timeperiod timezone | ❌ process-global (`setenv TZ`+`tzset`) | ✅ `time_zone_ptr` per object | ✅ `absl::TimeZone` per object (IANA tzdata) |
| Timezone thread-safety | ❌ global mutation | ✅ | ✅ (immutable value type) |
| Already a project dependency | ✅ (it is the code) | ❌ (new vcpkg port, compiled component) | ✅ **already used everywhere** |
| Code volume to maintain | ~1600 low-level LOC | Medium | Medium |
| Readability | Low (C-style) | Good | Good (modern C++) |
| Migration risk (Nagios parity) | **None** (nothing moves) | High | High |
| Timezone source | zoneinfo via libc | POSIX TZ strings / bundled CSV | **system IANA tzdata** |

## 6. Per-option summary

### Current code
Keep it if the goal is "usable from broker with minimal risk". Zero behavioural
change, the `get_next_valid_time/*` test suite already green, no dependency. But
DST is manual/fragile, and above all timezone via global `setenv/tzset` is
**unacceptable as-is in multi-threaded broker**. Reusing it in broker requires at
minimum replacing `timezone_manager` with a stateless computation.

### Boost.Date_Time
The best calendar *vocabulary* (`nth_kday_of_month`, date iterators); the option
that shortens the "nth weekday of month" logic the most. But a **new dependency**
(compiled component, vcpkg port to add) and a timezone model (`local_time` via
POSIX TZ strings / CSV) that is more dated and less clean than Abseil's tzdata.
Only justified if that ready-made vocabulary is a real win.

### Abseil
The best compromise for a from-scratch rewrite: **already a dependency** (zero
integration cost — `absl::flat_hash_map` is used right down to `timeperiod.hh`),
**best DST handling** of the lot (explicit skipped/ambiguous-instant handling,
exactly the `dst_forward`/`dst_backward` cases), **thread-safe per-object
timezone** (`absl::TimeZone`, real IANA tzdata) that eliminates the global
`setenv/tzset`. Only downside vs Boost: no ready-made `nth_kday` helper → a few
composed lines from `CivilDay` + `GetWeekday`, negligible.

## 7. Recommendation

The choice is essentially binary depending on the goal:

- **Goal = decouple quickly and reuse in broker without risk** → keep the current
  code, extract it into a library (cf. §8), but **mandatorily replace
  `setenv/tzset` with a stateless timezone computation** — possibly borrowing
  `absl::TimeZone` just for that brick, without rewriting the engine. Zero
  behavioural risk, broker-safe.

- **Goal = rebuild from scratch cleanly and durably** → **Abseil**. Already
  present, correct DST, thread-safe per-timeperiod timezone; and the proto already
  carries `timezone` / `use_timezone` (the need exists in the data model).

Boost wins in no realistic scenario here: Abseil is already in the project and its
tzdata is better.

> **Deciding factor**: going through `setenv("TZ")` + `tzset()` is process-global
> state, thread-unsafe. Acceptable in engine (single-threaded on this path), it is
> dangerous in broker (io_context thread pools). That is the real driver of the
> decision, more than syntactic convenience.

## 8. Extraction plan

Suggested order, least to most risky:

1. Move `timeperiod` / `daterange` / `timerange` (done, under
   `engine/src/timeperiods/`), fix all consumer includes.
2. Remove dead includes/usages (`broker.hh`, `using ...applier`).
3. Break the couplings: injected logger, `engine_error` → `msg_fmt`,
   `contains_illegal_object_chars` moved down, `daterange.hh` without
   `engine/common.hh`.
4. Structural decision: remove the **global static registry**
   `timeperiod::timeperiods`. Two options:
   - **A — pure-logic library**: the map stays with each consumer; `resolve()`
     receives the map. Cleanest/most reusable, but touches the signature and the
     engine code relying on `timeperiod::timeperiods`.
   - **B — library + manager**: the library provides a small `timeperiod_manager`
     each daemon instantiates. Less engine churn, more API surface.
5. Move the library under `common/timeperiods/`, namespace
   `com::centreon::common::timeperiods` (keep an engine-side transition alias
   while migrating the ~25 consumer files).
6. Link broker against the library for notification time-window evaluation.

## 9. Open leads

- **Abseil POC**: rewrite a key function (e.g. `get_next_valid_time` for the "nth
  weekday of month" rule) in Abseil and validate it against the existing
  `get_next_valid_time/*` suite, to measure the readability gain and DST
  robustness.
- **Replacing `timezone_manager`** with a stateless timezone computation
  (priority brick for any reuse in broker).
- **Choice A vs B** for the registry (cf. §8.4).
