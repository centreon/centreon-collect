/**
 * Copyright 2019-2024 Centreon (https://www.centreon.com/)
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

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <cstring>

#include <com/centreon/engine/macros.hh>
#include <com/centreon/engine/macros/grab_host.hh>
#include <com/centreon/engine/macros/process.hh>
#include "../test_engine.hh"
#include "../timeperiod/utils.hh"
#include "com/centreon/engine/checks/checker.hh"
#include "com/centreon/engine/commands/commands.hh"
#include "com/centreon/engine/configuration/applier/contact.hh"
#include "com/centreon/engine/configuration/applier/contactgroup.hh"
#include "com/centreon/engine/configuration/applier/host.hh"
#include "com/centreon/engine/configuration/applier/service.hh"
#include "com/centreon/engine/configuration/applier/servicedependency.hh"
#include "com/centreon/engine/configuration/applier/serviceescalation.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/serviceescalation.hh"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;
using namespace com::centreon::engine::configuration::applier;

class ServiceDowntimeNotification : public TestEngine {
 protected:
  std::unique_ptr<configuration::state_helper> _state_hlp;

 public:
  void SetUp() override {
    _state_hlp = init_config_state();
    error_cnt err;

    configuration::applier::contact ct_aply;
    configuration::Contact ctct{new_pb_configuration_contact("admin", true)};
    ct_aply.add_object(ctct);
    _state_hlp->expand(err);
    ct_aply.resolve_object(ctct, err);

    configuration::Host hst{new_pb_configuration_host("test_host", "admin")};
    configuration::applier::host hst_aply;
    hst_aply.add_object(hst);

    configuration::Service svc{
        new_pb_configuration_service("test_host", "test_svc", "admin")};
    configuration::applier::service svc_aply;
    svc_aply.add_object(svc);

    hst_aply.resolve_object(hst, err);
    svc_aply.resolve_object(svc, err);

    host_map const& hm{engine::host::hosts};
    _host = hm.begin()->second;
    _host->set_current_state(engine::host::state_up);
    _host->set_state_type(checkable::hard);
    _host->set_acknowledgement(AckType::NONE);
    _host->set_notify_on(static_cast<uint32_t>(-1));
    _host->set_max_attempts(1);

    service_map const& sm{engine::service::services};
    _svc = sm.begin()->second;
    _svc->set_current_state(engine::service::state_ok);
    _svc->set_state_type(checkable::hard);
    _svc->set_acknowledgement(AckType::NONE);
    _svc->set_notify_on(static_cast<uint32_t>(-1));
    _svc->set_max_attempts(1);
  }

  void TearDown() override {
    _svc.reset();
    _host.reset();
    deinit_config_state();
  }

 protected:
  std::shared_ptr<engine::host> _host;
  std::shared_ptr<engine::service> _svc;
};

/**
 * @brief Test case for service downtime notification.
 *
 * This test verifies that notifications are correctly sent when a service goes
 * down and then recovers. It simulates a service check result indicating a
 * critical state, followed by a service check result indicating an OK state.
 *
 * The test uses time travel to simulate the passage of time between the
 * critical and OK states.
 *
 * @details
 * - `t0`: The initial time when the service is in an OK state => critical
 * state.
 * - `t1`: The time 5 minutes later when the service recovers to an OK state.
 *
 * The test performs the following steps:
 * 1. Sets the initial time `t0` and configures the service to be in an OK
 * state.
 * 2. Simulates a service check result indicating a critical state.
 * 3. Advances the time by 5 minutes to `t1`.
 * 4. Simulates a service check result indicating an OK state.
 * 5. Ensures that the notification IDs are correctly incremented.
 * 6. Verifies that a notification is sent for the critical state.
 * 7. Verifies that a notification is sent for the recovery (OK) state.
 */
