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

/**
 * @brief What check_string_utf8() costs, and where.
 *
 * The production function validates its argument and, when it is already valid
 * UTF-8 -- the usual case -- returns a copy of it. The question this benchmark
 * answers is whether that copy is worth removing, which means measuring the two
 * halves separately:
 *
 *   validate_only  the scan alone, no copy: the floor of any implementation
 *   copy_only      std::string(str) alone: what a "no copy" variant would save
 *   current        the production function, validation + copy
 *   ascii_fastpath validation with a "no byte >= 0x80" preamble, + copy
 *
 * Keep in mind, when reading the numbers, that most call sites feed the result
 * straight into a protobuf setter, which takes it by rvalue and adopts the
 * buffer. There, the copy IS the message's string and removing it would only
 * move the allocation into SetBytes(). Only the sites that throw the result
 * away can actually save it.
 *
 * Measured on 2026-08-15 (medians of 10 runs, CV under 4%):
 *
 *                  validate_only  copy_only  current  ascii_fastpath
 *   ascii  (120B)       40.2         8.67     47.0        48.2
 *   utf8   (150B)       68.4        10.2      55.5        61.5
 *   latin1  (78B)       30.6        10.3      77.4       103
 *   short    (7B)        5.02        1.48      6.57        6.13
 *
 * Two conclusions, both negative, which is the point of keeping this file:
 *
 *   - the copy is 13 to 18% of the cost, and on the protobuf call sites it is
 *     not even wasted. Not worth an API change.
 *   - ascii_fastpath is SLOWER everywhere (+2.6% ascii, +11% utf8, +33%
 *     latin1): the compiler does not vectorize the high-bit scan any better
 *     than the existing loop, and the second pass costs more than it saves.
 *
 * One trap when reading these: validate_only is static here, so it gets inlined
 * into the benchmark loop while check_string_utf8() stays an out-of-line call.
 * On the utf8 input that inlining produces worse code, which is why
 * validate_only (68.4) comes out ABOVE current (55.5) even though it does
 * strictly less work. Use current - copy_only to estimate the validation share,
 * not validate_only.
 */

#include <benchmark/benchmark.h>

#include <cstdint>
#include <string>
#include <string_view>

#include "common/inc/com/centreon/common/utf8.hh"

using com::centreon::common::check_string_utf8;

// ── implementations ────────────────────────────────────────────────────────

/* The validation loop of common/src/utf8.cc, lifted as-is and stopped before
 * the copy: this is what the production function spends before deciding the
 * string is fine. */
static bool validate_only(std::string_view str) {
  std::string_view::const_iterator it;
  for (it = str.begin(); it < str.end();) {
    uint32_t val = (*it & 0xff);
    if ((val & 0x80) == 0) {
      ++it;
      continue;
    }
    if (it + 1 >= str.end())
      break;
    val = (val << 8) | (*(it + 1) & 0xff);
    if ((val & 0xe0c0) == 0xc080) {
      val &= 0x1e00;
      if (val == 0)
        break;
      it += 2;
      continue;
    }
    if (it + 2 >= str.end())
      break;
    val = (val << 8) | (*(it + 2) & 0xff);
    if ((val & 0xf0c0c0) == 0xe08080) {
      val &= 0xf2000;
      if (val == 0 || val == 0xd2000)
        break;
      it += 3;
      continue;
    }
    if (it + 3 >= str.end())
      break;
    val = (val << 8) | (*(it + 3) & 0xff);
    if ((val & 0xf8c0c0c0) == 0xF0808080) {
      val &= 0x7300000;
      if (val == 0 || val > 0x4000000)
        break;
      it += 4;
      continue;
    }
    break;
  }
  return it == str.end();
}

/* Pure ASCII is by far the most common case for a plugin output. memchr-like
 * scanning for a byte with the high bit set is vectorized by the compiler,
 * where the loop above advances one code point at a time. */
static bool is_ascii(std::string_view str) {
  for (unsigned char c : str)
    if (c & 0x80)
      return false;
  return true;
}

static std::string ascii_fastpath(std::string_view str) {
  if (is_ascii(str))
    return std::string(str);
  return check_string_utf8(str);
}

// ── inputs ─────────────────────────────────────────────────────────────────

/* Typical plugin output: pure ASCII, with its perfdata. */
static const std::string INPUT_ASCII =
    "CHECK OK - load average: 0.12, 0.08, 0.05 | "
    "load1=0.12;5.00;10.00;0; load5=0.08;4.00;6.00;0; "
    "load15=0.05;3.00;5.00;0;";

/* Valid UTF-8 carrying a few accented characters, as a French message would. */
static const std::string INPUT_UTF8 =
    "CHECK OK - la sonde répond en 12ms, température relevée à 21°C, "
    "état nominal | temp=21;30;40;0;100 délai=12ms;;;;";

/* Latin-1 / cp1252, the case the function actually has to convert. The
 * offending bytes are late in the string, so the scan runs almost to the end
 * before giving up. */
static const std::string INPUT_LATIN1 = [] {
  std::string s =
      "CHECK WARNING - la sonde repond en 12ms, temperature relevee a ";
  s += static_cast<char>(0xb0);  // ° in latin-1
  s += "C, etat degrade";
  return s;
}();

/* A short field, as most configuration strings are (host name, command name).
 * These go through check_string_utf8() too, on every event carrying them. */
static const std::string INPUT_SHORT = "host_51";

// ── benchmarks ─────────────────────────────────────────────────────────────

#define BENCH_ALL(suffix, input)                                     \
  static void BM_validate_only_##suffix(benchmark::State& state) {   \
    for (auto _ : state) {                                           \
      bool ok = validate_only(input);                                \
      benchmark::DoNotOptimize(ok);                                  \
    }                                                                \
  }                                                                  \
  BENCHMARK(BM_validate_only_##suffix);                              \
                                                                     \
  static void BM_copy_only_##suffix(benchmark::State& state) {       \
    for (auto _ : state) {                                           \
      std::string s{input};                                          \
      benchmark::DoNotOptimize(s);                                   \
    }                                                                \
  }                                                                  \
  BENCHMARK(BM_copy_only_##suffix);                                  \
                                                                     \
  static void BM_current_##suffix(benchmark::State& state) {         \
    for (auto _ : state) {                                           \
      std::string s = check_string_utf8(input);                      \
      benchmark::DoNotOptimize(s);                                   \
    }                                                                \
  }                                                                  \
  BENCHMARK(BM_current_##suffix);                                    \
                                                                     \
  static void BM_ascii_fastpath_##suffix(benchmark::State& state) {  \
    for (auto _ : state) {                                           \
      std::string s = ascii_fastpath(input);                         \
      benchmark::DoNotOptimize(s);                                   \
    }                                                                \
  }                                                                  \
  BENCHMARK(BM_ascii_fastpath_##suffix);

BENCH_ALL(ascii, INPUT_ASCII)
BENCH_ALL(utf8, INPUT_UTF8)
BENCH_ALL(latin1, INPUT_LATIN1)
BENCH_ALL(short_field, INPUT_SHORT)

BENCHMARK_MAIN();
