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

#include "agent/test/check_event_log_data_test.hh"

using namespace com::centreon::agent;
using namespace com::centreon::agent::event_log;

TEST(eventlog_data, event_log_event) {
  test_event_container cont(
      "System", "level in ('info', 'error', 'warning')", "level == 'warning'",
      "level == 'error'", std::chrono::days(4), true, spdlog::default_logger());

  cont.start();

  for (int ii = 0; ii < 10; ++ii) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::lock_guard l(cont);
    if (!cont.get_warning().empty() || !cont.get_critical().empty()) {
      break;
    }
  }

  std::lock_guard l(cont);
  EXPECT_NE(cont.get_nb_warning() + cont.get_nb_critical() + cont.get_nb_ok(),
            0);
}

TEST(eventlog_data, filters) {
  test_event_container cont(
      "System", "event_id = 3 or event_id = 4",
      "(level == 'warning' || level == 'error') and event_id = 4",
      "(level == 'error' || level == 'critical')  && written > -60m",
      std::chrono::days(2), true, spdlog::default_logger());

  EXPECT_EQ(cont.get_nb_warning() + cont.get_nb_critical() + cont.get_nb_ok(),
            0);

  mock_event_data raw_data;
  raw_data.event_id = 3;
  raw_data.level = 3;  // warning
  raw_data.time_created = 0;

  cont.on_event(raw_data, nullptr);
  EXPECT_EQ(cont.get_nb_warning() + cont.get_nb_critical() + cont.get_nb_ok(),
            0);

  raw_data.set_time_created(86400 * 2 + 1);
  cont.on_event(raw_data, nullptr);
  EXPECT_EQ(cont.get_nb_warning() + cont.get_nb_critical() + cont.get_nb_ok(),
            0);

  raw_data.set_time_created(86400 * 2 - 10);
  cont.on_event(raw_data, nullptr);
  EXPECT_EQ(cont.get_nb_warning() + cont.get_nb_critical(), 0);
  EXPECT_EQ(cont.get_nb_ok(), 1);

  raw_data.event_id = 4;
  raw_data.set_time_created(86400 * 2 - 10);
  cont.on_event(raw_data, nullptr);
  EXPECT_EQ(cont.get_nb_warning(), 1);
  EXPECT_EQ(cont.get_nb_critical(), 0);
  EXPECT_EQ(cont.get_nb_ok(), 1);

  raw_data.level = 2;  // error
  raw_data.set_time_created(86400 * 2 - 10);
  cont.on_event(raw_data, nullptr);
  EXPECT_EQ(cont.get_nb_warning(), 2);
  EXPECT_EQ(cont.get_nb_critical(), 0);
  EXPECT_EQ(cont.get_nb_ok(), 1);

  raw_data.set_time_created(3600 - 1);
  cont.on_event(raw_data, nullptr);
  EXPECT_EQ(cont.get_nb_warning(), 2);
  EXPECT_EQ(cont.get_nb_critical(), 1);
  EXPECT_EQ(cont.get_nb_ok(), 1);

  std::this_thread::sleep_for(
      std::chrono::seconds(2));  // critical event must be perempted
  cont.clean_perempted_events(true);
  EXPECT_EQ(cont.get_nb_warning(), 3);
  EXPECT_EQ(cont.get_nb_critical(), 0);
  EXPECT_EQ(cont.get_nb_ok(), 1);

  cont.on_event(raw_data, nullptr);
  EXPECT_EQ(cont.get_nb_warning(), 4);
  EXPECT_EQ(cont.get_nb_critical(), 0);
  EXPECT_EQ(cont.get_nb_ok(), 1);

  raw_data.event_id = 3;
  cont.on_event(raw_data, nullptr);
  EXPECT_EQ(cont.get_nb_warning(), 4);
  EXPECT_EQ(cont.get_nb_critical(), 0);
  EXPECT_EQ(cont.get_nb_ok(), 2);

  raw_data.set_time_created(3600 - 1);
  cont.on_event(raw_data, nullptr);
  EXPECT_EQ(cont.get_nb_warning(), 4);
  EXPECT_EQ(cont.get_nb_critical(), 1);
  EXPECT_EQ(cont.get_nb_ok(), 2);

  std::this_thread::sleep_for(
      std::chrono::seconds(2));  // critical event must be perempted
  cont.clean_perempted_events(true);
  EXPECT_EQ(cont.get_nb_warning(), 4);
  EXPECT_EQ(cont.get_nb_critical(), 0);
  EXPECT_EQ(cont.get_nb_ok(), 3);
}