TEST_F(ServiceDowntimeNotification, SVCKO_SVCOK_Notify) {
  // Step 1: Sets the initial time `t0` and configures the service to be in an
  // OK state.
  auto t0 =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

  set_time(t0);
  notifications_logger->set_pattern("[%E] [%n] [%l] [%s:%#] %v");

  _svc->set_notification_interval(1);
  _svc->set_current_state(engine::service::state_ok);
  _svc->set_last_hard_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(t0);
  _svc->set_state_type(checkable::hard);
  _svc->set_accept_passive_checks(true);

  // Step 2: Simulates a service check result indicating a critical state in t0.
  testing::internal::CaptureStdout();

  uint64_t id = _svc->get_next_notification_id();

  std::string cmd(fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;service crit",
      t0));
  process_external_command(cmd.c_str());
  // as we have _max_attempts=1, we should notify at the first failure
  checks::checker::instance().reap();
  uint64_t first_notif_id = _svc->get_next_notification_id();

  // Step 3: Advances the time by 5 minutes to `t1`.
  enable_time_travel(true, 300);
  auto t1 =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  set_time(t1);
  // Step 4: Simulates a service check result indicating an OK state after t1.
  cmd = fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;service ok", t1);
  process_external_command(cmd.c_str());
  // as we have _max_attempts=1, we should notify at the first failure
  checks::checker::instance().reap();

  std::string out = testing::internal::GetCapturedStdout();

  std::cout << " out=" << out << std::endl << std::endl;

  // Step 5: Ensures that the notification IDs are correctly incremented.
  ASSERT_EQ(_svc->get_next_notification_id(), id + 2);

  checks::checker::instance().reap();

  ASSERT_EQ(first_notif_id, id + 1);
  size_t step1 = out.find(fmt::format("[{}]", t0));
  ASSERT_NE(step1, std::string::npos);
  // Step 6: Verifies that a notification is sent for the critical state after
  // t0.
  size_t step2 = out.find(
      "SERVICE NOTIFICATION: "
      "admin;test_host;test_svc;CRITICAL;cmd;service crit",
      step1 + 1);
  ASSERT_NE(step2, std::string::npos);

  size_t step7 = out.find(fmt::format("[{}]", t1), step2);
  ASSERT_NE(step7, std::string::npos);
  // Step 7: Verifies that a notification is sent for the recovery (OK) state
  // after t1.
  ASSERT_NE(out.find("SERVICE NOTIFICATION: "
                     "admin;test_host;test_svc;RECOVERY (OK);cmd;service ok",
                     step7 + 1),
            std::string::npos);
  //  disable_time_travel();
  enable_time_travel(false, 0);
}

/**
 * @brief Test case for service downtime notification with downtime start and
 * end.
 *
 * This test verifies that notifications are correctly sent when a service goes
 * down, enters downtime, exits downtime, and then recovers. It simulates a
 * service check result indicating a critical state, followed by downtime start,
 * downtime end, and a service check result indicating an OK state.
 *
 * The test uses time travel to simulate the passage of time between the
 * critical state, downtime start, downtime end, and OK state.
 *
 * @details
 * - `t0`: The initial time when the service is in an OK state and then goes
 * critical.
 * - `t1`: The time 100 seconds later when the downtime starts.
 * - `t2`: The time 200 seconds later when the downtime ends.
 * - `t3`: The time 300 seconds later when the service recovers to an OK state.
 *
 * The test performs the following steps:
 * 1. Sets the initial time `t0` and configures the service to be in an OK
 * state.
 * 2. Simulates a service check result indicating a critical state.
 * 3. Advances the time by 100 seconds to `t1` and starts downtime.
 * 4. Advances the time by 200 seconds to `t2` and ends downtime.
 * 5. Advances the time by 300 seconds to `t3` and simulates a service check
 * result indicating an OK state.
 * 6. Ensures that the notification IDs are correctly incremented.
 * 7. Verifies that a notification is sent for the critical state.
 * 8. Verifies that a notification is sent for the downtime start.
 * 9. Verifies that a notification is sent for the downtime end.
 * 10. Verifies that a notification is sent for the recovery (OK) state.
 */
