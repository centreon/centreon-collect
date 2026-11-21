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
#include "common/engine_conf/timeperiod_legacy.hh"
#include "common/log_v2/log_v2.hh"
#include "common/timeperiods/timeperiod.hh"

using namespace com::centreon::broker;
using log_v2 = com::centreon::common::log_v2::log_v2;
namespace cfg = com::centreon::engine::configuration;

TEST(BamAvailabilityBuilder, Simple) {
  /* mon. 29 mars 2021 15:59:18 CEST */
  time_t end_time = 1617026358u;
  /* mon. 29 mars 2021 15:04:18 CEST */
  time_t start_time = 1617023058u;

  cfg::Timeperiod proto;
  proto.set_timeperiod_name("test_timeperiod");
  proto.set_alias("test_alias");
  for (int day = 0; day < 7; ++day)
    cfg::legacy_set_weekday(proto, day, "08:00-20:00");
  auto period =
      std::make_shared<com::centreon::common::timeperiods::timeperiod>(proto);
  ASSERT_TRUE(period->check_time_against_period(end_time));

  bam::availability_builder builder(end_time, start_time);
  ASSERT_EQ(builder.get_available(), 0);

  auto logger = log_v2::instance().get(log_v2::BAM);
  builder.add_event(0, start_time, end_time, false, period, logger);

  /* The availability here is the duration from start_time to end_time: 3300 */
  ASSERT_EQ(builder.get_available(), 3300);
}

// Same shape but on 2021-03-28, the spring-forward day in Europe/Paris. Both
// instants are after the 02:00->03:00 transition (CEST), inside the 08:00-20:00
// window, one hour apart: the availability is the full 3600 s. The shared
// library evaluates DST through Abseil, so this case (which the old broker
// timeperiod mishandled, hence it was disabled) now holds.
TEST(BamAvailabilityBuilder, SummerTime) {
  /* sun. 28 mars 2021 15:59:18 CEST */
  time_t end_time = 1616939958u;
  /* sun. 28 mars 2021 14:59:18 CEST */
  time_t start_time = 1616936358u;

  cfg::Timeperiod proto;
  proto.set_timeperiod_name("test_timeperiod");
  proto.set_alias("test_alias");
  for (int day = 0; day < 7; ++day)
    cfg::legacy_set_weekday(proto, day, "08:00-20:00");
  auto period =
      std::make_shared<com::centreon::common::timeperiods::timeperiod>(proto);
  ASSERT_TRUE(period->check_time_against_period(end_time));

  bam::availability_builder builder(end_time, start_time);
  ASSERT_EQ(builder.get_available(), 0);

  auto logger = log_v2::instance().get(log_v2::BAM);
  builder.add_event(0, start_time, end_time, false, period, logger);

  ASSERT_EQ(builder.get_available(), 3600);
}
