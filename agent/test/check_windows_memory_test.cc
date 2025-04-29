/**
 * Copyright 2024 Centreon
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

#include <gtest/gtest.h>

#include <psapi.h>

#include "com/centreon/common/rapidjson_helper.hh"

#include "check_memory.hh"

extern std::shared_ptr<asio::io_context> g_io_context;

using namespace com::centreon::agent;
using namespace com::centreon::agent::native_check_detail;

using namespace std::string_literals;

class test_check : public check_memory {
 public:
  static MEMORYSTATUSEX mock;
  static PERFORMANCE_INFORMATION perf_mock;

  test_check(const rapidjson::Value& args)
      : check_memory(
            g_io_context,
            spdlog::default_logger(),
            {},
            {},
            {},
            "serv"s,
            "cmd_name"s,
            "cmd_line"s,
            args,
            nullptr,
            [](const std::shared_ptr<check>& caller,
               int status,
               const std::list<com::centreon::common::perfdata>& perfdata,
               const std::list<std::string>& outputs) {},
            std::make_shared<checks_statistics>()) {}

  std::shared_ptr<native_check_detail::snapshot<
      native_check_detail::e_memory_metric::nb_metric>>
  measure() override {
    return std::make_shared<native_check_detail::w_memory_info>(mock, perf_mock,
                                                                _output_flags);
  }
};

MEMORYSTATUSEX test_check::mock = {
    0,
    0,
    16ull * 1024 * 1024 * 1024,   // ullTotalPhys
    7ull * 1024 * 1024,           // ullAvailPhys
    24ull * 1024 * 1024 * 1024,   // ullTotalPageFile
    6ull * 1024 * 1024 * 1024,    // ullAvailPageFile
    100ull * 1024 * 1024 * 1024,  // ullTotalVirtual
    40ull * 1024 * 1024 * 1024};  // ullAvailVirtual

PERFORMANCE_INFORMATION test_check::perf_mock = {
    0,                 // cb
    5 * 1024 * 1024,   // CommitTotal
    15 * 1024 * 1024,  // CommitLimit
    0,                 // CommitPeak
    4194304,           // PhysicalTotal
    1792,              // PhysicalAvailable
    0,                 // SystemCache
    0,                 // KernelTotal
    0,                 // KernelPaged
    0,                 // KernelNonpaged
    4096,              // PageSize
    0,                 // HandleCount
    0,                 // ProcessCount
    0,                 // ThreadCount
};

const uint64_t _total_phys = test_check::mock.ullTotalPhys;
const uint64_t _available_phys = test_check::mock.ullAvailPhys;
const uint64_t _total_swap =
    (test_check::perf_mock.CommitLimit - test_check::perf_mock.PhysicalTotal) *
    test_check::perf_mock.PageSize;
const uint64_t _used_swap = (test_check::perf_mock.CommitTotal +
                             test_check::perf_mock.PhysicalAvailable -
                             test_check::perf_mock.PhysicalTotal) *
                            test_check::perf_mock.PageSize;

const uint64_t _total_virtual = test_check::mock.ullTotalPageFile;
const uint64_t _available_virtual = test_check::mock.ullAvailPageFile;

static void test_perfs(std::list<com::centreon::common::perfdata> perfs) {
  ASSERT_EQ(perfs.size(), 9);
  for (const auto& perf : perfs) {
    ASSERT_EQ(perf.min(), 0);
    if (perf.name() == "memory.usage.bytes") {
      ASSERT_EQ(perf.value(), _total_phys - _available_phys);
      ASSERT_EQ(perf.max(), _total_phys);
      ASSERT_EQ(perf.unit(), "B");
    } else if (perf.name() == "memory.free.bytes") {
      ASSERT_EQ(perf.value(), _available_phys);
      ASSERT_EQ(perf.max(), _total_phys);
      ASSERT_EQ(perf.unit(), "B");
    } else if (perf.name() == "memory.usage.percentage") {
      ASSERT_NEAR(perf.value(),
                  (_total_phys - _available_phys) * 100.0 / _total_phys, 0.01);
      ASSERT_EQ(perf.max(), 100);
      ASSERT_EQ(perf.unit(), "%");
    } else if (perf.name() == "swap.free.bytes") {
      ASSERT_EQ(perf.max(), _total_swap);
      ASSERT_EQ(perf.value(), _total_swap - _used_swap);
      ASSERT_EQ(perf.unit(), "B");
    } else if (perf.name() == "swap.usage.bytes") {
      ASSERT_EQ(perf.max(), _total_swap);
      ASSERT_EQ(perf.value(), _used_swap);
      ASSERT_EQ(perf.unit(), "B");
    } else if (perf.name() == "swap.usage.percentage") {
      ASSERT_NEAR(perf.value(), _used_swap * 100.0 / _total_swap, 0.01);
      ASSERT_EQ(perf.max(), 100);
      ASSERT_EQ(perf.unit(), "%");
    } else if (perf.name() == "virtual-memory.usage.bytes") {
      ASSERT_EQ(perf.max(), _total_virtual);
      ASSERT_EQ(perf.value(), (_total_virtual - _available_virtual));
      ASSERT_EQ(perf.unit(), "B");
    } else if (perf.name() == "virtual-memory.free.bytes") {
      ASSERT_EQ(perf.max(), _total_virtual);
      ASSERT_EQ(perf.value(), _available_virtual);
      ASSERT_EQ(perf.unit(), "B");
    } else if (perf.name() == "virtual-memory.usage.percentage") {
      ASSERT_EQ(perf.value(),
                (_total_virtual - _available_virtual) * 100.0 / _total_virtual);
      ASSERT_EQ(perf.max(), 100);
      ASSERT_EQ(perf.unit(), "%");
    } else {
      FAIL() << "unexpected perfdata name:" << perf.name();
    }
  }
}

TEST(native_check_memory_windows, output_no_threshold) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args = R"({"swap": ""})"_json;
  test_check to_check(check_args);
  std::string output;
  std::list<com::centreon::common::perfdata> perfs;

  com::centreon::agent::e_status status =
      to_check.compute(*to_check.measure(), &output, &perfs);

  ASSERT_EQ(output,
            "OK: Ram total: 16 GB, used (-buffers/cache): 15.99 GB (99.96%), "
            "free: 7 MB (0.04%)");
  test_perfs(perfs);
}

TEST(native_check_memory_windows, output_no_threshold2) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args = R"({ "swap": true})"_json;
  test_check to_check(check_args);
  std::string output;
  std::list<com::centreon::common::perfdata> perfs;

  com::centreon::agent::e_status status =
      to_check.compute(*to_check.measure(), &output, &perfs);

  ASSERT_EQ(output,
            "OK: Ram total: 16 GB, used (-buffers/cache): 15.99 GB (99.96%), "
            "free: 7 MB (0.04%) "
            "Swap total: 44 GB, used: 4 GB (9.11%), free: 39.99 GB (90.89%)");
  test_perfs(perfs);
}

TEST(native_check_memory_windows, output_no_threshold3) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args = R"({ "swap": true, "virtual": "true"})"_json;
  test_check to_check(check_args);
  std::string output;
  std::list<com::centreon::common::perfdata> perfs;

  com::centreon::agent::e_status status =
      to_check.compute(*to_check.measure(), &output, &perfs);

  ASSERT_EQ(output,
            "OK: Ram total: 16 GB, used (-buffers/cache): 15.99 GB (99.96%), "
            "free: 7 MB (0.04%) "
            "Swap total: 44 GB, used: 4 GB (9.11%), free: 39.99 GB (90.89%) "
            "Virtual total: 24 GB, used: 18 GB (75.00%), free: 6 GB (25.00%)");
  test_perfs(perfs);
}

TEST(native_check_memory_windows, output_threshold) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({ "warning-usage-free": "8388609", "critical-usage-prct": 99.99, "warning-virtual": "20000000000", "critical-virtual": 50000000000 })"_json;
  test_check to_check(check_args);
  std::string output;
  std::list<com::centreon::common::perfdata> perfs;

  com::centreon::agent::e_status status =
      to_check.compute(*to_check.measure(), &output, &perfs);

  ASSERT_EQ(
      output,
      "WARNING: Ram total: 16 GB, used (-buffers/cache): 15.99 GB (99.96%), "
      "free: 7 MB (0.04%)");
  test_perfs(perfs);
  for (const auto& perf : perfs) {
    if (perf.name() == "memory.free.bytes") {
      ASSERT_EQ(perf.warning_low(), 0);
      ASSERT_EQ(perf.warning(), 8388609);
    } else if (perf.name() == "memory.usage.percentage") {
      ASSERT_EQ(perf.critical_low(), 0);
      ASSERT_NEAR(perf.critical(), 99.99, 0.01);
    } else if (perf.name() == "virtual-memory.usage.bytes") {
      ASSERT_EQ(perf.warning_low(), 0);
      ASSERT_EQ(perf.warning(), 20000000000);
      ASSERT_EQ(perf.critical_low(), 0);
      ASSERT_EQ(perf.critical(), 50000000000);
    }
  }
}

TEST(native_check_memory_windows, output_threshold_2) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({ "warning-usage-prct": "1", "critical-usage-prct": "99.5",  "warning-usage-free": "" })"_json;
  test_check to_check(check_args);
  std::string output;
  std::list<com::centreon::common::perfdata> perfs;

  com::centreon::agent::e_status status =
      to_check.compute(*to_check.measure(), &output, &perfs);

  ASSERT_EQ(
      output,
      "CRITICAL: Ram total: 16 GB, used (-buffers/cache): 15.99 GB (99.96%), "
      "free: 7 MB (0.04%)");
  test_perfs(perfs);
  for (const auto& perf : perfs) {
    if (perf.name() == "memory.usage.percentage") {
      ASSERT_EQ(perf.warning_low(), 0);
      ASSERT_NEAR(perf.warning(), 1, 0.01);
    } else if (perf.name() == "memory.usage.percentage") {
      ASSERT_EQ(perf.critical_low(), 0);
      ASSERT_NEAR(perf.critical(), 99.5, 0.01);
    }
  }
}
