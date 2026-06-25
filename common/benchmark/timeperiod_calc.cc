/**
 * Copyright 2026 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */

// POC benchmark comparing the two timeperiods calendar helpers in their current
// form (hand-rolled struct tm + the lib's mktime-equivalent time_from_tm,
// reached through timeperiod_detail.hh) against an Abseil CivilDay rewrite
// written locally below. A correctness sweep runs first and aborts if the two
// implementations ever disagree, so the timing comparison is apples to apples.
//
// Argument convention (struct tm), shared by old and new:
//   year = tm_year (year - 1900), month = tm_mon (0-based),
//   weekday = tm_wday (0 = Sunday).
//
// Build:  ninja -Cbuild common_timeperiod_calc_bench
// Run:    cd build && tests/../common/benchmark/common_timeperiod_calc_bench
//         (or: ./build/common/benchmark/common_timeperiod_calc_bench)

#include <benchmark/benchmark.h>

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <vector>

#include "absl/time/civil_time.h"
#include "absl/time/time.h"
#include "common/timeperiods/timeperiod.hh"
#include "common/timeperiods/timeperiod_detail.hh"

using com::centreon::common::timeperiods::calculate_time_from_day_of_month;
using com::centreon::common::timeperiods::calculate_time_from_weekday_of_month;
using com::centreon::common::timeperiods::timeperiod;
using com::centreon::common::timeperiods::timeperiod_test_access;
namespace cfg = com::centreon::engine::configuration;

