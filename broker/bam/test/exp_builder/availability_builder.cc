/**
 * Copyright 2021 Centreon
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
#include "com/centreon/broker/bam/availability_builder.hh"
#include <gtest/gtest.h>
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using log_v2 = com::centreon::common::log_v2::log_v2;

TEST(BamAvailabilityBuilder, Simple) {
  /* mon. 29 mars 2021 15:59:18 CEST */
  time_t end_time = 1617026358u;
  /* mon. 29 mars 2021 15:04:18 CEST */
  time_t start_time = 1617023058u;

  time::timeperiod::ptr period = std::make_shared<time::timeperiod>(
      4, "test_timeperiod", "test_alias", "08:00-20:00", "08:00-20:00",
      "08:00-20:00", "08:00-20:00", "08:00-20:00", "08:00-20:00",
      "08:00-20:00");
  ASSERT_TRUE(period->is_valid(end_time));

  bam::availability_builder builder(end_time, start_time);
  ASSERT_EQ(builder.get_available(), 0);

  auto logger = log_v2::instance().get(log_v2::BAM);
  builder.add_event(0, start_time, end_time, false, period, logger);

  /* The availability here is the duration from start_time to end_time: 3300 */
  ASSERT_EQ(builder.get_available(), 3300);
}

// TEST(BamAvailabilityBuilder, SummerTime) {
//  /* sun. 27 mars 2021 15:59:18 CEST */
//  time_t end_time = 1616939958u;
//  /* sun. 28 mars 2021 14:59:18 CEST */
//  time_t start_time = 1616936358u;
//
//  time::timeperiod::ptr period{std::make_shared<time::timeperiod>(
//      4, "test_timeperiod", "test_alias", "08:00-20:00", "08:00-20:00",
//      "08:00-20:00", "08:00-20:00", "08:00-20:00", "08:00-20:00",
//      "08:00-20:00")};
//  ASSERT_TRUE(period->is_valid(end_time));
//
//  bam::availability_builder builder(end_time, start_time);
//  ASSERT_EQ(builder.get_available(), 0);
//
//  builder.add_event(0, start_time, end_time, false, period);
//
//  /* The availability here is the duration from start_time to end_time: 3300
//  */ ASSERT_EQ(builder.get_available(), 3600);
//}
