#include <benchmark/benchmark.h>
#include <stdlib.h>
#include <iostream>
#include <absl/strings/str_replace.h>
#include <boost/algorithm/string/replace.hpp>

static void BM_absl_str_repl(benchmark::State& state) {
  for (auto _ : state) {
    std::string toto("aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::");
    auto retval = absl::StrReplaceAll(toto, {{"::", ":"}});
  }
}

static void BM_replace_all_copy(benchmark::State& state) {
  for (auto _ : state) {
    std::string toto("aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::aa::zz::ee::rr::tt::yy::uu::ii::oo::");
    auto retval = boost::replace_all_copy(toto, "::", ":");
  }
}

BENCHMARK(BM_absl_str_repl);

BENCHMARK(BM_replace_all_copy);

BENCHMARK_MAIN();
