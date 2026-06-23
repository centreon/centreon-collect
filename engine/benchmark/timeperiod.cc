/**
 * Copyright 2026 Centreon (https://www.centreon.com/)
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
 */

// Micro-benchmarks for the hot timeperiod functions used by the scheduler:
//   - check_time_against_period() : is a given instant inside a timeperiod ?
//   - get_next_valid_time()       : next instant covered by a timeperiod.
// Both are exercised on representative timeperiod shapes (24x7, weekly work
// hours, exception dateranges, exclusions) and on both a "cheap" instant
// (already valid) and an "expensive" one (forces a forward search).

#include <benchmark/benchmark.h>

#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <ctime>
#include <memory>
#include <string>
#include <vector>

#include "common/timeperiods/timeperiod.hh"
#include "common/timeperiods/timeperiod_manager.hh"

using namespace com::centreon::engine;
using namespace com::centreon::common::timeperiods;
namespace cfg = com::centreon::engine::configuration;

// The timeperiods library is now self-contained: it logs through
// timeperiod_manager::logger() and provides its own (library-local)
// contains_illegal_object_chars, so no engine global needs to be supplied here.

namespace {

constexpr uint64_t k_day = 86400;
constexpr uint64_t k_9h = 9 * 3600;
constexpr uint64_t k_17h = 17 * 3600;

void add_range(cfg::DaysArray* days,
               int weekday,
               uint64_t start,
               uint64_t end) {
  cfg::Timerange* tr = nullptr;
  switch (weekday) {
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
  tr->set_range_start(start);
  tr->set_range_end(end);
}

// Build a "24x7" timeperiod: every day, all day long.
cfg::Timeperiod conf_24x7() {
  cfg::Timeperiod tp;
  tp.set_timeperiod_name("24x7");
  tp.set_alias("24x7");
  for (int d = 0; d < 7; ++d)
    add_range(tp.mutable_timeranges(), d, 0, k_day);
  return tp;
}

// Build a weekly "work hours" timeperiod: Mon-Fri 09:00-17:00.
cfg::Timeperiod conf_workhours(const std::string& name = "workhours") {
  cfg::Timeperiod tp;
  tp.set_timeperiod_name(name);
  tp.set_alias(name);
  for (int d = 1; d <= 5; ++d)
    add_range(tp.mutable_timeranges(), d, k_9h, k_17h);
  return tp;
}

// Build a timeperiod driven by an exception daterange: the 3rd Monday of every
// month, 09:00-17:00 (exercises the month_week_day arithmetic).
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
  tr->set_range_start(k_9h);
  tr->set_range_end(k_17h);
  return tp;
}

// A fixed, deterministic set of reference instants (no wall clock).
// 2024-01-03 was a Wednesday, 2024-01-06 a Saturday.
constexpr std::time_t k_wed_noon = 1704283200;     // inside work hours
constexpr std::time_t k_sat_evening = 1704571200;  // outside -> forward search

// ── check_time_against_period ───────────────────────────────────────────────

void BM_check_24x7(benchmark::State& state) {
  auto tp = std::make_shared<timeperiod>(conf_24x7());
  for (auto _ : state)
    benchmark::DoNotOptimize(check_time_against_period(k_wed_noon, tp.get()));
}
BENCHMARK(BM_check_24x7);

void BM_check_workhours_inside(benchmark::State& state) {
  auto tp = std::make_shared<timeperiod>(conf_workhours());
  for (auto _ : state)
    benchmark::DoNotOptimize(check_time_against_period(k_wed_noon, tp.get()));
}
BENCHMARK(BM_check_workhours_inside);

void BM_check_workhours_outside(benchmark::State& state) {
  auto tp = std::make_shared<timeperiod>(conf_workhours());
  for (auto _ : state)
    benchmark::DoNotOptimize(
        check_time_against_period(k_sat_evening, tp.get()));
}
BENCHMARK(BM_check_workhours_outside);

void BM_check_exceptions(benchmark::State& state) {
  auto tp = std::make_shared<timeperiod>(conf_exceptions());
  for (auto _ : state)
    benchmark::DoNotOptimize(check_time_against_period(k_wed_noon, tp.get()));
}
BENCHMARK(BM_check_exceptions);

// ── get_next_valid_time ─────────────────────────────────────────────────────

void BM_gnvt_24x7(benchmark::State& state) {
  auto tp = std::make_shared<timeperiod>(conf_24x7());
  time_t out = 0;
  for (auto _ : state) {
    get_next_valid_time(k_sat_evening, &out, tp.get());
    benchmark::DoNotOptimize(out);
  }
}
BENCHMARK(BM_gnvt_24x7);

// Cheap: preferred time already valid.
void BM_gnvt_workhours_immediate(benchmark::State& state) {
  auto tp = std::make_shared<timeperiod>(conf_workhours());
  time_t out = 0;
  for (auto _ : state) {
    get_next_valid_time(k_wed_noon, &out, tp.get());
    benchmark::DoNotOptimize(out);
  }
}
BENCHMARK(BM_gnvt_workhours_immediate);

// Expensive: preferred time on a weekend evening -> must scan forward.
void BM_gnvt_workhours_search(benchmark::State& state) {
  auto tp = std::make_shared<timeperiod>(conf_workhours());
  time_t out = 0;
  for (auto _ : state) {
    get_next_valid_time(k_sat_evening, &out, tp.get());
    benchmark::DoNotOptimize(out);
  }
}
BENCHMARK(BM_gnvt_workhours_search);

// Exception daterange: next "3rd Monday" from an arbitrary instant.
void BM_gnvt_exceptions(benchmark::State& state) {
  auto tp = std::make_shared<timeperiod>(conf_exceptions());
  time_t out = 0;
  for (auto _ : state) {
    get_next_valid_time(k_wed_noon, &out, tp.get());
    benchmark::DoNotOptimize(out);
  }
}
BENCHMARK(BM_gnvt_exceptions);

// Exclusions: a work-hours timeperiod (Mon-Fri 09:00-17:00) excluding a
// "holidays" timeperiod that covers Wednesdays only. This exercises the
// recursive exclusion handling in get_next_valid_time: from a Wednesday the
// candidate day is excluded, so the code recurses into "holidays" to find the
// exclusion boundary and advances to Thursday. The exclusion is narrow on
// purpose, so the timeperiod stays satisfiable and the call returns quickly
// (no full one-year scan).
void BM_gnvt_exclusion(benchmark::State& state) {
  auto& tps = timeperiod_manager::instance().timeperiods();
  tps.clear();

  cfg::Timeperiod h;
  h.set_timeperiod_name("holidays");
  h.set_alias("holidays");
  add_range(h.mutable_timeranges(), 3, 0, k_day);  // Wednesday 00:00-24:00
  tps.emplace("holidays", std::make_shared<timeperiod>(h));

  cfg::Timeperiod c = conf_workhours("work_excl");
  *c.mutable_exclude()->add_data() = "holidays";
  auto tp = std::make_shared<timeperiod>(c);
  tps.emplace("work_excl", tp);

  uint32_t w = 0, e = 0;
  timeperiod_manager::instance().resolve(*tp, w, e);

  time_t out = 0;
  for (auto _ : state) {
    get_next_valid_time(k_wed_noon, &out, tp.get());
    benchmark::DoNotOptimize(out);
  }
  tps.clear();
}
BENCHMARK(BM_gnvt_exclusion);

// Recursion-depth benchmark: a chain of `range` exclusion links
// (chain_0 excludes chain_1 excludes ... excludes chain_N), measuring how
// get_next_valid_time scales with the exclusion recursion depth.
//
// To keep every level satisfiable within a week (so the cost reflects the
// recursion, not a one-year scan), the leaf covers Sundays only and each
// intermediate link is 24x7 excluding the next one. Validity then alternates
// Sundays / Mon-Sat with the parity of the depth, always bounded.
void BM_gnvt_exclusion_chain(benchmark::State& state) {
  const int depth = static_cast<int>(state.range(0));
  auto& tps = timeperiod_manager::instance().timeperiods();
  tps.clear();

  std::vector<std::shared_ptr<timeperiod>> chain;
  chain.reserve(depth + 1);

  // Leaf (deepest): Sunday only, no exclusion.
  {
    const std::string name = "chain_" + std::to_string(depth);
    cfg::Timeperiod leaf;
    leaf.set_timeperiod_name(name);
    leaf.set_alias(name);
    add_range(leaf.mutable_timeranges(), 0, 0, k_day);  // Sunday 00:00-24:00
    chain.push_back(std::make_shared<timeperiod>(leaf));
    tps.emplace(name, chain.back());
  }

  // Intermediate links: 24x7 each excluding the next one down the chain.
  for (int i = depth - 1; i >= 0; --i) {
    const std::string name = "chain_" + std::to_string(i);
    cfg::Timeperiod c = conf_24x7();
    c.set_timeperiod_name(name);
    c.set_alias(name);
    *c.mutable_exclude()->add_data() = "chain_" + std::to_string(i + 1);
    chain.push_back(std::make_shared<timeperiod>(c));
    tps.emplace(name, chain.back());
  }

  // Resolve every period so exclusion pointers are wired across the chain.
  uint32_t w = 0, e = 0;
  for (auto& tp : chain)
    timeperiod_manager::instance().resolve(*tp, w, e);

  timeperiod* root = tps.at("chain_0").get();
  time_t out = 0;
  for (auto _ : state) {
    get_next_valid_time(k_wed_noon, &out, root);
    benchmark::DoNotOptimize(out);
  }
  tps.clear();
}
BENCHMARK(BM_gnvt_exclusion_chain)->RangeMultiplier(2)->Range(1, 16);

}  // namespace

int main(int argc, char** argv) {
  // Pin the timezone: get_next_valid_time() goes through localtime_r/mktime,
  // and when TZ is unset glibc stat()s /etc/localtime on every call (~4-8x
  // slower, and noisy). Fixing TZ makes the numbers reproducible and isolates
  // the timeperiod logic from the environment.
  setenv("TZ", "UTC", 1);
  tzset();

  // Load the manager with a null-sink logger so the library logs nowhere and
  // instance() is available (the exclusion benchmark registers timeperiods).
  timeperiod_manager::load(
      std::make_shared<spdlog::logger>(
          "bench", std::make_shared<spdlog::sinks::null_sink_mt>()),
      {});

  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();

  timeperiod_manager::unload();
  return 0;
}
