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
  - [10. Benchmarks — current baseline](#10-benchmarks--current-baseline)
  - [11. Final library architecture (as implemented)](#11-final-library-architecture-as-implemented)

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
4. Structural decision (**settled**): remove the **global static registry**
   `timeperiod::timeperiods`. Two options were on the table:
   - **A — pure-logic library**: the map stays with each consumer; `resolve()`
     receives the map. Cleanest/most reusable, but touches the signature and the
     engine code relying on `timeperiod::timeperiods`.
   - **B — library + manager**: the library provides a small `timeperiod_manager`
     each daemon instantiates. Less engine churn, more API surface.

   **Chosen: a B-dominated hybrid.** A `timeperiod_manager` (a `load`/`unload`
   singleton, with logger and illegal characters injected) owns the map and
   carries the collection-level operations (lookup, exclusion resolution). But
   the `timeperiod` value class stays **pure, A-style**: `resolve()` takes a
   `const timeperiod_map&` and no evaluation method depends on a global registry
   — so the library is testable without the manager and reusable as is.
5. Move the library under `common/timeperiods/`, namespace
   `com::centreon::common::timeperiods` (keep an engine-side transition alias
   while migrating the ~25 consumer files).
6. Link broker against the library for notification time-window evaluation.

## 9. Replacing `timezone_manager` — phased plan

Going through `setenv("TZ")` + `tzset()` (cf. §3) is the real blocker for broker.
We remove it in three phases, least to most risky:

- **Phase 0 — safety net (DONE).** Before touching the engine, freeze the current
  behaviour with multi-timezone golden-master tests:
  `common/tests/timeperiods/get_next_valid_time/timezone.cc`. The same
  "Monday 09:00-17:00" timeperiod, evaluated from a single absolute instant,
  yields each zone's local 09:00 (Paris `07:00 UTC` < UTC `09:00 UTC` <
  New_York `13:00 UTC`), plus a half-hour case (Australia/Lord_Howe, +10:30) to
  trap any whole-hour-offset assumption. A `DISABLED_ConcurrentEvaluationsAre…`
  test documents the goal: it can only pass once the timezone is a per-call
  parameter (impossible today, the TZ state being global). Until then the tests
  set the zone via `setenv/tzset` in a `scoped_tz` RAII guard.
- **Phase 1 — timezone as a parameter (DONE).** `const absl::TimeZone& tz` now
  flows through the whole chain (`get_next_valid_time` /
  `check_time_against_period` / `*_per_timeperiod` + every internal static
  helper). Two stateless helpers replace libc in `timeperiod.cc`:
  `tm_from_time(time_t, tz)` (= `localtime_r`, via `absl::ToTM`) and
  `time_from_tm(tm*, tz)` (= `mktime`, via `absl::FromTM` then `absl::ToTM` to
  write back the normalised fields). DST: `absl::FromTM` returns the
  pre-transition instant when `tm_isdst != 0`, which exactly reproduces `mktime`
  with `tm_isdst = -1` (verified: every internal call-site sets `tm_isdst = -1`
  before converting). **Engine compatibility without touching the ~40
  call-sites**: the parameter defaults to `absl::LocalTimeZone()`, which re-reads
  `TZ` on every call (verified) → the default path reproduces today's behaviour
  identically, including the global `timezone_locker`. Consumers (broker, tests)
  pass an explicit `absl::TimeZone` → thread-safe, no global state. The Phase 0
  concurrent test is enabled and green. Benchmark unchanged under `TZ=UTC`
  (gnvt_24x7 ~0.6 µs; `LocalTimeZone()` is resolved once per public call, then
  the immutable zone is reused for all internal conversions).
- **Phase 2 — removal of `timezone_manager` / `timezone_locker` (DONE).** The 15
  engine sites that pushed a `timezone_locker` now pass an explicit zone via the
  `engine::string_to_timezone(get_timezone())` helper
  (`engine/src/timezone.{cc,hh}`: empty → `LocalTimeZone()`; strips the leading
  `:` of the TZ-env form `:Europe/Paris`; `LoadTimeZone` with `LocalTimeZone()`
  fallback). Engine's `timezone_manager.{cc,hh}` and `timezone_locker.{cc,hh}` are
  deleted (removed from CMake + dead includes cleaned from 12 tests). **No more
  `setenv`/`tzset` on the engine side** → the computation is genuinely free of
  global state. The sites NOT gated by `check_time_against_period` (4 reschedule
  spots where `check_period_ptr` may be null — `notifier.cc` sets it to `nullptr`
  when no period is defined) get an explicit `if (ptr) … else max(pref, now)`
  guard. Bonus: the per-call `stat()` of `/etc/localtime` disappears (the ×4–×8
  measured in §10). NB: `broker::time::timezone_locker` (BAM, legacy
  `broker::time::timeperiod`) is a separate module, out of scope.

### API cleanups done along the way

- `get_next_valid_time` is no longer a free function but a **method**
  `timeperiod::get_next_valid_time(time_t pref, tz)` that **returns** the `time_t`
  (instead of an out-pointer plus a `tperiod` parameter). Call-sites gated by
  `check_time_against_period` call `ptr->get_next_valid_time(…)` directly.
- The `common/timeperiods/` headers are guarded by `CCC_TIMEPERIODS_*` (the
  `common/` convention), no longer `CCE_*` (engine heritage).

Reminder: the timezone is **not** a property of the timeperiod (the proto
`Timeperiod` message has no tz field). It comes from the context
(host/service/contact, `get_timezone()`); it is an evaluation parameter, not a
member.

Other leads:

- **Abseil POC (DONE)**: the two calendar helpers `calculate_time_from_day_of_
  month` and `calculate_time_from_weekday_of_month` were rewritten with Abseil
  (`CivilDay` + `NextWeekday`/`PrevWeekday`) in a dedicated benchmark
  `common/benchmark/timeperiod_calc.cc` (target `common_timeperiod_calc_bench`).
  An equivalence sweep (4 years × 12 months × all offsets, including the
  degenerate `%7`/clamp edges) confirms **identical results** to the current
  code. At equal flags (-O2 on both sides):

  | Function | current | Abseil | speedup |
  |---|---|---|---|
  | `calculate_time_from_day_of_month` | ~866 ns | ~482 ns | ~1.8× |
  | `calculate_time_from_weekday_of_month` | ~1713 ns | ~616 ns | ~2.8× |

  (per-iteration over ~12 representative cases; CPU scaling on → orders of
  magnitude.) The gain is **algorithmic**: the -O0+coverage penalty is
  negligible (`_libcov` ≈ `_old`), the cost is dominated by `absl::FromTM/ToTM`
  timezone conversions. The current code does several (the `do/while` in
  `weekday_of_month` calls the conversion up to ~5×); the Abseil version does
  all the arithmetic in `CivilDay` (integers) and converts only **once** at the
  end. Bonus: ~20 lines instead of ~70, and the "+12h DST trick" in
  `_add_round_days_to_midnight` would vanish likewise. Prerequisite met: these
  helpers are pinned by isolated tests
  (`common/tests/timeperiods/calculate_time_from_*`).

  **Migration applied to the library (DONE)**: the three helpers
  (`calculate_time_from_day_of_month`, `calculate_time_from_weekday_of_month`,
  `_add_round_days_to_midnight`) are replaced by their Abseil versions in
  `common/timeperiods/timeperiod.cc` (internal `civil_midnight` helper +
  `kWeekday` table). Validation: isolated tests + `get_next_valid_time/*` +
  `dst_*` (ut_common) green, ut_engine green, and the bench sweeps (3-way
  equivalence lib==pre-migration==Abseil; multi-timezone DST sweep of
  `_add_round_days` over UTC/Paris/New_York/Lord_Howe/São_Paulo) OK everywhere.
  After migration the `_libcov` micro-benchmark (library as built) drops to
  ~485 ns (day) / ~629 ns (weekday) — the library itself sped up.
- **`get_next_invalid_time_per_timeperiod`: return value + single pass (DONE)**.
  The method now returns the `time_t` (instead of an out-parameter).
  Characterization (isolated `get_next_invalid_time.cc` tests + coverage) showed
  the `while`, the `in_one_year` cap and the
  `*invalid_time = original_preferred_time` branch were **vestigial/dead** (the
  loop ran only once), and the 8-day weekly scan was pointless (only the day of
  `preferred_time` can contain a fixed instant). Rewritten as a single pass
  (day-0 only), strictly equivalent (bench sweep OK). Bench: "outside-range"
  path **2017 → 209 ns (~9.6×)**, "inside-range" path 475 → 327 ns. The residual
  vs a weekly-only ideal is the faithful handling of exceptions/exclusions.
- **`_timerange_to_time_t` and `_daterange_calendar_date_to_time_t` → direct
  Abseil (DONE)**. `_timerange_to_time_t` (the most-called conversion) no longer
  uses `struct tm`/`memcpy`/`time_from_tm` (whose `ToTM` write-back was useless
  here) but `CivilSecond(...).pre` directly → `BM_next_invalid_old_covered`
  327 → 271 ns. `_daterange_calendar_date_to_time_t` switches to `CivilDay`.
  Equivalence preserved (bench sweeps + ut_common/ut_engine green).
- **`get_next_valid_time_per_timeperiod`: return value + occurrence jump
  (DONE)**. Returns the `time_t` (no out-param). Unlike `get_next_invalid`, its
  loop is **genuinely iterative** (day-by-day search) — no dead code, no
  single-pass rewrite. The optimization targets the **day-by-day crawl** toward
  a distant exception occurrence: when the timeperiod has **no** weekly schedule
  (`has_weekly` false) and nothing is valid today, it **jumps** straight to the
  earliest future date-range start instead of advancing one day. Safe (jump only
  when the day's scan completed → true minimum; an intermediate weekly range
  would otherwise be missed, hence the guard). Identical result (validated by the
  whole `get_next_valid_time/*` suite). Bench: "3rd Monday" **9580 → 1454 ns
  (~6.6×)**; weekly case unchanged.
- **`const` + thread-safe evaluation: exclusion cycle guard rewritten (DONE)**.
  The evaluation methods (`check_time_against_period`,
  `check_time_against_period_for_notif`, `get_next_valid_time` and the two
  internal helpers) are now **`const`**. The old cycle guard — which temporarily
  emptied `this->_exclusions` (a `std::move` round-trip) so a cyclic exclusion
  (A excludes B, B excludes A) terminated when recursion came back to `this` —
  **mutated the object**: it forbade `const` and, more importantly, **was not
  thread-safe** (two threads evaluating the same timeperiod clobbered each
  other's `_exclusions`). That is exactly the multi-threaded scenario targeted on
  the Broker side, hence a latent bug. It is replaced by a **recursion set**
  passed as a parameter (`absl::flat_hash_set<const timeperiod*>`, `nullptr` at
  the top level): each level inserts itself before descending and erases itself
  on return. Strictly identical semantics (it only blocks the ancestors of the
  current call chain = a true cycle; a diamond exclusion graph is still evaluated
  on every path), but the object is no longer mutated → **shared-state-free
  evaluation, safe from multiple threads**. A `!_exclusions.empty()` guard means
  a timeperiod with no exclusion (the hot path: 24x7/work hours) pays no
  allocation. Validated by ut_common + ut_engine.
- **Internal helper rename (DONE)**: `get_next_valid_time_per_timeperiod` /
  `get_next_invalid_time_per_timeperiod` (a suffix left over from their
  free-function days) become **private** `_get_next_valid_time` /
  `_get_next_invalid_time`. No production caller outside the library
  (engine/broker go through `check_time_against_period[_for_notif]` and
  `get_next_valid_time`). The benchmark and the characterization test, which call
  the internal method directly (to avoid the non-deterministic clamp-to-`now` of
  the public `get_next_valid_time`), reach it via `struct timeperiod_test_access`
  (a friend defined in the header, for test/bench use only).
- **Registry: choice A vs B — settled (DONE)**: a B-dominated hybrid
  (`timeperiod_manager` owns the map, the value class kept pure A-style); details
  in §8, step 4.

## 10. Benchmarks — current baseline

google-benchmark micro-benchmarks measure the two scheduler hot functions
(`check_time_against_period` and `get_next_valid_time`) on representative
timeperiod shapes. Source: `engine/benchmark/timeperiod.cc` (CMake target
`timeperiod_bench`), linked only against `timeperiods` + `engine_conf`.

```bash
ninja -Cbuild timeperiod_bench
./build/engine/benchmark/timeperiod_bench
```

**Method / caveats**: measured on a dev machine (22 cores @ 4.5 GHz, `-O2`
build), *CPU scaling enabled* → read as **orders of magnitude**. Medians over 10
repetitions. Fixed deterministic reference instants (Wed 2024-01-03, Sat
2024-01-06); logging routed to a null sink. The bench **pins `TZ=UTC`**
(`setenv`+`tzset` in its `main`) — see the box below: without it the figures
blow up and become uninterpretable.

| Benchmark | Shape / case | Time |
|---|---|---|
| `BM_check_24x7` | 24x7, valid instant | ~0.66 µs |
| `BM_check_workhours_inside` | Mon-Fri 9-17, inside | ~0.63 µs |
| `BM_check_workhours_outside` | Mon-Fri 9-17, outside | ~1.1 µs |
| `BM_check_exceptions` | 3rd Monday of month (`month_week_day`) | ~8 µs |
| `BM_gnvt_24x7` | 24x7 | ~0.59 µs |
| `BM_gnvt_workhours_immediate` | already valid | ~0.58 µs |
| `BM_gnvt_workhours_search` | weekend → forward scan | ~0.58 µs |
| `BM_gnvt_exceptions` | next "3rd Monday" | **~120 µs** |
| `BM_gnvt_exclusion` | work hours excluding 1 day | ~1.5 µs |
| `BM_gnvt_exclusion_chain/1` | exclusion chain, depth 1 | ~1.5 µs |
| `BM_gnvt_exclusion_chain/2` | depth 2 | ~16 µs |
| `BM_gnvt_exclusion_chain/4` | depth 4 | ~66 µs |
| `BM_gnvt_exclusion_chain/8` | depth 8 | **~524 µs** |
| `BM_gnvt_exclusion_chain/16` | depth 16 | **~9 ms** |

**Takeaways:**

- Simple cases (24x7, work hours) cost ~0.6 µs under a fixed `TZ`.
- The **exceptions** path (`month_week_day`) is 2 orders of magnitude slower
  (~8 µs for `check`, **~120 µs** for `get_next_valid_time`): the day-by-day
  search combined with per-candidate-day date computation is costly.
- The **exclusion chain blows up with depth**: roughly ×8 per depth doubling
  (1.5 µs → 16 → 66 → 524 µs → **9 ms** at depth 16). The exclusion recursion
  multiplicatively compounds each level's day-by-day scan → near geometric cost.
  A strong argument for the rework: deep exclusions are currently pathological.

> **Note**: the cycle-guard rewrite (§9, `const` + thread-safe evaluation) does
> **not** change this profile — the bench chain is acyclic, the geometric blowup
> is intrinsic to each level's day-by-day recursion. The hot path with no
> exclusion is unchanged (the `!_exclusions.empty()` guard). Cutting this cost
> would require an algorithmic change, still under study.

> **Critical `TZ` sensitivity**: `get_next_valid_time`/`check_time_against_period`
> go through `localtime_r`/`mktime`. When `TZ` is unset, glibc `stat()`s
> `/etc/localtime` on *every* call → **×4 to ×8** on every figure (e.g.
> `gnvt_24x7`: 0.46 µs under `TZ=UTC`, 0.56 µs under `TZ=Europe/Paris`, **2.0 µs
> with `TZ` unset**; `gnvt_workhours`: 0.58 µs → 1.0 µs → **3.8 µs**). That is why
> the bench pins `TZ`. Beyond the bench, this is **a finding in itself**: the
> current implementation's performance is dominated by libc time functions and
> highly environment-sensitive — one more concrete argument for Abseil, whose
> `absl::TimeZone` caches the zone (value object, no per-call `stat`, no global
> state).

These figures are the **baseline** (under `TZ=UTC`) to compare against for a
possible Abseil port (cf. §9) — in particular on the exceptions path and the
exclusion chain.

## 11. Final library architecture (as implemented)

The extraction and rework are done; this section describes where the library
landed.

**Location & contents.** `common/timeperiods/` is a standalone static library
(`timeperiods`) with no dependency on any host application:

- `timeperiod` — the value class / recurrence engine.
- `daterange`, `timerange` — the building blocks (exceptions and time ranges).
- `timezone` — `string_to_timezone()`, converting an Engine timezone directive
  (e.g. `:Europe/Paris`) into an `absl::TimeZone`.
- the `timeperiod_map` typedef: `absl::flat_hash_map<std::string,
  std::shared_ptr<timeperiod>>` (keyed by name).

**Timezone: per-call, no global state.** Every evaluation method
(`check_time_against_period`, `check_time_against_period_for_notif`,
`get_next_valid_time`, `get_next_invalid_time`, `duration_intersect`) takes an
explicit `absl::TimeZone` (defaulting to the daemon's local zone). The former
`setenv`/`tzset`/`timezone_manager` global approach is gone: civil-time
conversions go through `absl::FromTM`/`ToTM` against the passed zone. The
methods are `const` and cyclic-exclusion-safe (a per-traversal `chain` guard
breaks `A excludes B excludes A`).

**Exclusion resolution.** A `timeperiod`'s `exclude` directives are stored by
name and, once the objects exist, linked to raw pointers by
`timeperiod::resolve(const timeperiod_map& all, …)`. That linkage is a runtime
object-graph fact that does not survive serialization, so **every consumer that
builds `timeperiod` objects resolves them against its own map**.

**Logger injected, no global.** Each `timeperiod` carries its own logger, passed
to the constructor (`timeperiod(obj, logger)`), with a silent null-sink fallback
when none is given; the library no longer reaches a process-global logger.
`timerange` validates its bounds by throwing a descriptive exception (no
logging). This is what lets Broker run the library with a *dedicated* logger
(cf. the `CheckPollerConfig` endpoint) without touching any shared logger.

**No manager — each consumer owns its collection.** There is no
`timeperiod_manager` anymore; the collection lives wherever it is used:

- **Engine**: a process-global `::timeperiods` (`timeperiod_map`, declared in
  `engine/globals`), fed by `applier::timeperiod` and cleared at a controlled
  point on shutdown.
- **Broker (notifications)**: `broker_cache` owns its own `_timeperiods`
  (name-keyed), fed from the config `State`/`DiffState`, reference-counted per
  poller (a timeperiod is dropped when the last poller referencing it leaves).
- **BAM**: `reporting_stream` owns its own `bam::timeperiod_map` (keyed by the
  numeric reporting id), fed from BBDO `dimension_timeperiod` events.

**Where validation lives.** Character/name validation is a host concern (Engine
does it in `applier::timeperiod::add_object` via `contains_illegal_object_chars`;
the same check is being added to `engine_conf` for the Broker-side
`CheckPollerConfig`). Cross-reference validation (an `exclude` naming an
undefined timeperiod) lives in `engine_conf`'s `timeperiod_helper::expand`, so
both Engine and Broker reject such a configuration at parse/expand time — it
reports every offending timeperiod, then fails.
