#include <absl/strings/numbers.h>
#include <absl/strings/internal/ostringstream.h>
#include <stdlib.h>
#include <iomanip>
#include <iostream>
#include <string>
#include <sstream>
#include <fmt/format.h>
#include <benchmark/benchmark.h>

static void BM_url_encode_with_stringstream(benchmark::State& state) {
  const char* str{"\%a-b_c.d~A-B_C.D~a- b_c.d~A-B_C.D~a -b_c.d~A-B_C.D~a-b_c.d~A-B_C.D~a-b_c.d~A-B_C.D~a-b_c.d~A-B_C.D~a-b_c.d~A-B_C.D~a-b_c.d~A-B_C.D~a-b_c.d~A-B_C.D~a-b_c.d~A-B_C.D~a-b_c.d~A-B_C.D~a-b_c.d~A-B_C.D~a-b_c.d~A-B_C.D~a-b_c.d~A-B_C.D~a-b_c.d~A-B_C.D~a-b_c.d~A-B_C.D~"};

  std::string retval;
  for (auto _ : state) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char const* cc = str; *cc; ++cc) {
      char c = *cc;
      // Keep alphanumeric and other accepted characters intact
      if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
        escaped << c;
        continue;
      }

      // Any other characters are percent-encoded
      escaped << std::uppercase;
      escaped << '%' << std::setw(2) << int((unsigned char)c);
      escaped << std::nouppercase;
    }
    retval = escaped.str();
  }
  std::cout << "retval = " << retval << std::endl;
}

// Register the function as a benchmark
BENCHMARK(BM_url_encode_with_stringstream);

// Define another benchmark
static void BM_url_encode_with_AbslStringstream(benchmark::State& state) {
  const char* str{"\%a-b_c.d~A-B_C.D~a- b_c.d~A-B_C.D~a -b_c.d~A-B_C.D~a-b_c.d~A-B_C.D~a-b_c.d~A-B_C.D~a-b_c.d~A-B_C.D~a-b_c.d~A-B_C.D~a-b_c.d~A-B_C.D~a-b_c.d~A-B_C.D~a-b_c.d~A-B_C.D~a-b_c.d~A-B_C.D~a-b_c.d~A-B_C.D~a-b_c.d~A-B_C.D~a-b_c.d~A-B_C.D~a-b_c.d~A-B_C.D~a-b_c.d~A-B_C.D~"};

  std::string retval;
  for (auto _ : state) {
    retval = "";
    absl::strings_internal::OStringStream escaped(&retval);
    escaped.fill('0');
    escaped << std::hex;

    for (char const* cc = str; *cc; ++cc) {
      char c = *cc;
      // Keep alphanumeric and other accepted characters intact
      if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
        escaped << c;
        continue;
      }

      // Any other characters are percent-encoded
      escaped << std::uppercase;
      escaped << '%' << std::setw(2) << int((unsigned char)c);
      escaped << std::nouppercase;
    }

  }
  std::cout << "retval = " << retval << std::endl;
}

BENCHMARK(BM_url_encode_with_AbslStringstream);

// Define another benchmark
static void BM_url_encode_without_stringstream(benchmark::State& state) {
  const char* str{"\%a-b_c.d~A-B_C.D~a- b_c.d~A-B_C.D~a -b_c.d~A-B_C.D~a-b_c.d~A-B_C.D~a-b_c.d~A-B_C.D~a-b_c.d~A-B_C.D~a-b_c.d~A-B_C.D~a-b_c.d~A-B_C.D~a-b_c.d~A-B_C.D~a-b_c.d~A-B_C.D~a-b_c.d~A-B_C.D~a-b_c.d~A-B_C.D~a-b_c.d~A-B_C.D~a-b_c.d~A-B_C.D~a-b_c.d~A-B_C.D~a-b_c.d~A-B_C.D~"};
  size_t len = strlen(str);

  std::string retval;
  for (auto _ : state) {
    retval = "";
    retval.reserve(len * 1.5);

    for (char const* cc = str; *cc; ++cc) {
      char c = *cc;
      // Keep alphanumeric and other accepted characters intact
      if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
        retval.push_back(c);
        continue;
      }

      // Any other characters are percent-encoded
      retval.append(fmt::format("\%{:02X}", int((unsigned char)c)));
    }
  }
  std::cout << "retval = " << retval << std::endl;
}

BENCHMARK(BM_url_encode_without_stringstream);

BENCHMARK_MAIN();