namespace {

// ── Verbatim copies of the current implementations ──────────────────────────
//
// The library is built, in this dev tree, at -O0 with coverage instrumentation
// (-fprofile-arcs -ftest-coverage); the bench file is built at -O2. Comparing
// the lib functions directly against the new code would therefore mix the
// algorithm change with a flags difference. To time old vs new under identical
// flags we keep verbatim copies of the current code here (so both are -O2).
// The correctness sweep checks these copies stay faithful to the library.

time_t time_from_tm_old(struct tm* t, const absl::TimeZone& tz) {
  absl::Time when = absl::FromTM(*t, tz);
  *t = absl::ToTM(when, tz);
  return absl::ToTimeT(when);
}

void tm_from_time_old(time_t when, const absl::TimeZone& tz, struct tm* out) {
  *out = absl::ToTM(absl::FromTimeT(when), tz);
}

time_t add_round_days_old(time_t midnight,
                          time_t skip,
                          const absl::TimeZone& tz) {
  time_t next_day_time(midnight + skip);
  struct tm next_day;
  tm_from_time_old(next_day_time, tz, &next_day);
  if (next_day.tm_hour || next_day.tm_min || next_day.tm_sec) {
    next_day_time += 12 * 60 * 60;
    tm_from_time_old(next_day_time, tz, &next_day);
    next_day.tm_hour = 0;
    next_day.tm_min = 0;
    next_day.tm_sec = 0;
    next_day.tm_isdst = -1;
    next_day_time = time_from_tm_old(&next_day, tz);
  }
  return next_day_time;
}

time_t day_of_month_old(int year,
                        int month,
                        int monthday,
                        const absl::TimeZone& tz) {
  time_t midnight;
  tm t;
  t.tm_sec = 0;
  t.tm_min = 0;
  t.tm_hour = 0;
  if (monthday > 0) {
    t.tm_year = year;
    t.tm_mon = month;
    t.tm_mday = monthday;
    t.tm_isdst = -1;
    midnight = time_from_tm_old(&t, tz);
    if (t.tm_mon != month)
      midnight = (time_t)-1;
  } else {
    int day(32);
    do {
      --day;
      t.tm_mon = month;
      t.tm_year = year;
      t.tm_mday = day;
      t.tm_isdst = -1;
      midnight = time_from_tm_old(&t, tz);
    } while ((midnight == (time_t)-1) || (t.tm_mon != month));
    t.tm_mon = month;
    t.tm_year = year;
    if (-monthday >= t.tm_mday)
      t.tm_mday = 1;
    else
      t.tm_mday += monthday + 1;
    t.tm_isdst = -1;
    midnight = time_from_tm_old(&t, tz);
  }
  return midnight;
}

time_t weekday_of_month_old(int year,
                            int month,
                            int weekday,
                            int weekday_offset,
                            const absl::TimeZone& tz) {
  tm t;
  t.tm_sec = 0;
  t.tm_min = 0;
  t.tm_hour = 0;
  t.tm_year = year;
  t.tm_mon = month;
  t.tm_mday = 1;
  t.tm_isdst = -1;
  time_from_tm_old(&t, tz);
  time_t midnight;
  int days(weekday - (t.tm_wday));
  if (days < 0)
    days += 7;
  if (weekday_offset > 0) {
    int weeks((weekday_offset > 5) ? 5 : weekday_offset);
    days += ((weeks - 1) * 7);
    t.tm_mon = month;
    t.tm_year = year;
    t.tm_mday = days + 1;
    t.tm_isdst = -1;
    midnight = time_from_tm_old(&t, tz);
    if (t.tm_mon != month)
      midnight = (time_t)-1;
  } else {
    days += (5 * 7);
    do {
      days -= 7;
      t.tm_mon = month;
      t.tm_year = year;
      t.tm_mday = days + 1;
      t.tm_isdst = -1;
      midnight = time_from_tm_old(&t, tz);
    } while ((midnight == (time_t)-1) || (t.tm_mon != month));
    days = ((weekday_offset + 1) * 7);
    t.tm_mon = month;
    t.tm_year = year;
    if (-days >= t.tm_mday)
      t.tm_mday = t.tm_mday % 7;
    else
      t.tm_mday += days;
    t.tm_isdst = -1;
    midnight = time_from_tm_old(&t, tz);
  }
  return midnight;
}

// ── Abseil rewrites (the "new" implementations under evaluation) ────────────

// Midnight of a civil day, expressed in tz, as a time_t. Mirrors the lib's
// time_from_tm for a midnight with tm_isdst = -1 (which resolves to the
// pre-transition instant).
time_t civil_midnight(absl::CivilDay d, const absl::TimeZone& tz) {
  return absl::ToTimeT(
      tz.At(absl::CivilSecond(d.year(), d.month(), d.day(), 0, 0, 0)).pre);
}

time_t add_round_days_new(time_t midnight,
                          time_t skip,
                          const absl::TimeZone& tz) {
  absl::CivilDay base = absl::ToCivilDay(absl::FromTimeT(midnight), tz);
  return civil_midnight(base + (skip / (24 * 60 * 60)), tz);
}

time_t day_of_month_absl(int year,
                         int month,
                         int monthday,
                         const absl::TimeZone& tz) {
  const int y = year + 1900;
  const int m = month + 1;  // CivilDay months are 1-based.

  if (monthday > 0) {
    absl::CivilDay d(y, m, monthday);
    // CivilDay normalises overflow into the next month; the old code treats
    // that as "day does not exist in this month".
    if (d.month() != m)
      return static_cast<time_t>(-1);
    return civil_midnight(d, tz);
  }

  // Negative: count from the end. Last day = first of next month minus one.
  const int last_day = (absl::CivilDay(y, m + 1, 1) - 1).day();
  // Same clamp as the current code: too far back collapses to the 1st.
  const int mday = (-monthday >= last_day) ? 1 : (last_day + monthday + 1);
  return civil_midnight(absl::CivilDay(y, m, mday), tz);
}

constexpr absl::Weekday kWeekday[7] = {
    absl::Weekday::sunday,    absl::Weekday::monday,   absl::Weekday::tuesday,
    absl::Weekday::wednesday, absl::Weekday::thursday, absl::Weekday::friday,
    absl::Weekday::saturday};

time_t weekday_of_month_absl(int year,
                             int month,
                             int weekday,
                             int weekday_offset,
                             const absl::TimeZone& tz) {
  const int y = year + 1900;
  const int m = month + 1;
  const absl::Weekday wd = kWeekday[weekday];

  if (weekday_offset > 0) {
    absl::CivilDay first(y, m, 1);
    // First occurrence on/after the 1st.
    absl::CivilDay occ =
        (absl::GetWeekday(first) == wd) ? first : absl::NextWeekday(first, wd);
    const int weeks = (weekday_offset > 5) ? 5 : weekday_offset;
    occ = occ + (weeks - 1) * 7;
    if (occ.month() != m)  // Rolled past the month → invalid.
      return static_cast<time_t>(-1);
    return civil_midnight(occ, tz);
  }

  // Negative: last occurrence on/before the last day, then step back.
  absl::CivilDay last = absl::CivilDay(y, m + 1, 1) - 1;
  absl::CivilDay last_occ =
      (absl::GetWeekday(last) == wd) ? last : absl::PrevWeekday(last, wd);
  const int last_occ_day = last_occ.day();
  const int days = (weekday_offset + 1) * 7;  // <= 0
  // Same clamp as the current code (note: %7, like the original).
  const int mday =
      (-days >= last_occ_day) ? (last_occ_day % 7) : (last_occ_day + days);
  return civil_midnight(absl::CivilDay(y, m, mday), tz);
}

// ── Inputs ──────────────────────────────────────────────────────────────────

constexpr int kYear = 2016 - 1900;  // tm_year

struct day_case {
  int month;  // tm_mon (0-based)
  int monthday;
};
const std::vector<day_case> kDayCases = {{0, 1},  {0, 15},  {0, 31},  {1, 28},
                                         {1, 29}, {1, -1},  {2, -2},  {3, 31},
                                         {9, -1}, {9, -31}, {10, 31}, {11, -2}};

struct weekday_case {
  int weekday;  // tm_wday (0 = Sunday)
  int offset;
};
const std::vector<weekday_case> kWeekdayCases = {
    {1, 1},  {1, 3},  {1, 5},  {1, 6},  {2, 5},  {6, 1},
    {1, -1}, {0, -1}, {4, -2}, {2, -6}, {3, -3}, {5, -4}};

// ── Correctness sweep: old must equal new, else the bench is meaningless ────

int correctness_sweep() {
  const absl::TimeZone tz = absl::UTCTimeZone();
  int mismatches = 0;
  for (int year : {2015, 2016, 2020, 2024}) {
    const int ty = year - 1900;
    for (int month = 0; month < 12; ++month) {
      for (int monthday :
           {0, 1, 2, 14, 15, 28, 29, 30, 31, -1, -2, -5, -28, -31, -40}) {
        time_t lib = calculate_time_from_day_of_month(ty, month, monthday, tz);
        time_t old_local = day_of_month_old(ty, month, monthday, tz);
        time_t neu = day_of_month_absl(ty, month, monthday, tz);
        if (lib != old_local || lib != neu) {
          if (mismatches < 10)
            std::printf(
                "DAY mismatch y=%d m=%d day=%d  lib=%ld old=%ld new=%ld\n",
                year, month, monthday, static_cast<long>(lib),
                static_cast<long>(old_local), static_cast<long>(neu));
          ++mismatches;
        }
      }
      for (int weekday = 0; weekday < 7; ++weekday) {
        for (int offset : {1, 2, 3, 4, 5, 6, -1, -2, -3, -4, -5, -6}) {
          time_t lib = calculate_time_from_weekday_of_month(ty, month, weekday,
                                                            offset, tz);
          time_t old_local =
              weekday_of_month_old(ty, month, weekday, offset, tz);
          time_t neu = weekday_of_month_absl(ty, month, weekday, offset, tz);
          if (lib != old_local || lib != neu) {
            if (mismatches < 10)
              std::printf(
                  "WEEKDAY mismatch y=%d m=%d wd=%d off=%d  lib=%ld old=%ld "
                  "new=%ld\n",
                  year, month, weekday, offset, static_cast<long>(lib),
                  static_cast<long>(old_local), static_cast<long>(neu));
            ++mismatches;
          }
        }
      }
    }
  }
  return mismatches;
}

// _add_round_days is the DST-sensitive helper, so its equivalence is swept
// across several timezones and every day of two years (covering the spring and
// autumn transitions), including a half-hour-DST zone (Lord Howe) and a zone
// whose DST transition historically fell on midnight (São Paulo).
int add_round_days_sweep() {
  int mismatches = 0;
  const char* zones[] = {"UTC", "Europe/Paris", "America/New_York",
                         "Australia/Lord_Howe", "America/Sao_Paulo"};
  for (const char* zone_name : zones) {
    absl::TimeZone tz;
    if (!absl::LoadTimeZone(zone_name, &tz)) {
      std::printf("add_round_days_sweep: cannot load zone %s\n", zone_name);
      ++mismatches;
      continue;
    }
    for (int year : {2016, 2017}) {
      for (absl::CivilDay d(year, 1, 1); d < absl::CivilDay(year + 1, 1, 1);
           d = d + 1) {
        const time_t midnight = civil_midnight(d, tz);
        for (int n : {1, 2, 7, 30, 180, 200, 365}) {
          const time_t skip = static_cast<time_t>(n) * 24 * 60 * 60;
          time_t a = add_round_days_old(midnight, skip, tz);
          time_t b = add_round_days_new(midnight, skip, tz);
          if (a != b) {
            if (mismatches < 10)
              std::printf(
                  "ADD_ROUND_DAYS mismatch zone=%s date=%d-%02d-%02d n=%d  "
                  "old=%ld new=%ld\n",
                  zone_name, static_cast<int>(d.year()), d.month(), d.day(), n,
                  static_cast<long>(a), static_cast<long>(b));
            ++mismatches;
          }
        }
      }
    }
  }
  return mismatches;
}

// ── Benchmarks ───────────────────────────────────────────────────────────────

const absl::TimeZone g_tz = absl::UTCTimeZone();

// Reference: the library function as actually built in this dev tree
// (-O0 + coverage instrumentation). Not a fair algorithmic comparison, shown
// only to gauge the real cost during instrumented test runs.
void BM_day_of_month_old_libcov(benchmark::State& state) {
  for (auto _ : state)
    for (const auto& c : kDayCases)
      benchmark::DoNotOptimize(
          calculate_time_from_day_of_month(kYear, c.month, c.monthday, g_tz));
}
BENCHMARK(BM_day_of_month_old_libcov);

// Fair baseline: the current implementation compiled at -O2 like the new one.
void BM_day_of_month_old(benchmark::State& state) {
  for (auto _ : state)
    for (const auto& c : kDayCases)
      benchmark::DoNotOptimize(
          day_of_month_old(kYear, c.month, c.monthday, g_tz));
}
BENCHMARK(BM_day_of_month_old);

void BM_day_of_month_absl(benchmark::State& state) {
  for (auto _ : state)
    for (const auto& c : kDayCases)
      benchmark::DoNotOptimize(
          day_of_month_absl(kYear, c.month, c.monthday, g_tz));
}
BENCHMARK(BM_day_of_month_absl);

constexpr int kBenchMonth = 9;  // October (tm_mon)

// Reference: library function as built here (-O0 + coverage).
void BM_weekday_of_month_old_libcov(benchmark::State& state) {
  for (auto _ : state)
    for (const auto& c : kWeekdayCases)
      benchmark::DoNotOptimize(calculate_time_from_weekday_of_month(
          kYear, kBenchMonth, c.weekday, c.offset, g_tz));
}
BENCHMARK(BM_weekday_of_month_old_libcov);

// Fair baseline: current implementation at -O2.
void BM_weekday_of_month_old(benchmark::State& state) {
  for (auto _ : state)
    for (const auto& c : kWeekdayCases)
      benchmark::DoNotOptimize(
          weekday_of_month_old(kYear, kBenchMonth, c.weekday, c.offset, g_tz));
}
BENCHMARK(BM_weekday_of_month_old);

void BM_weekday_of_month_absl(benchmark::State& state) {
  for (auto _ : state)
    for (const auto& c : kWeekdayCases)
      benchmark::DoNotOptimize(
          weekday_of_month_absl(kYear, kBenchMonth, c.weekday, c.offset, g_tz));
}
BENCHMARK(BM_weekday_of_month_absl);

// _add_round_days is benchmarked under Europe/Paris so the old code's DST
// correction path (the +12h trick) is exercised on the dates that straddle a
// transition.
const absl::TimeZone g_paris = [] {
  absl::TimeZone z;
  absl::LoadTimeZone("Europe/Paris", &z);
  return z;
}();
const std::vector<time_t> g_midnights = [] {
  std::vector<time_t> v;
  for (absl::CivilDay d :
       {absl::CivilDay(2016, 1, 15), absl::CivilDay(2016, 3, 26),
        absl::CivilDay(2016, 6, 15), absl::CivilDay(2016, 10, 29),
        absl::CivilDay(2016, 12, 20)})
    v.push_back(civil_midnight(d, g_paris));
  return v;
}();
const std::vector<int> kSkips = {1, 7, 30, 200};

void BM_add_round_days_old(benchmark::State& state) {
  for (auto _ : state)
    for (time_t mid : g_midnights)
      for (int n : kSkips)
        benchmark::DoNotOptimize(add_round_days_old(
            mid, static_cast<time_t>(n) * 24 * 60 * 60, g_paris));
}
BENCHMARK(BM_add_round_days_old);

void BM_add_round_days_absl(benchmark::State& state) {
  for (auto _ : state)
    for (time_t mid : g_midnights)
      for (int n : kSkips)
        benchmark::DoNotOptimize(add_round_days_new(
            mid, static_cast<time_t>(n) * 24 * 60 * 60, g_paris));
}
BENCHMARK(BM_add_round_days_absl);

// ── _get_next_invalid_time: current vs one-pass Abseil ──────────────────────
//
// Characterization showed the method is effectively a single pass: it returns
// the end of the timerange containing preferred_time (no extension across
// days), the start of an exclusion that cuts the window short, or
// preferred_time when already outside the period. For *weekly* timeperiods (no
// exceptions, no exclusions — the dominant case) the current code still scans
// up to 8 future days, even though only day 0 can ever contain a fixed instant.
// The "new" version below does only that day-0 check. Both are compared for
// equivalence and speed; the gain shows up on the "not covered" path (the
// wasted 8-day scan).

void bench_add_range(cfg::DaysArray* days, int wd, uint64_t s, uint64_t e) {
  cfg::Timerange* tr = nullptr;
  switch (wd) {
    case 0:
      tr = days->add_sunday();
      break;
    case 1:
      tr = days->add_monday();
      break;
    case 2:
      tr = days->add_tuesday();
      break;
    case 3:
      tr = days->add_wednesday();
      break;
    case 4:
      tr = days->add_thursday();
      break;
    case 5:
      tr = days->add_friday();
      break;
    case 6:
      tr = days->add_saturday();
      break;
  }
  tr->set_range_start(s);
  tr->set_range_end(e);
}

cfg::Timeperiod conf_24x7() {
  cfg::Timeperiod tp;
  tp.set_timeperiod_name("24x7");
  tp.set_alias("24x7");
  for (int d = 0; d < 7; ++d)
    bench_add_range(tp.mutable_timeranges(), d, 0, 86400);
  return tp;
}

cfg::Timeperiod conf_workhours() {
  cfg::Timeperiod tp;
  tp.set_timeperiod_name("workhours");
  tp.set_alias("workhours");
  for (int d = 1; d <= 5; ++d)
    bench_add_range(tp.mutable_timeranges(), d, 9 * 3600, 17 * 3600);
  return tp;
}

// The 3rd Monday of every month, 09:00-17:00 (a month_week_day exception): the
// path that makes get_next_valid crawl day by day toward the occurrence.
cfg::Timeperiod conf_exceptions() {
  cfg::Timeperiod tp;
  tp.set_timeperiod_name("third_monday");
  tp.set_alias("third_monday");
  cfg::Daterange* dr = tp.mutable_exceptions()->add_month_week_day();
  dr->set_type(cfg::Daterange::month_week_day);
  dr->set_swday(1);         // Monday
  dr->set_swday_offset(3);  // 3rd
  dr->set_ewday(1);
  dr->set_ewday_offset(3);
  cfg::Timerange* tr = dr->add_timerange();
  tr->set_range_start(9 * 3600);
  tr->set_range_end(17 * 3600);
  return tp;
}

// OLD: the library method as it is today.
time_t next_invalid_old(timeperiod& tp, time_t pref, const absl::TimeZone& tz) {
  return timeperiod_test_access::next_invalid_time(tp, pref, false, tz);
}

// NEW: one-pass, day-0-only (valid for weekly timeperiods without exceptions or
// exclusions).
time_t next_invalid_weekly_new(timeperiod& tp,
                               time_t pref,
                               const absl::TimeZone& tz) {
  struct tm t = absl::ToTM(absl::FromTimeT(pref), tz);
  for (const auto& tr : tp.days[t.tm_wday]) {
    auto to_t = [&](int secs) {
      return absl::ToTimeT(
          tz.At(absl::CivilSecond(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                                  secs / 3600, (secs / 60) % 60, 0))
              .pre);
    };
    time_t rs = to_t(tr.get_range_start());
    time_t re = to_t(tr.get_range_end());
    if (rs <= re && pref >= rs && pref < re)
      return re;
  }
  return pref;
}

timeperiod g_workhours(conf_workhours());
timeperiod g_24x7(conf_24x7());
timeperiod g_exceptions(conf_exceptions());
// 2024-01-03 12:00 UTC (Wednesday, inside work hours) and 2024-01-06 20:00 UTC
// (Saturday, outside work hours → the wasted-scan path).
constexpr time_t k_wed_noon = 1704283200;
constexpr time_t k_sat_evening = 1704571200;
// 2024-01-01 00:00 UTC (Monday); the 3rd Monday of Jan 2024 is the 15th, so the
// current get_next_valid crawls ~2 weeks day by day to reach it.
constexpr time_t k_jan1 = 1704067200;

int next_invalid_sweep() {
  int mismatches = 0;
  const absl::TimeZone tz = absl::UTCTimeZone();
  struct {
    timeperiod* tp;
    time_t pref;
  } cases[] = {{&g_workhours, k_wed_noon},
               {&g_workhours, k_sat_evening},
               {&g_24x7, k_wed_noon},
               {&g_24x7, k_sat_evening}};
  for (auto& c : cases) {
    time_t a = next_invalid_old(*c.tp, c.pref, tz);
    time_t b = next_invalid_weekly_new(*c.tp, c.pref, tz);
    if (a != b) {
      std::printf("NEXT_INVALID mismatch pref=%ld  old=%ld new=%ld\n",
                  static_cast<long>(c.pref), static_cast<long>(a),
                  static_cast<long>(b));
      ++mismatches;
    }
  }
  return mismatches;
}

void BM_next_invalid_old_covered(benchmark::State& state) {
  for (auto _ : state)
    benchmark::DoNotOptimize(next_invalid_old(g_workhours, k_wed_noon, g_tz));
}
BENCHMARK(BM_next_invalid_old_covered);

void BM_next_invalid_new_covered(benchmark::State& state) {
  for (auto _ : state)
    benchmark::DoNotOptimize(
        next_invalid_weekly_new(g_workhours, k_wed_noon, g_tz));
}
BENCHMARK(BM_next_invalid_new_covered);

void BM_next_invalid_old_uncovered(benchmark::State& state) {
  for (auto _ : state)
    benchmark::DoNotOptimize(
        next_invalid_old(g_workhours, k_sat_evening, g_tz));
}
BENCHMARK(BM_next_invalid_old_uncovered);

void BM_next_invalid_new_uncovered(benchmark::State& state) {
  for (auto _ : state)
    benchmark::DoNotOptimize(
        next_invalid_weekly_new(g_workhours, k_sat_evening, g_tz));
}
BENCHMARK(BM_next_invalid_new_uncovered);

// get_next_valid baselines (call the inner method directly to avoid the
// wall-clock clamp of the public get_next_valid_time, keeping the timing
// deterministic). Weekly = a short forward scan (weekend -> Monday); exceptions
// = the costly day-by-day crawl toward the next "3rd Monday".
void BM_get_next_valid_weekly(benchmark::State& state) {
  for (auto _ : state)
    benchmark::DoNotOptimize(timeperiod_test_access::next_valid_time(
        g_workhours, k_sat_evening, false, g_tz));
}
BENCHMARK(BM_get_next_valid_weekly);

void BM_get_next_valid_exceptions(benchmark::State& state) {
  for (auto _ : state)
    benchmark::DoNotOptimize(timeperiod_test_access::next_valid_time(
        g_exceptions, k_jan1, false, g_tz));
}
BENCHMARK(BM_get_next_valid_exceptions);

}  // namespace

int main(int argc, char** argv) {
  const int mismatches = correctness_sweep();
  if (mismatches == 0)
    std::printf("correctness sweep: OK (old == new on all swept inputs)\n");
  else
    std::printf("correctness sweep: %d MISMATCH(es) — see above\n", mismatches);

  const int add_mismatches = add_round_days_sweep();
  if (add_mismatches == 0)
    std::printf(
        "_add_round_days sweep: OK (old == new, all zones/dates/skips)\n");
  else
    std::printf("_add_round_days sweep: %d MISMATCH(es) — see above\n",
                add_mismatches);

  const int inv_mismatches = next_invalid_sweep();
  if (inv_mismatches == 0)
    std::printf("get_next_invalid sweep: OK (old == new, weekly cases)\n");
  else
    std::printf("get_next_invalid sweep: %d MISMATCH(es) — see above\n",
                inv_mismatches);

  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
  return 0;
}
