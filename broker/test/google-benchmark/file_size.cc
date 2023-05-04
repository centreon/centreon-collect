#include <absl/strings/internal/ostringstream.h>
#include <absl/strings/numbers.h>
#include <benchmark/benchmark.h>
#include <fcntl.h>
#include <fmt/format.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

static void BM_ifstream(benchmark::State& state) {
  size_t length;
  for (auto _ : state) {
    std::ifstream in("/etc/centreon-broker/central-broker.json",
                     std::ifstream::ate | std::ifstream::binary);
    length = in.tellg();
  }
  std::cout << "length = " << length << std::endl;
}

// Register the function as a benchmark
BENCHMARK(BM_ifstream);

// Define another benchmark
static void BM_stat(benchmark::State& state) {
  size_t length;
  for (auto _ : state) {
    struct stat file_stat;
    stat("/etc/centreon-broker/central-broker.json", &file_stat);
    length = file_stat.st_size;
  }
  std::cout << "retval = " << length << std::endl;
}

BENCHMARK(BM_stat);

// Define another benchmark
static void BM_fseek(benchmark::State& state) {
  size_t length;
  for (auto _ : state) {
    FILE* fp = fopen("/etc/centreon-broker/central-broker.json", "rb");
    fseek(fp, 0, SEEK_END);
    length = ftell(fp);
    fclose(fp);
  }
  std::cout << "retval1 = " << length << std::endl;
}

BENCHMARK(BM_fseek);

// Define another benchmark
static void BM_lseek(benchmark::State& state) {
  size_t length;
  for (auto _ : state) {
    int fd = open("/etc/centreon-broker/central-broker.json", O_RDONLY);
    size_t current = lseek(fd, 0, SEEK_CUR);
    length = lseek(fd, 0, SEEK_END);
    close(fd);
  }
  std::cout << "retval2 = " << length << std::endl;
}

BENCHMARK(BM_lseek);
BENCHMARK_MAIN();
