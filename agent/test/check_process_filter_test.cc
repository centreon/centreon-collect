/**
 * Copyright 2025 Centreon
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

#include "agent/test/process_data_test.hh"
#include "filter.hh"
#include "process/process_filter.hh"

using namespace com::centreon::agent;
using namespace com::centreon::agent::process;

/**
 * @brief given incorrect string or labels, we expect an exception
 */
TEST(process_filter, incorrect_field_throw) {
  EXPECT_THROW(process_filter("pid", spdlog::default_logger()), std::exception);
  EXPECT_THROW(process_filter("ezrzrzr 7897#", spdlog::default_logger()),
               std::exception);
  EXPECT_THROW(process_filter("pidt=5", spdlog::default_logger()),
               std::exception);
  EXPECT_THROW(process_filter("pid=5 && exxe = 'ut_agent.exe'",
                              spdlog::default_logger()),
               std::exception);
  EXPECT_THROW(process_filter("pid=5 && exxe in ('ut_agent.exe')",
                              spdlog::default_logger()),
               std::exception);
}

TEST(process_filter, filter) {
  mock_process_data pd(
      55, process_data::started, "C:/Users/ut_agent.exe",
      mock_process_data::times{std::chrono::file_clock::now(),
                               {},
                               std::chrono::minutes(5),
                               std::chrono::minutes(10)},
      mock_process_data::handle_count{1, 5},
      PROCESS_MEMORY_COUNTERS_EX{.PageFaultCount = 2000,
                                 .PeakWorkingSetSize = 3000,
                                 .WorkingSetSize = 4000,
                                 .QuotaPeakPagedPoolUsage = 5000,
                                 .QuotaPagedPoolUsage = 6000,
                                 .QuotaPeakNonPagedPoolUsage = 7000,
                                 .QuotaNonPagedPoolUsage = 8000,
                                 .PagefileUsage = 9000,
                                 .PeakPagefileUsage = 10000,
                                 .PrivateUsage = 9000});

  process_filter pf(
      "pid=55 && exe='ut_agent.exe' && filename in ('C:/Users/ut_agent.exe') "
      "&& status == 'started' && "
      "creation > -1s && gdi_handles > 0 && gdi_handles <=1 && user_handles >= "
      "5 && "
      "handles == 6 && kernel > 4m && user > 9m && time=15m && page_fault == "
      "2000 && "
      "pagefile == 9000 && peak_pagefile==10000 && peak_virtual=10000 &&  "
      "peak_working_set == 3000 && working_set == 4000 && "
      "virtual == 9000",
      spdlog::default_logger());

  EXPECT_TRUE(pf.check(pd));
  std::this_thread::sleep_for(std::chrono::milliseconds(1500));
  EXPECT_FALSE(pf.check(pd));

  process_filter pf2(
      "pid=55 && exe='ut_agent.exe' && filename in ('C:/Users/ut_agent.exe') "
      "&& status == 'started' && "
      "creation > -1h && gdi_handles > 0 && gdi_handles <=1 && user_handles >= "
      "5 && "
      "handles == 6 && kernel > 4m && user > 9m && time=15m && page_fault == "
      "2000 && "
      "pagefile == 9000 && peak_pagefile==10000 && peak_virtual=10000 &&  "
      "peak_working_set == 3000 && working_set == 4000 && "
      "virtual == 9000",
      spdlog::default_logger());
  pd << 54;  // pid
  EXPECT_FALSE(pf2.check(pd));
  pd << 55;
  EXPECT_TRUE(pf2.check(pd));
  pd << "C:/Users/utttt_agent.exe";
  EXPECT_FALSE(pf2.check(pd));
  pd << "C:/Users/ut_agent.exe";
  EXPECT_TRUE(pf2.check(pd));
  pd << process_data::unreadable;
  EXPECT_FALSE(pf2.check(pd));
  pd << process_data::started;
  EXPECT_TRUE(pf2.check(pd));
  pd << mock_process_data::handle_count{2, 5};
  EXPECT_FALSE(pf2.check(pd));
  pd << mock_process_data::handle_count{1, 6};
  EXPECT_FALSE(pf2.check(pd));
  pd << mock_process_data::handle_count{1, 5};
  EXPECT_TRUE(pf2.check(pd));
  pd << mock_process_data::times{std::chrono::file_clock::now(),
                                 {},
                                 std::chrono::minutes(3),
                                 std::chrono::minutes(12)};
  EXPECT_FALSE(pf2.check(pd));
  pd << mock_process_data::times{std::chrono::file_clock::now(),
                                 {},
                                 std::chrono::minutes(6),
                                 std::chrono::minutes(9)};
  EXPECT_FALSE(pf2.check(pd));
  pd << mock_process_data::times{std::chrono::file_clock::now(),
                                 {},
                                 std::chrono::minutes(6),
                                 std::chrono::minutes(10)};
  EXPECT_FALSE(pf2.check(pd));
  pd << mock_process_data::times{std::chrono::file_clock::now(),
                                 {},
                                 std::chrono::minutes(5),
                                 std::chrono::minutes(10)};
  EXPECT_TRUE(pf2.check(pd));
  pd << PROCESS_MEMORY_COUNTERS_EX{.PageFaultCount = 2500,
                                   .PeakWorkingSetSize = 3000,
                                   .WorkingSetSize = 4000,
                                   .QuotaPeakPagedPoolUsage = 5000,
                                   .QuotaPagedPoolUsage = 6000,
                                   .QuotaPeakNonPagedPoolUsage = 7000,
                                   .QuotaNonPagedPoolUsage = 8000,
                                   .PagefileUsage = 9000,
                                   .PeakPagefileUsage = 10000,
                                   .PrivateUsage = 9000};
  EXPECT_FALSE(pf2.check(pd));
  pd << PROCESS_MEMORY_COUNTERS_EX{.PageFaultCount = 2000,
                                   .PeakWorkingSetSize = 3000,
                                   .WorkingSetSize = 4000,
                                   .QuotaPeakPagedPoolUsage = 5000,
                                   .QuotaPagedPoolUsage = 6000,
                                   .QuotaPeakNonPagedPoolUsage = 7000,
                                   .QuotaNonPagedPoolUsage = 8000,
                                   .PagefileUsage = 9000,
                                   .PeakPagefileUsage = 15000,
                                   .PrivateUsage = 9000};
  EXPECT_FALSE(pf2.check(pd));
  pd << PROCESS_MEMORY_COUNTERS_EX{.PageFaultCount = 2000,
                                   .PeakWorkingSetSize = 3500,
                                   .WorkingSetSize = 4000,
                                   .QuotaPeakPagedPoolUsage = 5000,
                                   .QuotaPagedPoolUsage = 6000,
                                   .QuotaPeakNonPagedPoolUsage = 7000,
                                   .QuotaNonPagedPoolUsage = 8000,
                                   .PagefileUsage = 9000,
                                   .PeakPagefileUsage = 10000,
                                   .PrivateUsage = 9000};
  EXPECT_FALSE(pf2.check(pd));
  pd << PROCESS_MEMORY_COUNTERS_EX{.PageFaultCount = 2000,
                                   .PeakWorkingSetSize = 3000,
                                   .WorkingSetSize = 4500,
                                   .QuotaPeakPagedPoolUsage = 5000,
                                   .QuotaPagedPoolUsage = 6000,
                                   .QuotaPeakNonPagedPoolUsage = 7000,
                                   .QuotaNonPagedPoolUsage = 8000,
                                   .PagefileUsage = 9000,
                                   .PeakPagefileUsage = 10000,
                                   .PrivateUsage = 9000};
  EXPECT_FALSE(pf2.check(pd));
  pd << PROCESS_MEMORY_COUNTERS_EX{.PageFaultCount = 2000,
                                   .PeakWorkingSetSize = 3000,
                                   .WorkingSetSize = 4000,
                                   .QuotaPeakPagedPoolUsage = 5000,
                                   .QuotaPagedPoolUsage = 6000,
                                   .QuotaPeakNonPagedPoolUsage = 7000,
                                   .QuotaNonPagedPoolUsage = 8000,
                                   .PagefileUsage = 9000,
                                   .PeakPagefileUsage = 10000,
                                   .PrivateUsage = 9000};
  EXPECT_TRUE(pf2.check(pd));
}