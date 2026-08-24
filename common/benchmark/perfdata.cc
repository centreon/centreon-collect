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
 * @brief Does parse_perfdata() cost more than linearly in the number of
 * metrics?
 *
 * The question is not academic. extract_float() locates a possible decimal
 * comma with strchr(str, ','), which scans to the end of the *whole* perfdata
 * string, and parse_perfdata() calls it up to seven times per metric -- value,
 * two thresholds of two bounds, min and max. A check output carrying one metric
 * pays nothing for that; an output carrying a thousand, as a docker or an
 * otel-fed check does, would pay it a thousand times over a thousand times more
 * bytes.
 *
 * The benchmark answers by sweeping the metric count and reporting items per
 * second: a linear parser keeps that figure flat, a quadratic one sees it
 * collapse as N grows. That is also why it reports per-item throughput rather
 * than wall time, which grows in both cases and tells them apart much less
 * readably.
 *
 * Three shapes, because the suspected cost has two distinct halves:
 *
 * * NoComma is the ordinary output. strchr finds nothing and therefore reads
 *   every remaining byte before giving up, so this isolates the quadratic
 *   *scan*.
 * * TrailingComma puts a decimal comma in the last metric only. Every number
 *   before it now finds a comma far away, and each one builds a std::string
 *   reaching up to it: this adds the quadratic *allocation and copy*. A comma
 *   anywhere in the output does it -- inside a quoted label, a unit, or a single
 *   value emitted by a plugin running under a French locale.
 * * CommaEverywhere is the same locale, applied to every number. Counter-
 *   intuitively it should be the cheapest of the three: the nearest comma is
 *   always inside the number being read, so both the scan and the copy stay
 *   short. Worth measuring, because it says the pathology is about the distance
 *   to the next comma and not about commas as such.
 */

#include <benchmark/benchmark.h>
#include <spdlog/sinks/null_sink.h>

#include <cstdio>
#include <string>

#include "com/centreon/common/perfdata.hh"

using com::centreon::common::perfdata;

namespace {

/**
 * @brief Build a perfdata string holding the requested number of metrics.
 *
 * The shape mirrors what a plugin emits: a name, a value with a unit, then the
 * four optional fields. Names differ from one metric to the next, since
 * parse_perfdata keeps a set of the ones it has seen and would otherwise report
 * duplicates and take its error path.
 *
 * @param count How many metrics.
 * @param decimal_comma_last Emit the last value with a decimal comma.
 * @param decimal_comma_all Emit every value with a decimal comma.
 *
 * @return The perfdata string.
 */
std::string make_perfdata(size_t count,
                          bool decimal_comma_last = false,
                          bool decimal_comma_all = false) {
  std::string out;
  /* Reserved generously: building the input must not show up in the profile of
   * the run that uses it. */
  out.reserve(count * 48);
  for (size_t i = 0; i < count; ++i) {
    if (i)
      out += ' ';
    const bool comma = decimal_comma_all || (decimal_comma_last &&
                                             i + 1 == count);
    out += "metric_";
    out += std::to_string(i);
    out += comma ? "=12,5MB;80;90;0;100" : "=12.5MB;80;90;0;100";
  }
  return out;
}

/**
 * @brief A logger that formats nothing.
 *
 * parse_perfdata logs one debug line per metric, and the fields it interpolates
 * would otherwise dominate the measurement. Level off, so spdlog gives up
 * before formatting -- which is also what happens in production, where the sql
 * logger sits at info or above.
 *
 * @return The logger.
 */
const std::shared_ptr<spdlog::logger>& silent_logger() {
  static std::shared_ptr<spdlog::logger> logger = [] {
    auto l = std::make_shared<spdlog::logger>(
        "perfdata_bench", std::make_shared<spdlog::sinks::null_sink_mt>());
    l->set_level(spdlog::level::off);
    return l;
  }();
  return logger;
}

/**
 * @brief The metric counts every shape is measured at.
 *
 * Dense below a hundred, which is where the output of a real plugin lives and
 * where a regression would go unnoticed, then spread out to three thousand,
 * where the shape of the curve shows. Fourteen points rather than a decade
 * scale: an exponent read off two or three measurements is a guess, and the
 * whole question here is whether the cost grows like N or like N squared.
 *
 * @param b The benchmark being configured.
 */
void sweep(benchmark::internal::Benchmark* b) {
  for (int count : {1, 10, 20, 30, 40, 50, 100, 200, 300, 400, 500, 1000, 2000,
                    3000})
    b->Arg(count);
}

}  // namespace

