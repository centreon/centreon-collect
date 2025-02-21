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

#include "check_event_log_container.hh"

using namespace com::centreon::agent;
using namespace com::centreon::agent::check_event_log_detail;

TEST(eventlog_data, event_log_event) {
  event_container cont("System", "${log}-${source}-${id}",
                       "level in ('error', 'warning')", "level == 'warning'",
                       "level == 'critical'", std::chrono::days(2),
                       spdlog::default_logger());

  cont.start();

  for (int ii = 0; ii < 10; ++ii) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::lock_guard l(cont);
    if (!cont.get_warning().empty() || !cont.get_critical().empty()) {
      break;
    }
  }

  std::lock_guard l(cont);
  EXPECT_FALSE(cont.get_warning().empty() && cont.get_critical().empty());

  for (const auto& evt : cont.get_warning()) {
    std::cout << evt.first << std::endl;
  }
}
