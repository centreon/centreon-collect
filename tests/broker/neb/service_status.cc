/*
 * Copyright 2011 - 2019 Centreon (https://www.centreon.com/)
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
#include "com/centreon/broker/neb/service_status.hh"
#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include "randomize.hh"

using namespace com::centreon::broker;

class ServiceStatus : public ::testing::Test {
  void SetUp() override { randomize_init(); };

  void TearDown() override { randomize_cleanup(); };
};

/**
 *  Check service_status' assignment operator.
 *
 *  @return EXIT_SUCCESS on success.
 */
TEST_F(ServiceStatus, Assign) {
  // Object #1.
  neb::service_status ss1;
  std::vector<randval> randvals1;
  randomize(ss1, &randvals1);

  // Object #2.
  neb::service_status ss2;
  randomize(ss2);

  // Assignment.
  ss2 = ss1;

  // Reset object #1.
  std::vector<randval> randvals2;
  randomize(ss1, &randvals2);

  ASSERT_FALSE(ss1 != randvals2);
  ASSERT_FALSE(ss2 != randvals1);
}

/**
 *  Check service_status' copy constructor.
 *
 *  @return EXIT_SUCCESS on success.
 */
TEST_F(ServiceStatus, CopyCtor) {
  // Object #1.
  neb::service_status ss1;
  std::vector<randval> randvals1;
  randomize(ss1, &randvals1);

  // Object #2.
  neb::service_status ss2(ss1);

  // Reset object #1.
  std::vector<randval> randvals2;
  randomize(ss1, &randvals2);

  // Compare objects with expected results.
  ASSERT_FALSE(ss1 != randvals2);
  ASSERT_FALSE(ss2 != randvals1);
}

/**
 *  Check service_status' constructor with parameters.
 */
TEST_F(ServiceStatus, ParamCtor) {
  neb::service_status ss("host_name", // host_name,
                         true,        // acknowledged,
                         2,           // acknowledgement_type,
                         false,       // active_checks_enabled,
                         "check_cmd", // check_command
                         0.5,         // check_interval,
                         "chk_priod", // check_period
                         7,           // check_type,
                         8,           // current_check_attempt,
                         9,           // current_state,
                         10,           // downtime_depth,
                         "evnt_hdlr", // event_handler,
                         "true",      // event_handler_enabled
                         1.3,         // execution_time,
                         true,        // flap_detection_enabled,
                         false,       // has_been_checked,
                         16,          // host_id,
                         true,        // is_flapping,
                         18,          // last_check
                         19,          // last_hard_state
                         20,          // last_hard_state_change
                         21,          // last_notification
                         22,          // last_state_change
                         23,          // last_time_critical
                         24,          // last_time_ok
                         25,          // last_time_unknown
                         26,          // last_time_warning
                         27,          // last_update
                         2.8,         // latency
                         29,          // max_check_attempts
                         30,          // next_check
                         31,          // next_notification
                         false,       // no_more_notifications
                         33,          // notification_number
                         true,        // obsess_over
                         "output",    // output
                         false,       // passive_checks_enabled
                         3.7,         // percent_state_change
                         "perf_data", // perf_data
                         3.9,         // retry_interval
                         "srvc_dscpt",// service_description
                         true,        // should_be_scheduled
                         42);         // state_type

ASSERT_EQ(ss.host_name, "host_name");
ASSERT_EQ(ss.acknowledged, true);
ASSERT_EQ(ss.acknowledgement_type, 2);
ASSERT_EQ(ss.active_checks_enabled, false);
ASSERT_EQ(ss.check_command, "check_cmd");
ASSERT_EQ(ss.check_interval, 0.5);
ASSERT_EQ(ss.check_period, "chk_priod");
ASSERT_EQ(ss.check_type, 7);
ASSERT_EQ(ss.current_check_attempt, 8);
ASSERT_EQ(ss.current_state, 9);
ASSERT_EQ(ss.downtime_depth, 10);
ASSERT_EQ(ss.event_handler, "evnt_hdlr");
ASSERT_EQ(ss.event_handler_enabled, true);
ASSERT_EQ(ss.execution_time, 1.3);
ASSERT_EQ(ss.flap_detection_enabled, true);
ASSERT_EQ(ss.has_been_checked, false);
ASSERT_EQ(ss.host_id, 16);
ASSERT_EQ(ss.is_flapping, true);
ASSERT_EQ(ss.last_check, 18);
ASSERT_EQ(ss.last_hard_state, 19);
ASSERT_EQ(ss.last_hard_state_change, 20);
ASSERT_EQ(ss.last_notification, 21);
ASSERT_EQ(ss.last_state_change, 22);
ASSERT_EQ(ss.last_time_critical, 23);
ASSERT_EQ(ss.last_time_ok, 24);
ASSERT_EQ(ss.last_time_unknown, 25);
ASSERT_EQ(ss.last_time_warning, 26);
ASSERT_EQ(ss.last_update, 27);
ASSERT_EQ(ss.latency, 2.8);
ASSERT_EQ(ss.max_check_attempts, 29);
ASSERT_EQ(ss.next_check, 30);
ASSERT_EQ(ss.next_notification, 31);
ASSERT_EQ(ss.no_more_notifications, false);
ASSERT_EQ(ss.notification_number, 33);
ASSERT_EQ(ss.obsess_over, true);
ASSERT_EQ(ss.output, "output");
ASSERT_EQ(ss.passive_checks_enabled, false);
ASSERT_EQ(ss.percent_state_change, 3.7);
ASSERT_EQ(ss.perf_data, "perf_data");
ASSERT_EQ(ss.retry_interval, 3.9);
ASSERT_EQ(ss.service_description, "srvc_dscpt");
ASSERT_EQ(ss.should_be_scheduled, true);
ASSERT_EQ(ss.state_type, 42);
/*
ASSERT_EQ(ss., 43);
ASSERT_EQ(ss., 44);
ASSERT_EQ(ss., 45);
ASSERT_EQ(ss., 46);
ASSERT_EQ(ss., 47);*/

}


