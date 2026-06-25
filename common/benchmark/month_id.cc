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

/* Micro-benchmark for the month-name → index lookup used by timeperiod_legacy.
 * Compares the current linear scan over a constexpr std::array<string_view>
 * against a static absl::flat_hash_map<std::string, uint32_t> lookup, for the
 * three regimes that matter at 12 entries: an early hit, a late hit, and a
 * miss (which forces the array to scan all 12 entries). */

#include <benchmark/benchmark.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

#include "absl/container/flat_hash_map.h"

namespace {

// A — current implementation: linear scan over a constexpr array.
bool month_id_array(std::string_view name, uint32_t& id) {
  static constexpr std::array<std::string_view, 12> months{
      "january", "february", "march",     "april",   "may",      "june",
      "july",    "august",   "september", "october", "november", "december"};
  for (id = 0; id < months.size(); ++id)
    if (name == months[id])
      return true;
  return false;
}

// B — static hash map, looked up heterogeneously with the string_view (no
// allocation thanks to absl's transparent string hashing).
bool month_id_hash(std::string_view name, uint32_t& id) {
  static const absl::flat_hash_map<std::string, uint32_t> months = {
      {"january", 0},   {"february", 1}, {"march", 2},     {"april", 3},
      {"may", 4},       {"june", 5},     {"july", 6},      {"august", 7},
      {"september", 8}, {"october", 9},  {"november", 10}, {"december", 11}};
  auto it = months.find(name);
  if (it == months.end())
    return false;
  id = it->second;
  return true;
}

// C — switch on the first letter, then compare only within that letter's group
// (at most three candidates, the "j" group january/june/july). Misses bail out
// on the first letter or after at most a couple of compares.
bool month_id_switch(std::string_view name, uint32_t& id) {
  if (name.empty())
    return false;
  switch (name[0]) {
    case 'j':
      if (name == "january") {
        id = 0;
        return true;
      }
      if (name == "june") {
        id = 5;
        return true;
      }
      if (name == "july") {
        id = 6;
        return true;
      }
      return false;
    case 'f':
      if (name == "february") {
        id = 1;
        return true;
      }
      return false;
    case 'm':
      if (name == "march") {
        id = 2;
        return true;
      }
      if (name == "may") {
        id = 4;
        return true;
      }
      return false;
    case 'a':
      if (name == "april") {
        id = 3;
        return true;
      }
      if (name == "august") {
        id = 7;
        return true;
      }
      return false;
    case 's':
      if (name == "september") {
        id = 8;
        return true;
      }
      return false;
    case 'o':
      if (name == "october") {
        id = 9;
        return true;
      }
      return false;
    case 'n':
      if (name == "november") {
        id = 10;
        return true;
      }
      return false;
    case 'd':
      if (name == "december") {
        id = 11;
        return true;
      }
      return false;
    default:
      return false;
  }
}

template <typename F>
void run_one(benchmark::State& state, F fn, std::string_view in) {
  uint32_t id = 0;
  for (auto _ : state) {
    bool ok = fn(in, id);
    benchmark::DoNotOptimize(ok);
    benchmark::DoNotOptimize(id);
  }
}

void BM_array_first(benchmark::State& s) {
  run_one(s, month_id_array, "january");
}
void BM_hash_first(benchmark::State& s) {
  run_one(s, month_id_hash, "january");
}
void BM_switch_first(benchmark::State& s) {
  run_one(s, month_id_switch, "january");
}
void BM_array_last(benchmark::State& s) {
  run_one(s, month_id_array, "december");
}
void BM_hash_last(benchmark::State& s) {
  run_one(s, month_id_hash, "december");
}
void BM_switch_last(benchmark::State& s) {
  run_one(s, month_id_switch, "december");
}
void BM_array_miss(benchmark::State& s) {
  run_one(s, month_id_array, "monday");
}
void BM_hash_miss(benchmark::State& s) {
  run_one(s, month_id_hash, "monday");
}
void BM_switch_miss(benchmark::State& s) {
  run_one(s, month_id_switch, "monday");
}
BENCHMARK(BM_array_first);
BENCHMARK(BM_hash_first);
BENCHMARK(BM_switch_first);
BENCHMARK(BM_array_last);
BENCHMARK(BM_hash_last);
BENCHMARK(BM_switch_last);
BENCHMARK(BM_array_miss);
BENCHMARK(BM_hash_miss);
BENCHMARK(BM_switch_miss);

// Realistic mix: the 12 months plus a few non-month tokens (the parser also
// feeds it weekday names and "day").
constexpr std::array<std::string_view, 16> kMix{
    "january", "february", "march",     "april",   "may",      "june",
    "july",    "august",   "september", "october", "november", "december",
    "monday",  "day",      "sunday",    "foo"};

void BM_array_mix(benchmark::State& s) {
  uint32_t id = 0;
  for (auto _ : s)
    for (std::string_view m : kMix) {
      benchmark::DoNotOptimize(month_id_array(m, id));
      benchmark::DoNotOptimize(id);
    }
}
void BM_hash_mix(benchmark::State& s) {
  uint32_t id = 0;
  for (auto _ : s)
    for (std::string_view m : kMix) {
      benchmark::DoNotOptimize(month_id_hash(m, id));
      benchmark::DoNotOptimize(id);
    }
}
void BM_switch_mix(benchmark::State& s) {
  uint32_t id = 0;
  for (auto _ : s)
    for (std::string_view m : kMix) {
      benchmark::DoNotOptimize(month_id_switch(m, id));
      benchmark::DoNotOptimize(id);
    }
}
BENCHMARK(BM_array_mix);
BENCHMARK(BM_hash_mix);
BENCHMARK(BM_switch_mix);

}  // namespace

int main(int argc, char** argv) {
  // Sanity: the three implementations must agree on every input before timing.
  for (std::string_view in : kMix) {
    uint32_t a = 0, b = 0, c = 0;
    bool ra = month_id_array(in, a);
    bool rb = month_id_hash(in, b);
    bool rc = month_id_switch(in, c);
    if (ra != rb || ra != rc || (ra && (a != b || a != c))) {
      std::fprintf(stderr, "MISMATCH on '%.*s'\n", static_cast<int>(in.size()),
                   in.data());
      return 1;
    }
  }
  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
  return 0;
}
