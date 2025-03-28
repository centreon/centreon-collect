/**
 * Copyright 2023-2024 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */

#include <gtest/gtest.h>

#include "process_stat.hh"

using namespace com::centreon::common;

TEST(process_stat, current_process) {
  process_stat current(getpid());

  ASSERT_GT(current.starttime(),
            std::chrono::system_clock::now() - std::chrono::minutes(10));
  ASSERT_LE(current.starttime(),
            std::chrono::system_clock::now() + std::chrono::minutes(1));

  ASSERT_EQ(current.cmdline().substr(0, 15), "tests/ut_common");
}

TEST(process_stat, current_heavy_process) {
  std::chrono::system_clock::time_point time_out =
      std::chrono::system_clock::now() + std::chrono::seconds(10);

  while (std::chrono::system_clock::now() < time_out) {
  }

  process_stat current(getpid());

  ASSERT_GE(current.utime(), std::chrono::seconds(9));
  ASSERT_LE(current.utime(), std::chrono::seconds(12));
}