TEST_F(ServiceDowntimeNotification, SVCKO_Dt_CancelDt_SVCOK_Notify) {
  // Step 1: Sets the initial time `t0` and configures the service to be in an
  // OK state.
  auto t0 =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

  set_time(t0);
  notifications_logger->set_pattern("[%E] [%n] [%l] [%s:%#] %v");

  _svc->set_notification_interval(1);
  _svc->set_current_state(engine::service::state_ok);
  _svc->set_last_hard_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(t0);
  _svc->set_state_type(checkable::hard);
  _svc->set_accept_passive_checks(true);

  // Step 2: Simulates a service check result indicating a critical state.
  testing::internal::CaptureStdout();

  uint64_t id{_svc->get_next_notification_id()};

  std::string cmd(fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;service crit",
      t0));
  process_external_command(cmd.c_str());
  // as we have _max_attempts=1, we should notify at the first failure
  checks::checker::instance().reap();
  uint64_t first_notif_id = _svc->get_next_notification_id();

  // Step 3: Advances the time by 100 seconds to `t1` and starts downtime.
  enable_time_travel(true, 100);
  auto t1 =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  set_time(t1);

  int res1 = _svc->notify(notifier::reason_downtimestart, "", "",
                          notifier::notification_option_none);
  _svc->inc_scheduled_downtime_depth();
  uint64_t second_notif_id = _svc->get_next_notification_id();

  // Step 4: Advances the time by 200 seconds to `t2` and ends downtime.
  enable_time_travel(true, 200);
  auto t2 =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  set_time(t2);

  _svc->set_scheduled_downtime_depth(0);
  int res2 = _svc->notify(notifier::reason_downtimeend, "", "",
                          notifier::notification_option_none);
  uint64_t third_notif_id = _svc->get_next_notification_id();

  // Step 5: Advances the time by 300 seconds to `t3` and simulates a service
  // check result indicating an OK state.
  enable_time_travel(true, 300);
  auto t3 =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  set_time(t3);

  cmd = fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;service ok", t3);
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();

  std::string out = testing::internal::GetCapturedStdout();

  std::cout << " out=" << out << std::endl << std::endl;

  // Step 6: Ensures that the notification IDs are correctly incremented.
  ASSERT_EQ(first_notif_id, id + 1);
  size_t step1{out.find(fmt::format("[{}]", t0))};
  ASSERT_NE(step1, std::string::npos);
  // Step 7: Verifies that a notification is sent for the critical state.
  size_t step2 = out.find(
      "SERVICE NOTIFICATION: "
      "admin;test_host;test_svc;CRITICAL;cmd;service crit",
      step1 + 1);
  ASSERT_NE(step2, std::string::npos);

  // Step 8: Verifies that a notification is sent for the downtime start.
  ASSERT_EQ(res1, OK);
  ASSERT_EQ(second_notif_id, id + 2);
  size_t step3{out.find(fmt::format("[{}]", t1), step2)};
  ASSERT_NE(step3, std::string::npos);
  size_t step4 = out.find(
      "SERVICE NOTIFICATION: admin;test_host;test_svc;DOWNTIMESTART "
      "(CRITICAL);cmd;service crit",
      step3);
  ASSERT_NE(step4, std::string::npos);

  // Step 9: Verifies that a notification is sent for the downtime end.
  ASSERT_EQ(res2, OK);
  ASSERT_EQ(third_notif_id, id + 3);
  size_t step5{out.find(fmt::format("[{}]", t2), step4)};
  ASSERT_NE(step5, std::string::npos);
  size_t step6 = out.find(
      "SERVICE NOTIFICATION: admin;test_host;test_svc;DOWNTIMEEND "
      "(CRITICAL);cmd;service crit",
      step5);
  ASSERT_NE(step6, std::string::npos);

  // Step 10: Verifies that a notification is sent for the recovery (OK) state.
  ASSERT_EQ(_svc->get_next_notification_id(), id + 4);
  size_t step7 = out.find(fmt::format("[{}]", t3), step6);
  ASSERT_NE(step7, std::string::npos);
  ASSERT_NE(out.find("SERVICE NOTIFICATION: "
                     "admin;test_host;test_svc;RECOVERY (OK);cmd;service ok",
                     step7 + 1),
            std::string::npos);
  //  disable_time_travel();
  enable_time_travel(false, 0);
}

/**
 * @brief Test case for service downtime notification with downtime start,
 * service going critical, downtime end, and recovery.
 *
 * This test verifies that notifications are correctly sent when a service is
 * in an OK state, enters downtime, goes critical, exits downtime, and then
 * recovers. It simulates a service check result indicating a critical state,
 * followed by downtime start, downtime end, and a service check result
 * indicating an OK state.
 *
 * The test uses time travel to simulate the passage of time between the
 * different states.
 *
 * @details
 * - `t0`: The initial time when the service is in an OK state.
 * - `t1`: The time 100 seconds later when the downtime starts.
 * - `t2`: The time 200 seconds later when the service goes critical.
 * - `t3`: The time 300 seconds later when the downtime ends.
 * - `t4`: The time 400 seconds later when the service recovers to an OK state.
 *
 * The test performs the following steps:
 * 1. Sets the initial time `t0` and configures the service to be in an OK
 * state.
 * 2. Advances the time by 100 seconds to `t1` and starts downtime.
 * 3. Advances the time by 200 seconds to `t2` and simulates a service check
 * result indicating a critical state.
 * 4. Advances the time by 300 seconds to `t3` and ends downtime.
 * 5. Advances the time by 400 seconds to `t4` and simulates a service check
 * result indicating an OK state.
 * 6. Verifies that a notification is sent for the downtime start.
 * 7. Verifies that no notification is sent for the critical state due to
 * active downtime.
 * 8. Verifies that a notification is sent for the downtime end.
 * 9. Verifies that a notification is sent for the recovery (OK) state.
 */
