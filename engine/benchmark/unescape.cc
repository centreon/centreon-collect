/**
 * Copyright 2025 Centreon (https://www.centreon.com/)
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

#include <benchmark/benchmark.h>
#include <absl/strings/escaping.h>
#include <absl/strings/str_replace.h>
#include <boost/algorithm/string/replace.hpp>
#include <cstring>
#include <string>
#include <string_view>

// ── implementations ────────────────────────────────────────────────────────

// Current production implementation (boost, 4 sequential passes)
static void unescape_boost(std::string& str) {
  boost::replace_all(str, "\\n", "\n");
  boost::replace_all(str, "\\r", "\r");
  boost::replace_all(str, "\\t", "\t");
  boost::replace_all(str, "\\\\", "\\");
}

// Original char* implementation (memmove-based, single pass)
static void unescape_char_ptr(char* buffer) {
  if (buffer == nullptr)
    return;
  char* read_pos = strchrnul(buffer, '\\');
  char* prev_read_pos = nullptr;
  while (*read_pos) {
    char c = read_pos[1];
    if (c == 'n' || c == 'r' || c == 't' || c == '\\') {
      if (prev_read_pos) {
        size_t len = read_pos - prev_read_pos;
        memmove(buffer, prev_read_pos, len);
        buffer += len;
      } else
        buffer = read_pos;
      prev_read_pos = read_pos + 2;
      switch (c) {
        case 'n': *buffer = '\n'; break;
        case 'r': *buffer = '\r'; break;
        case 't': *buffer = '\t'; break;
        case '\\': *buffer = '\\'; break;
      }
      ++buffer;
    } else if (read_pos[1] == 0)
      break;
    read_pos = strchrnul(read_pos + 2, '\\');
  }
  if (prev_read_pos) {
    size_t len = read_pos - prev_read_pos + 1;
    if (len) {
      memmove(buffer, prev_read_pos, len);
      buffer += len;
    }
    *buffer = 0;
  }
}

// Proposed C++ index-based implementation (single pass, no memmove)
static void unescape_cpp(std::string& str) {
  size_t read = str.find('\\');
  if (read == std::string::npos)
    return;

  size_t write = read;
  const size_t len = str.size();
  while (read < len) {
    if (str[read] != '\\' || read + 1 == len) {
      str[write++] = str[read++];
      continue;
    }
    switch (str[read + 1]) {
      case 'n':  str[write++] = '\n'; break;
      case 'r':  str[write++] = '\r'; break;
      case 't':  str[write++] = '\t'; break;
      case '\\': str[write++] = '\\'; break;
      default:
        str[write++] = '\\';
        str[write++] = str[read + 1];
        break;
    }
    read += 2;
  }
  str.resize(write);
}

// Proposed C++ v2: uses str.find() (memchr/SIMD) + memmove for bulk copies
static void unescape_cpp_v2(std::string& str) {
  size_t read = str.find('\\');
  if (read == std::string::npos)
    return;

  size_t write = read;
  const size_t len = str.size();
  while (read < len) {
    size_t next_bs = str.find('\\', read);
    if (next_bs == std::string::npos) {
      size_t seg = len - read;
      if (write != read)
        std::memmove(&str[write], &str[read], seg);
      write += seg;
      break;
    }
    size_t seg = next_bs - read;
    if (seg) {
      if (write != read)
        std::memmove(&str[write], &str[read], seg);
      write += seg;
    }
    read = next_bs;
    if (read + 1 == len) {
      str[write++] = '\\';
      ++read;
      break;
    }
    switch (str[read + 1]) {
      case 'n':  str[write++] = '\n'; break;
      case 'r':  str[write++] = '\r'; break;
      case 't':  str[write++] = '\t'; break;
      case '\\': str[write++] = '\\'; break;
      default:
        str[write++] = '\\';
        str[write++] = str[read + 1];
        break;
    }
    read += 2;
  }
  str.resize(write);
}

// Iterator-based version: same algorithm as cpp but using string iterators
static void unescape_cpp_iter(std::string& str) {
  auto read = std::find(str.begin(), str.end(), '\\');
  if (read == str.end())
    return;

  auto write = read;
  while (read != str.end()) {
    if (*read != '\\' || std::next(read) == str.end()) {
      *write++ = *read++;
      continue;
    }
    switch (*std::next(read)) {
      case 'n':  *write++ = '\n'; break;
      case 'r':  *write++ = '\r'; break;
      case 't':  *write++ = '\t'; break;
      case '\\': *write++ = '\\'; break;
      default:
        *write++ = *read;
        *write++ = *std::next(read);
        break;
    }
    std::advance(read, 2);
  }
  str.erase(write, str.end());
}

// Hybrid: str.find() for fast initial scan, then iterators in the loop
static void unescape_cpp_iter_v2(std::string& str) {
  size_t pos = str.find('\\');
  if (pos == std::string::npos)
    return;

  auto write = str.begin() + pos;
  auto read  = write;
  const auto end = str.end();

  while (read != end) {
    if (*read != '\\' || std::next(read) == end) {
      *write++ = *read++;
      continue;
    }
    switch (*std::next(read)) {
      case 'n':  *write++ = '\n'; break;
      case 'r':  *write++ = '\r'; break;
      case 't':  *write++ = '\t'; break;
      case '\\': *write++ = '\\'; break;
      default:
        *write++ = *read;
        *write++ = *std::next(read);
        break;
    }
    std::advance(read, 2);
  }
  str.erase(write, end);
}

// absl::StrReplaceAll: all patterns replaced simultaneously in one pass
static void unescape_absl_replace(std::string& str) {
  absl::StrReplaceAll({{"\\n", "\n"}, {"\\r", "\r"}, {"\\t", "\t"}, {"\\\\", "\\"}},
                      &str);
}

// absl::CUnescape: full C-style unescape into a new string (allocates)
static void unescape_absl_cunescape(std::string& str) {
  std::string out;
  absl::CUnescape(str, &out);
  str = std::move(out);
}

// ── inputs ─────────────────────────────────────────────────────────────────

// Typical plugin output: no backslashes at all (fast path)
static const std::string INPUT_NO_ESCAPE =
    "CHECK OK - load average: 0.12, 0.08, 0.05 | "
    "load1=0.12;5.00;10.00;0; load5=0.08;4.00;6.00;0; "
    "load15=0.05;3.00;5.00;0;";

// Typical connector output: a few \\n sequences
static const std::string INPUT_FEW_ESCAPES =
    "CHECK OK - rta=0.053ms;3000.000;5000.000;0;\\n"
    "pl=0%;80;100;0;100\\n"
    "rtmax=0.053ms;;;;\\n"
    "rtmin=0.053ms;;;;";

// Worst case: dense \\n every few chars
static const std::string INPUT_DENSE_ESCAPES = [] {
  std::string s;
  s.reserve(512);
  for (int i = 0; i < 40; ++i)
    s += "data\\n";
  return s;
}();

// ── benchmarks ─────────────────────────────────────────────────────────────

#define BENCH_ALL(suffix, input)                                              \
  static void BM_boost_##suffix(benchmark::State& state) {                   \
    for (auto _ : state) {                                                    \
      std::string s = input;                                                  \
      unescape_boost(s);                                                      \
      benchmark::DoNotOptimize(s);                                            \
    }                                                                         \
  }                                                                           \
  BENCHMARK(BM_boost_##suffix);                                               \
                                                                              \
  static void BM_char_ptr_##suffix(benchmark::State& state) {                \
    for (auto _ : state) {                                                    \
      std::string s = input;                                                  \
      unescape_char_ptr(s.data());                                            \
      benchmark::DoNotOptimize(s);                                            \
    }                                                                         \
  }                                                                           \
  BENCHMARK(BM_char_ptr_##suffix);                                            \
                                                                              \
  static void BM_cpp_##suffix(benchmark::State& state) {                     \
    for (auto _ : state) {                                                    \
      std::string s = input;                                                  \
      unescape_cpp(s);                                                        \
      benchmark::DoNotOptimize(s);                                            \
    }                                                                         \
  }                                                                           \
  BENCHMARK(BM_cpp_##suffix);                                                 \
                                                                              \
  static void BM_cpp_v2_##suffix(benchmark::State& state) {                  \
    for (auto _ : state) {                                                    \
      std::string s = input;                                                  \
      unescape_cpp_v2(s);                                                     \
      benchmark::DoNotOptimize(s);                                            \
    }                                                                         \
  }                                                                           \
  BENCHMARK(BM_cpp_v2_##suffix);                                               \
                                                                              \
  static void BM_absl_replace_##suffix(benchmark::State& state) {            \
    for (auto _ : state) {                                                    \
      std::string s = input;                                                  \
      unescape_absl_replace(s);                                               \
      benchmark::DoNotOptimize(s);                                            \
    }                                                                         \
  }                                                                           \
  BENCHMARK(BM_absl_replace_##suffix);                                         \
                                                                              \
  static void BM_cpp_iter_##suffix(benchmark::State& state) {                \
    for (auto _ : state) {                                                    \
      std::string s = input;                                                  \
      unescape_cpp_iter(s);                                                   \
      benchmark::DoNotOptimize(s);                                            \
    }                                                                         \
  }                                                                           \
  BENCHMARK(BM_cpp_iter_##suffix);                                             \
                                                                              \
  static void BM_cpp_iter_v2_##suffix(benchmark::State& state) {             \
    for (auto _ : state) {                                                    \
      std::string s = input;                                                  \
      unescape_cpp_iter_v2(s);                                                \
      benchmark::DoNotOptimize(s);                                            \
    }                                                                         \
  }                                                                           \
  BENCHMARK(BM_cpp_iter_v2_##suffix);                                        \
                                                                              \
  static void BM_absl_cunescape_##suffix(benchmark::State& state) {          \
    for (auto _ : state) {                                                    \
      std::string s = input;                                                  \
      unescape_absl_cunescape(s);                                             \
      benchmark::DoNotOptimize(s);                                            \
    }                                                                         \
  }                                                                           \
  BENCHMARK(BM_absl_cunescape_##suffix);

BENCH_ALL(no_escape, INPUT_NO_ESCAPE)
BENCH_ALL(few_escapes, INPUT_FEW_ESCAPES)
BENCH_ALL(dense_escapes, INPUT_DENSE_ESCAPES)

BENCHMARK_MAIN();
