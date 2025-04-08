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

using namespace com::centreon::agent::process;
using namespace com::centreon::agent;

/**
 * Given some filters rules we check container status and internal containers
 */
TEST(process_container, count) {
  process_container_mock cont(
      "exe in ('ok.exe', 'warning.exe', 'critical.exe')", "exe = 'exclude.exe'",
      " exe == 'warning.exe'  and creation > -1s",
      "exe =='critical.exe' and (time_percent>50% or state != 'started')",
      "warn_count > 0 ", "crit_count >= 2 ", 0, spdlog::default_logger());

  cont.add_process(1, "critical.exe");
  cont.refresh({1}, {1});

  EXPECT_EQ(cont.get_critical_processes().size(), 1);
  EXPECT_EQ(cont.check_container(), e_status::ok);

  cont.processes.clear();

  cont.add_process(1, "critical.exe", process_data::e_state::hung);
  cont.add_process(
      2, "critical.exe",
      mock_process_data::times{.creation_time = std::chrono::file_clock::now() -
                                                std::chrono::seconds(1),
                               .kernel_time = std::chrono::seconds(2)});
  cont.refresh({1, 2}, {1});
  EXPECT_EQ(cont.get_critical_processes().size(), 2);
  EXPECT_EQ(cont.check_container(), e_status::critical);
  cont.processes.clear();

  // two warning.exe but older than 1s
  cont.processes.clear();
  cont.add_process(
      1, "warning.exe",
      mock_process_data::times{.creation_time = std::chrono::file_clock::now() -
                                                std::chrono::seconds(2)});
  cont.add_process(
      2, "warning.exe",
      mock_process_data::times{.creation_time = std::chrono::file_clock::now() -
                                                std::chrono::seconds(2)});
  cont.refresh({1, 2}, {1});
  EXPECT_EQ(cont.get_ok_processes().size(), 2);
  EXPECT_EQ(cont.check_container(), e_status::ok);

  // two warning, but only one older than 1s
  cont.processes.clear();
  cont.add_process(1, "warning.exe",
                   mock_process_data::times{
                       .creation_time = std::chrono::file_clock::now()});
  cont.add_process(
      2, "warning.exe",
      mock_process_data::times{.creation_time = std::chrono::file_clock::now() -
                                                std::chrono::seconds(2)});
  cont.refresh({1, 2}, {1});
  EXPECT_EQ(cont.get_ok_processes().size(), 1);
  EXPECT_EQ(cont.get_warning_processes().size(), 1);
  EXPECT_EQ(cont.check_container(), e_status::warning);

  cont.processes.clear();

  cont.add_process(1, "critical.exe", 1, process_data::e_state::started);
  cont.add_process(
      2, "critical.exe", 2, process_data::e_state::started,
      mock_process_data::times{.creation_time = std::chrono::file_clock::now() -
                                                std::chrono::seconds(5),
                               .kernel_time = std::chrono::seconds(2),
                               .user_time = std::chrono::seconds(2)});
  cont.refresh({1, 2}, {1});
  EXPECT_EQ(cont.get_critical_processes().size(), 2);
  EXPECT_EQ(cont.check_container(), e_status::critical);
}