/**
 * @brief Parse an output of N metrics carrying no comma at all.
 *
 * @param state Benchmark state, holding N as its first argument.
 */
static void BM_NoComma(benchmark::State& state) {
  const size_t count = state.range(0);
  const std::string input = make_perfdata(count);
  for (auto _ : state) {
    auto result = perfdata::parse_perfdata(0, 0, input.c_str(),
                                           silent_logger());
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * count);
  state.SetBytesProcessed(state.iterations() * input.size());
}
BENCHMARK(BM_NoComma)->Apply(sweep);

/**
 * @brief Parse an output whose last value alone uses a decimal comma.
 *
 * @param state Benchmark state, holding N as its first argument.
 */
static void BM_TrailingComma(benchmark::State& state) {
  const size_t count = state.range(0);
  const std::string input = make_perfdata(count, true, false);
  for (auto _ : state) {
    auto result = perfdata::parse_perfdata(0, 0, input.c_str(),
                                           silent_logger());
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * count);
  state.SetBytesProcessed(state.iterations() * input.size());
}
BENCHMARK(BM_TrailingComma)->Apply(sweep);

/**
 * @brief Parse an output where every value uses a decimal comma.
 *
 * @param state Benchmark state, holding N as its first argument.
 */
static void BM_CommaEverywhere(benchmark::State& state) {
  const size_t count = state.range(0);
  const std::string input = make_perfdata(count, false, true);
  for (auto _ : state) {
    auto result = perfdata::parse_perfdata(0, 0, input.c_str(),
                                           silent_logger());
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * count);
  state.SetBytesProcessed(state.iterations() * input.size());
}
BENCHMARK(BM_CommaEverywhere)->Apply(sweep);

/**
 * @brief Check that the generated input is really parsed, then run everything.
 *
 * The check is not ceremony. parse_perfdata takes an error path on a metric it
 * cannot read -- a duplicate name, a missing value -- and that path is *faster*
 * than parsing: a generator producing something slightly wrong would be
 * measured as an excellent result. So every shape is parsed once, and its
 * metric count and first metric are verified, before anything is timed.
 *
 * @param argc Argument count, forwarded to google-benchmark.
 * @param argv Argument values, forwarded to google-benchmark.
 *
 * @return EXIT_SUCCESS, or 1 if the generated input does not parse as expected.
 */
int main(int argc, char** argv) {
  struct {
    const char* name;
    std::string input;
  } shapes[] = {
      {"NoComma", make_perfdata(3)},
      {"TrailingComma", make_perfdata(3, true, false)},
      {"CommaEverywhere", make_perfdata(3, false, true)},
  };

  for (const auto& shape : shapes) {
    auto parsed =
        perfdata::parse_perfdata(0, 0, shape.input.c_str(), silent_logger());
    if (parsed.size() != 3) {
      std::fprintf(stderr, "%s: %zu metrics parsed out of 3 in '%s'\n",
                   shape.name, parsed.size(), shape.input.c_str());
      return 1;
    }
    const perfdata& first = parsed.front();
    /* 12.5 either way: the comma shapes exist to make the parser work harder,
     * not to make it read something else. */
    if (first.name() != "metric_0" || first.value() != 12.5f ||
        first.unit() != "MB" || first.warning() != 80.0f ||
        first.critical() != 90.0f || first.min() != 0.0f ||
        first.max() != 100.0f) {
      std::fprintf(stderr,
                   "%s: first metric read as name='%s' value=%f unit='%s' "
                   "warning=%f critical=%f min=%f max=%f\n",
                   shape.name, first.name().c_str(), first.value(),
                   first.unit().c_str(), first.warning(), first.critical(),
                   first.min(), first.max());
      return 1;
    }
  }

  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
  return 0;
}
