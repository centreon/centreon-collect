#include <absl/strings/numbers.h>
#include <benchmark/benchmark.h>
#include <stdlib.h>
#include <boost/lexical_cast.hpp>
#include <sstream>
#include <string>

static void BM_atoi(benchmark::State& state) {
  std::string a{"12345678"};
  const char* ca = a.c_str();
  for (auto _ : state)
    int aa = atoi(ca);
}
// Register the function as a benchmark
BENCHMARK(BM_atoi);

// Define another benchmark
static void BM_stoi(benchmark::State& state) {
  std::string a{"12345678"};
  const char* ca = a.c_str();
  for (auto _ : state)
    int aa = std::stoi(ca, nullptr);
}

BENCHMARK(BM_stoi);

// Define another benchmark
static void BM_SimpleAtoi(benchmark::State& state) {
  std::string a{"12345678"};
  for (auto _ : state) {
    int aa;
    absl::SimpleAtoi(a, &aa);
  }
}
BENCHMARK(BM_SimpleAtoi);

// Define another benchmark
static void BM_StringStream(benchmark::State& state) {
  std::string a{"12345678"};
  for (auto _ : state) {
    std::stringstream ss;
    ss << a;
    int aa;
    ss >> aa;
  }
}
BENCHMARK(BM_StringStream);

static void BM_boost(benchmark::State& state) {
  std::string a{"12345678"};
  for (auto _ : state) {
    boost::lexical_cast<int>(a);
  }
}
BENCHMARK(BM_boost);

BENCHMARK_MAIN();
