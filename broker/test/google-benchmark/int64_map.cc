#include <absl/container/flat_hash_map.h>
#include <benchmark/benchmark.h>
#include <stdlib.h>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <typeinfo>
#include <unordered_map>
#include <vector>

#include <absl/container/btree_map.h>
#include <absl/container/flat_hash_map.h>
#include <boost/container/flat_map.hpp>

struct last_call {
  using pointer = std::shared_ptr<last_call>;
  time_t launch_time;
  std::shared_ptr<int> res;

  last_call() : launch_time(0) {}
};

template <class container>
static void BM_ordered_insert_and_random_erase(benchmark::State& state) {
  container cont;
  std::cout << "sizeof(" << typeid(cont).name() << ")=" << sizeof(cont)
            << std::endl;
  srand(time(nullptr));
  for (auto _ : state) {
    for (uint64_t key = 0; key < 100000; ++key) {
      cont[key] = std::make_shared<last_call>();
      if (cont.size() >= 10) {
        auto iter_to_erase = cont.begin();
        std::advance(iter_to_erase, rand() % 10);
        cont.erase(iter_to_erase);
      }
    }
  }
}

template <class container>
static void BM_random_search(benchmark::State& state) {
  container cont;
  srand(time(nullptr));
  for (int ii = 0; ii < 100; ++ii) {
    cont.emplace(ii, std::make_shared<last_call>());
  }
  for (auto _ : state) {
    for (uint64_t key = 0; key < 100000; ++key) {
      static auto search __attribute__((used)) = cont.find(rand() % 100);
    }
  }
}

BENCHMARK(
    BM_ordered_insert_and_random_erase<std::map<uint64_t, last_call::pointer>>);

BENCHMARK(BM_ordered_insert_and_random_erase<
          std::unordered_map<uint64_t, last_call::pointer>>);

BENCHMARK(BM_ordered_insert_and_random_erase<
          absl::flat_hash_map<uint64_t, last_call::pointer>>);

BENCHMARK(BM_ordered_insert_and_random_erase<
          absl::btree_map<uint64_t, last_call::pointer>>);

BENCHMARK(BM_ordered_insert_and_random_erase<boost::container::flat_map<
              uint64_t,
              last_call::pointer,
              std::less<uint64_t>,
              std::vector<std::pair<uint64_t, last_call::pointer>>>>);

BENCHMARK(BM_random_search<std::map<uint64_t, last_call::pointer>>);

BENCHMARK(BM_random_search<std::unordered_map<uint64_t, last_call::pointer>>);

BENCHMARK(BM_random_search<absl::flat_hash_map<uint64_t, last_call::pointer>>);

BENCHMARK(BM_random_search<absl::btree_map<uint64_t, last_call::pointer>>);

BENCHMARK(BM_random_search<boost::container::flat_map<
              uint64_t,
              last_call::pointer,
              std::less<uint64_t>,
              std::vector<std::pair<uint64_t, last_call::pointer>>>>);

BENCHMARK_MAIN();
