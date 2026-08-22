/**
 * Copyright 2026 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */

/**
 * @file
 * @brief Stripping the blanks off a view: our own loop, or abseil's?
 *
 * The perfdata parser trims a lot -- once per metric, once per threshold field,
 * once per name -- so the question came up while rewriting it: it had grown its
 * own two-loop trim() when absl::StripAsciiWhitespace() does exactly the same
 * work on exactly the same character set. Rather than argue about it, measure.
 *
 * The expectation is that they are indistinguishable: abseil's is
 * StripTrailing(StripLeading(v)), each a loop over ascii_isspace, which is the
 * same table lookup ours uses. If the measurement confirms it, the tie-breaker
 * is the amount of code we maintain, and abseil wins by fourteen lines.
 *
 * Four shapes, because a trim's cost depends entirely on what it finds:
 *
 * * Clean is a field with nothing to strip -- "80", "12.5MB" -- which is what
 *   the parser meets most of the time, and where only the two failed tests cost
 *   anything.
 * * Leading is what sits between two metrics, " metric_2=12.5MB".
 * * Both is a quoted name once its quotes are gone, "  \n time".
 * * Long is the whole output on the first call: nothing to strip, but a big view
 *   -- it checks that neither implementation does anything silly with the size.
 */

#include <absl/strings/ascii.h>
#include <benchmark/benchmark.h>

#include <cstdio>
#include <string>
#include <string_view>

namespace {

/**
 * @brief Strip the leading and trailing blanks of a view, by hand.
 *
 * @param v The view.
 *
 * @return The trimmed view, which may be empty.
 */
inline std::string_view trim_hand(std::string_view v) {
  while (!v.empty() &&
         absl::ascii_isspace(static_cast<unsigned char>(v.front())))
    v.remove_prefix(1);
  while (!v.empty() &&
         absl::ascii_isspace(static_cast<unsigned char>(v.back())))
    v.remove_suffix(1);
  return v;
}

/**
 * @brief Strip the leading and trailing blanks of a view, with abseil.
 *
 * @param v The view.
 *
 * @return The trimmed view, which may be empty.
 */
inline std::string_view trim_absl(std::string_view v) {
  return absl::StripAsciiWhitespace(v);
}

const std::string& long_input() {
  static const std::string input = [] {
    std::string out;
    for (int i = 0; i < 3000; ++i) {
      if (i)
        out += ' ';
      out += "metric_" + std::to_string(i) + "=12.5MB;80;90;0;100";
    }
    return out;
  }();
  return input;
}

/* The four shapes, as views into storage that outlives the run. */
const std::string_view kClean = "12.5MB";
const std::string_view kLeading = " metric_2=12.5MB";
const std::string_view kBoth = "  \n time \t ";

}  // namespace

/**
 * @brief Trim a field with nothing to strip, by hand.
 *
 * @param state Benchmark state.
 */
static void BM_HandClean(benchmark::State& state) {
  for (auto _ : state)
    benchmark::DoNotOptimize(trim_hand(kClean));
}
BENCHMARK(BM_HandClean);

/**
 * @brief Trim a field with nothing to strip, with abseil.
 *
 * @param state Benchmark state.
 */
static void BM_AbslClean(benchmark::State& state) {
  for (auto _ : state)
    benchmark::DoNotOptimize(trim_absl(kClean));
}
BENCHMARK(BM_AbslClean);

/**
 * @brief Trim a view with leading blanks, by hand.
 *
 * @param state Benchmark state.
 */
static void BM_HandLeading(benchmark::State& state) {
  for (auto _ : state)
    benchmark::DoNotOptimize(trim_hand(kLeading));
}
BENCHMARK(BM_HandLeading);

/**
 * @brief Trim a view with leading blanks, with abseil.
 *
 * @param state Benchmark state.
 */
static void BM_AbslLeading(benchmark::State& state) {
  for (auto _ : state)
    benchmark::DoNotOptimize(trim_absl(kLeading));
}
BENCHMARK(BM_AbslLeading);

/**
 * @brief Trim a view with blanks at both ends, by hand.
 *
 * @param state Benchmark state.
 */
static void BM_HandBoth(benchmark::State& state) {
  for (auto _ : state)
    benchmark::DoNotOptimize(trim_hand(kBoth));
}
BENCHMARK(BM_HandBoth);

/**
 * @brief Trim a view with blanks at both ends, with abseil.
 *
 * @param state Benchmark state.
 */
static void BM_AbslBoth(benchmark::State& state) {
  for (auto _ : state)
    benchmark::DoNotOptimize(trim_absl(kBoth));
}
BENCHMARK(BM_AbslBoth);

/**
 * @brief Trim a large view with nothing to strip, by hand.
 *
 * @param state Benchmark state.
 */
static void BM_HandLong(benchmark::State& state) {
  const std::string_view v = long_input();
  for (auto _ : state)
    benchmark::DoNotOptimize(trim_hand(v));
}
BENCHMARK(BM_HandLong);

/**
 * @brief Trim a large view with nothing to strip, with abseil.
 *
 * @param state Benchmark state.
 */
static void BM_AbslLong(benchmark::State& state) {
  const std::string_view v = long_input();
  for (auto _ : state)
    benchmark::DoNotOptimize(trim_absl(v));
}
BENCHMARK(BM_AbslLong);

/**
 * @brief Check that both implementations agree, then run the benchmarks.
 *
 * Comparing the speed of two functions that do not return the same thing would
 * be meaningless, and the character sets are the only place they could differ.
 *
 * @param argc Argument count, forwarded to google-benchmark.
 * @param argv Argument values, forwarded to google-benchmark.
 *
 * @return EXIT_SUCCESS, or 1 if the two disagree on any input.
 */
int main(int argc, char** argv) {
  const std::string_view cases[] = {
      kClean,   kLeading, kBoth, long_input(), "",     " ",  "\t\n\r\f\v",
      "a",      " a",     "a ",  " a ",        "  ab", "ab", "\va\f",
  };
  for (std::string_view in : cases) {
    const std::string_view a = trim_hand(in);
    const std::string_view b = trim_absl(in);
    if (a != b) {
      std::fprintf(stderr, "MISMATCH on '%.*s': hand '%.*s' vs absl '%.*s'\n",
                   static_cast<int>(in.size()), in.data(),
                   static_cast<int>(a.size()), a.data(),
                   static_cast<int>(b.size()), b.data());
      return 1;
    }
  }
  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
  return 0;
}