/**
 *  Check service_status' default constructor.
 */
TEST_F(ServiceStatus, DefaultCtor) {
  // Object.
  neb::service_status ss;

  // Check.
  ASSERT_FALSE((ss.source_id != 0));
  ASSERT_FALSE((ss.destination_id != 0));
  ASSERT_FALSE(ss.acknowledged);
  ASSERT_FALSE((ss.acknowledgement_type != 0));
  ASSERT_FALSE(ss.active_checks_enabled);
  ASSERT_FALSE(!ss.check_command.empty());
  ASSERT_FALSE((fabs(ss.check_interval) > 0.001));
  ASSERT_FALSE(!ss.check_period.empty());
  ASSERT_FALSE((ss.check_type != 0));
  ASSERT_FALSE((ss.current_check_attempt != 0));
  ASSERT_FALSE((ss.current_state != 4));
  ASSERT_FALSE((ss.downtime_depth != 0));
  ASSERT_FALSE(!ss.enabled);
  ASSERT_FALSE(!ss.event_handler.empty());
  ASSERT_FALSE(ss.event_handler_enabled);
  ASSERT_FALSE((fabs(ss.execution_time) > 0.001));
  ASSERT_FALSE(ss.flap_detection_enabled);
  ASSERT_FALSE(ss.has_been_checked);
  ASSERT_FALSE((ss.host_id != 0));
  ASSERT_FALSE(!ss.host_name.empty());
  ASSERT_FALSE(ss.is_flapping);
  ASSERT_FALSE((ss.last_check != 0));
  ASSERT_FALSE((ss.last_hard_state != 4));
  ASSERT_FALSE((ss.last_hard_state_change != 0));
  ASSERT_FALSE((ss.last_notification != 0));
  ASSERT_FALSE((ss.last_state_change != 0));
  ASSERT_FALSE((ss.last_time_critical != 0));
  ASSERT_FALSE((ss.last_time_ok != 0));
  ASSERT_FALSE((ss.last_time_unknown != 0));
  ASSERT_FALSE((ss.last_time_warning != 0));
  ASSERT_FALSE((ss.last_update != 0));
  ASSERT_FALSE((fabs(ss.latency) > 0.001));
  ASSERT_FALSE((ss.max_check_attempts != 0));
  ASSERT_FALSE((ss.next_check != 0));
  ASSERT_FALSE((ss.next_notification != 0));
  ASSERT_FALSE(ss.no_more_notifications);
  ASSERT_FALSE((ss.notification_number != 0));
  ASSERT_FALSE(ss.notifications_enabled);
  ASSERT_FALSE(ss.obsess_over);
  ASSERT_FALSE(!ss.output.empty());
  ASSERT_FALSE(ss.passive_checks_enabled);
  ASSERT_FALSE((fabs(ss.percent_state_change) > 0.001));
  ASSERT_FALSE(!ss.perf_data.empty());
  ASSERT_FALSE((fabs(ss.retry_interval) > 0.001));
  ASSERT_FALSE(ss.should_be_scheduled);
  ASSERT_FALSE((ss.state_type != 0));
  ASSERT_FALSE(!ss.service_description.empty());
  ASSERT_FALSE((ss.service_id != 0));
}