TEST_F(ServiceDowntimeNotification,
       SVCOK_Dt_SVCKO_CancelDt_Notify_SVCOK_Notify) {
  // Step 1: Sets the initial time `t0` and configures the service to be in an
  // OK state.
  auto t0 =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  set_time(t0);
  notifications_logger->set_pattern("[%E] [%n] [%l] [%s:%#] %v");

  _svc->set_notification_interval(1);
  _svc->set_current_state(engine::service::state_ok);
  _svc->set_last_hard_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(t0);
  _svc->set_state_type(checkable::hard);
  _svc->set_accept_passive_checks(true);

  testing::internal::CaptureStdout();

  uint64_t id = _svc->get_next_notification_id();

  // Step 2: Advances the time by 100 seconds to `t1` and starts downtime.
  enable_time_travel(true, 100);
  auto t1 =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  set_time(t1);
  _svc->notify(notifier::reason_downtimestart, "", "",
               notifier::notification_option_none);
  _svc->inc_scheduled_downtime_depth();
  uint64_t first_notif_id = _svc->get_next_notification_id();

  // Step 3: Advances the time by 200 seconds to `t2` and simulates a service
  // check result indicating a critical state.
  enable_time_travel(true, 200);
  auto t2 =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  set_time(t2);
  // downtime active => no notification
  std::string cmd(fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;service crit",
      t2));
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();
  _svc->get_next_notification_id();

  // Step 4: Advances the time by 300 seconds to `t3` and ends downtime.
  enable_time_travel(true, 300);
  auto t3 =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  set_time(t3);
  _svc->set_scheduled_downtime_depth(0);
  _svc->notify(notifier::reason_downtimeend, "", "",
               notifier::notification_option_none);
  _svc->get_next_notification_id();

  // Step 5: Advances the time by 400 seconds to `t4` and simulates a service
  // check result indicating an OK state.
  enable_time_travel(true, 400);
  auto t4 =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  set_time(t4);
  cmd = fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;service ok", t4);
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();
  _svc->get_next_notification_id();

  std::string out = testing::internal::GetCapturedStdout();

  std::cout << " out=" << out << std::endl << std::endl;

  // Step 6: Verifies that a notification is sent for the downtime start.
  ASSERT_EQ(first_notif_id, id + 1);
  size_t step1{out.find(fmt::format("[{}]", t1))};
  ASSERT_NE(step1, std::string::npos);
  size_t step1_bis = out.find(
      "SERVICE NOTIFICATION: admin;test_host;test_svc;DOWNTIMESTART (OK);cmd;",
      step1);
  ASSERT_NE(step1_bis, std::string::npos);

  // Step 7: Verifies that no notification is sent for the critical state due to
  // active downtime.
  ASSERT_EQ(out.find("SERVICE NOTIFICATION: "
                     "admin;test_host;test_svc;CRITICAL;cmd;service crit"),
            std::string::npos);

  // Step 8: Verifies that a notification is sent for the downtime end.
  size_t step2{out.find(fmt::format("[{}]", t3), step1_bis)};
  ASSERT_NE(step2, std::string::npos);
  size_t step2_bis = out.find(
      "SERVICE NOTIFICATION: admin;test_host;test_svc;DOWNTIMEEND "
      "(CRITICAL);cmd;service crit",
      step2);
  ASSERT_NE(step2_bis, std::string::npos);

  // Step 9: Verifies that a notification is sent for the recovery (OK) state.
  ASSERT_EQ(out.find("SERVICE NOTIFICATION: "
                     "admin;test_host;test_svc;RECOVERY (OK);cmd;service ok",
                     step2_bis + 1),
            std::string::npos);
  //  disable_time_travel();
  enable_time_travel(false, 0);
}
