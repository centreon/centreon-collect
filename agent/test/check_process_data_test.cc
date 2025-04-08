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

#include "process/process_data.hh"

using namespace com::centreon::agent;
using namespace com::centreon::agent::process;

/**
 * @brief given current process, we expect correct values
 */
TEST(process_data, current_process) {
  auto rush_to = std::chrono::system_clock::now() + std::chrono::seconds(1);
  unsigned ii = 0;
  // consume some user time
  while (std::chrono::system_clock::now() < rush_to) {
    ++ii;
  }

  process_data pd(GetCurrentProcessId(),
                  process_field::exe_filename | process_field::times |
                      process_field::handle | process_field::memory,
                  spdlog::default_logger());

  EXPECT_EQ(pd.get_state(), process_data::started);
  EXPECT_EQ(pd.get_pid(), GetCurrentProcessId());
  EXPECT_EQ(pd.get_exe(), "ut_agent.exe");
  EXPECT_GT(pd.get_creation_time(),
            std::chrono::file_clock::now() - std::chrono::seconds(600));
  EXPECT_GE(pd.get_user_handle_count(), 1);
  EXPECT_GE(pd.get_kernel_time().count(), 0);
  EXPECT_GT(pd.get_user_time().count(), 0);
  EXPECT_GT(pd.get_memory_counters().PageFaultCount, 0);
  EXPECT_GT(pd.get_memory_counters().PeakWorkingSetSize, 0);

  EXPECT_GT(pd.get_memory_counters().WorkingSetSize, 0);
  EXPECT_GT(pd.get_memory_counters().QuotaPeakPagedPoolUsage, 0);
  EXPECT_GT(pd.get_memory_counters().QuotaPagedPoolUsage, 0);
  EXPECT_GT(pd.get_memory_counters().QuotaPeakNonPagedPoolUsage, 0);
  EXPECT_GT(pd.get_memory_counters().QuotaNonPagedPoolUsage, 0);
  EXPECT_GT(pd.get_memory_counters().PagefileUsage, 0);
  EXPECT_GT(pd.get_memory_counters().PeakPagefileUsage, 0);
  EXPECT_GT(pd.get_memory_counters().PrivateUsage, 0);
}
