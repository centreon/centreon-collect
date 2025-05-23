/**
 * Copyright 2019 Centreon (https://www.centreon.com/)
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

class ServiceNotification : public TestEngine {
 public:
  void SetUp() override {
    init_config_state();
    error_cnt err;

    configuration::applier::contact ct_aply;
    configuration::Contact ctct{new_pb_configuration_contact("admin", true)};
    configuration::Contact ctct1{
        new_pb_configuration_contact("admin1", false, "c,r")};
    ct_aply.add_object(ctct);
    ct_aply.add_object(ctct1);
    ct_aply.expand_objects(pb_config);
    ct_aply.resolve_object(ctct, err);
    ct_aply.resolve_object(ctct1, err);

    configuration::Host hst{new_pb_configuration_host("test_host", "admin")};
    configuration::Service svc{
        new_pb_configuration_service("test_host", "test_svc", "admin,admin1")};
    configuration::applier::host hst_aply;
    hst_aply.add_object(hst);

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

    service_map const& sm{engine::service::services};
    _svc = sm.begin()->second;
    _svc->set_current_state(engine::service::state_ok);
    _svc->set_state_type(checkable::hard);
    _svc->set_acknowledgement(AckType::NONE);
    _svc->set_notify_on(static_cast<uint32_t>(-1));
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

TEST_F(ServiceNotification, SimpleNormalServiceNotification) {
  /* We are using a local time() function defined in tests/timeperiod/utils.cc.
   * If we call time(), it is not the glibc time() function that will be called.
   */
  ASSERT_EQ(_host->services.size(), 1u);
  set_time(43200);
  std::unique_ptr<engine::timeperiod> tperiod{
      new_timeperiod_with_timeranges("tperiod", "alias")};

  std::unique_ptr<engine::serviceescalation> service_escalation{
      new engine::serviceescalation("test_host", "test_svc", 0, 1, 1.0,
                                    "tperiod", 7, 12345)};
  _svc->set_current_state(engine::service::state_critical);
  _svc->set_last_state(engine::service::state_critical);
  _svc->set_last_hard_state_change(43200);
  _svc->set_state_type(checkable::hard);

  ASSERT_TRUE(service_escalation);
  uint64_t id{_svc->get_next_notification_id()};
  _svc->set_notification_period_ptr(tperiod.get());
  ASSERT_EQ(_svc->notify(notifier::reason_normal, "", "",
                         notifier::notification_option_none),
            OK);
  ASSERT_EQ(id + 1, _svc->get_next_notification_id());
}

TEST_F(ServiceNotification,
       SimpleNormalServiceNotificationNotificationsdisabled) {
  /* We are using a local time() function defined in tests/timeperiod/utils.cc.
   * If we call time(), it is not the glibc time() function that will be called.
   */
  pb_config.set_enable_notifications(false);
  set_time(43200);
  std::unique_ptr<engine::timeperiod> tperiod{
      new_timeperiod_with_timeranges("tperiod", "alias")};

  std::unique_ptr<engine::serviceescalation> service_escalation{
      new engine::serviceescalation("test_host", "test_svc", 0, 1, 1.0,
                                    "tperiod", 7, 12345)};

  ASSERT_TRUE(service_escalation);
  uint64_t id{_svc->get_next_notification_id()};
  _svc->set_notification_period_ptr(tperiod.get());
  ASSERT_EQ(_svc->notify(notifier::reason_normal, "", "",
                         notifier::notification_option_none),
            OK);
  ASSERT_EQ(id, _svc->get_next_notification_id());
}

TEST_F(ServiceNotification,
       SimpleNormalServiceNotificationNotifierNotifdisabled) {
  /* We are using a local time() function defined in tests/timeperiod/utils.cc.
   * If we call time(), it is not the glibc time() function that will be called.
   */
  set_time(43200);
  std::unique_ptr<engine::timeperiod> tperiod{
      new_timeperiod_with_timeranges("tperiod", "alias")};

  std::unique_ptr<engine::serviceescalation> service_escalation{
      new engine::serviceescalation("test_host", "test_svc", 0, 1, 1.0,
                                    "tperiod", 7, 12345)};

  ASSERT_TRUE(service_escalation);
  uint64_t id{_svc->get_next_notification_id()};
  _svc->set_notifications_enabled(false);
  _svc->set_notification_period_ptr(tperiod.get());
  ASSERT_EQ(_svc->notify(notifier::reason_normal, "", "",
                         notifier::notification_option_none),
            OK);
  ASSERT_EQ(id, _svc->get_next_notification_id());
}

TEST_F(ServiceNotification, SimpleNormalServiceNotificationOutsideTimeperiod) {
  std::unique_ptr<engine::timeperiod> tperiod{
      new_timeperiod_with_timeranges("tperiod", "alias")};
  set_time(20000);

  uint64_t id{_svc->get_next_notification_id()};
  for (int i = 0; i < 7; ++i)
    tperiod->days[i].emplace_back(43200, 86400);

  std::unique_ptr<engine::serviceescalation> service_escalation{
      new engine::serviceescalation("test_host", "test_svc", 0, 1, 1.0, "", 7,
                                    12345)};
  _svc->set_notification_period_ptr(tperiod.get());

  ASSERT_TRUE(service_escalation);
  ASSERT_EQ(_svc->notify(notifier::reason_normal, "", "",
                         notifier::notification_option_none),
            OK);
  ASSERT_EQ(id, _svc->get_next_notification_id());
}

TEST_F(ServiceNotification,
       SimpleNormalServiceNotificationForcedWithNotificationDisabled) {
  pb_config.set_enable_notifications(false);
  std::unique_ptr<engine::timeperiod> tperiod{
      new_timeperiod_with_timeranges("tperiod", "alias")};
  set_time(20000);

  uint64_t id{_svc->get_next_notification_id()};
  for (int i = 0; i < 7; ++i)
    tperiod->days[i].emplace_back(43200, 86400);

  std::unique_ptr<engine::serviceescalation> service_escalation{
      new engine::serviceescalation("test_host", "test_svc", 0, 1, 1.0, "", 7,
                                    12345)};
  _svc->set_notification_period_ptr(tperiod.get());

  ASSERT_TRUE(service_escalation);
  ASSERT_EQ(_svc->notify(notifier::reason_normal, "", "",
                         notifier::notification_option_forced),
            OK);
  ASSERT_EQ(id + 1, _svc->get_next_notification_id());
}

TEST_F(ServiceNotification, SimpleNormalServiceNotificationForcedNotification) {
  std::unique_ptr<engine::timeperiod> tperiod{
      new_timeperiod_with_timeranges("tperiod", "alias")};
  set_time(20000);

  uint64_t id{_svc->get_next_notification_id()};
  for (int i = 0; i < 7; ++i)
    tperiod->days[i].emplace_back(43200, 86400);

  std::unique_ptr<engine::serviceescalation> service_escalation{
      new engine::serviceescalation("test_host", "test_svc", 0, 1, 1.0, "", 7,
                                    12345)};
  _svc->set_notification_period_ptr(tperiod.get());

  ASSERT_TRUE(service_escalation);
  ASSERT_EQ(_svc->notify(notifier::reason_normal, "", "",
                         notifier::notification_option_forced),
            OK);
  ASSERT_EQ(id + 1, _svc->get_next_notification_id());
}

TEST_F(ServiceNotification, SimpleNormalServiceNotificationWithDowntime) {
  std::unique_ptr<engine::timeperiod> tperiod{
      new_timeperiod_with_timeranges("tperiod", "alias")};
  set_time(20000);

  _svc->set_scheduled_downtime_depth(30);
  uint64_t id{_svc->get_next_notification_id()};

  std::unique_ptr<engine::serviceescalation> service_escalation{
      new engine::serviceescalation("test_host", "test_svc", 0, 1, 1.0, "", 7,
                                    12345)};
  _svc->set_notification_period_ptr(tperiod.get());

  ASSERT_TRUE(service_escalation);
  ASSERT_EQ(_svc->notify(notifier::reason_normal, "", "",
                         notifier::notification_option_none),
            OK);
  ASSERT_EQ(id, _svc->get_next_notification_id());
}

TEST_F(ServiceNotification, SimpleNormalServiceNotificationWithFlapping) {
  std::unique_ptr<engine::timeperiod> tperiod{
      new_timeperiod_with_timeranges("tperiod", "alias")};
  set_time(20000);

  _svc->set_is_flapping(true);
  uint64_t id{_svc->get_next_notification_id()};

  std::unique_ptr<engine::serviceescalation> service_escalation{
      new engine::serviceescalation("test_host", "test_svc", 0, 1, 1.0, "", 7,
                                    12345)};
  _svc->set_notification_period_ptr(tperiod.get());

  ASSERT_TRUE(service_escalation);
  ASSERT_EQ(_svc->notify(notifier::reason_normal, "", "",
                         notifier::notification_option_none),
            OK);
  ASSERT_EQ(id, _svc->get_next_notification_id());
}

TEST_F(ServiceNotification, SimpleNormalServiceNotificationWithSoftState) {
  std::unique_ptr<engine::timeperiod> tperiod{
      new_timeperiod_with_timeranges("tperiod", "alias")};
  set_time(20000);

  _svc->set_state_type(checkable::soft);
  uint64_t id{_svc->get_next_notification_id()};

  std::unique_ptr<engine::serviceescalation> service_escalation{
      new engine::serviceescalation("test_host", "test_svc", 0, 1, 1.0, "", 7,
                                    12345)};
  _svc->set_notification_period_ptr(tperiod.get());

  ASSERT_TRUE(service_escalation);
  ASSERT_EQ(_svc->notify(notifier::reason_normal, "", "",
                         notifier::notification_option_none),
            OK);
  ASSERT_EQ(id, _svc->get_next_notification_id());
}

TEST_F(ServiceNotification,
       SimpleNormalServiceNotificationWithHardStateAcknowledged) {
  std::unique_ptr<engine::timeperiod> tperiod{
      new_timeperiod_with_timeranges("tperiod", "alias")};
  set_time(20000);

  uint64_t id{_svc->get_next_notification_id()};

  std::unique_ptr<engine::serviceescalation> service_escalation{
      new engine::serviceescalation("test_host", "test_svc", 0, 1, 1.0, "", 7,
                                    12345)};
  _svc->set_notification_period_ptr(tperiod.get());

  _svc->set_acknowledgement(AckType::NORMAL);
  ASSERT_TRUE(service_escalation);
  ASSERT_EQ(_svc->notify(notifier::reason_normal, "", "",
                         notifier::notification_option_none),
            OK);
  ASSERT_EQ(id, _svc->get_next_notification_id());
}

TEST_F(ServiceNotification,
       SimpleNormalServiceNotificationAfterPreviousTooSoon) {
  std::unique_ptr<engine::timeperiod> tperiod{
      new_timeperiod_with_timeranges("tperiod", "alias")};
  set_time(20000);

  uint64_t id{_svc->get_next_notification_id()};

  std::unique_ptr<engine::serviceescalation> service_escalation{
      new engine::serviceescalation("test_host", "test_svc", 0, 1, 1.0, "", 7,
                                    12345)};
  _svc->set_notification_period_ptr(tperiod.get());

  _svc->set_acknowledgement(AckType::NORMAL);
  ASSERT_TRUE(service_escalation);
  _svc->set_last_notification(19999);
  ASSERT_EQ(_svc->notify(notifier::reason_normal, "", "",
                         notifier::notification_option_none),
            OK);
  ASSERT_EQ(id, _svc->get_next_notification_id());
}

TEST_F(ServiceNotification,
       SimpleNormalServiceNotificationAfterPreviousWithNullInterval) {
  std::unique_ptr<engine::timeperiod> tperiod{
      new_timeperiod_with_timeranges("tperiod", "alias")};
  set_time(20000);

  uint64_t id{_svc->get_next_notification_id()};

  std::unique_ptr<engine::serviceescalation> service_escalation{
      new engine::serviceescalation("test_host", "test_svc", 0, 1, 1.0, "", 7,
                                    12345)};
  _svc->set_notification_period_ptr(tperiod.get());

  _svc->set_acknowledgement(AckType::NORMAL);
  ASSERT_TRUE(service_escalation);
  _svc->set_last_notification(19500);
  _svc->set_notification_number(1);
  _svc->set_notification_interval(0);
  ASSERT_EQ(_svc->notify(notifier::reason_normal, "", "",
                         notifier::notification_option_none),
            OK);
  ASSERT_EQ(id, _svc->get_next_notification_id());
}

TEST_F(ServiceNotification, SimpleNormalServiceNotificationOnStateNotNotified) {
  std::unique_ptr<engine::timeperiod> tperiod{
      new_timeperiod_with_timeranges("tperiod", "alias")};
  set_time(20000);

  uint64_t id{_svc->get_next_notification_id()};

  std::unique_ptr<engine::serviceescalation> service_escalation{
      new engine::serviceescalation("test_host", "test_svc", 0, 1, 1.0, "", 7,
                                    12345)};
  _svc->set_notification_period_ptr(tperiod.get());

  _svc->set_acknowledgement(AckType::NONE);
  ASSERT_TRUE(service_escalation);
  _svc->remove_notify_on(notifier::critical);
  _svc->set_current_state(engine::service::state_critical);
  ASSERT_EQ(_svc->notify(notifier::reason_normal, "", "",
                         notifier::notification_option_none),
            OK);
  ASSERT_EQ(id, _svc->get_next_notification_id());
}

TEST_F(ServiceNotification,
       SimpleNormalServiceNotificationOnStateBeforeFirstNotifDelay) {
  std::unique_ptr<engine::timeperiod> tperiod{
      new_timeperiod_with_timeranges("tperiod", "alias")};
  set_time(20000);

  uint64_t id{_svc->get_next_notification_id()};

  std::unique_ptr<engine::serviceescalation> service_escalation{
      new engine::serviceescalation("test_host", "test_svc", 0, 1, 1.0, "", 7,
                                    12345)};
  _svc->set_notification_period_ptr(tperiod.get());

  _svc->set_acknowledgement(AckType::NONE);
  ASSERT_TRUE(service_escalation);
  _svc->set_current_state(engine::service::state_critical);
  _svc->set_last_hard_state_change(20000 - 200);
  /* It is multiplicated by config->interval_length(): we set 5 for 5*60 */
  _svc->set_first_notification_delay(5);
  ASSERT_EQ(_svc->notify(notifier::reason_normal, "", "",
                         notifier::notification_option_none),
            OK);
  ASSERT_EQ(id, _svc->get_next_notification_id());
}

TEST_F(ServiceNotification,
       SimpleNormalServiceNotificationOnStateAfterFirstNotifDelay) {
  std::unique_ptr<engine::timeperiod> tperiod{
      new_timeperiod_with_timeranges("tperiod", "alias")};
  set_time(20000);

  uint64_t id{_svc->get_next_notification_id()};

  std::unique_ptr<engine::serviceescalation> service_escalation{
      new engine::serviceescalation("test_host", "test_svc", 0, 1, 1.0, "", 7,
                                    12345)};
  _svc->set_notification_period_ptr(tperiod.get());

  _svc->set_acknowledgement(AckType::NONE);
  ASSERT_TRUE(service_escalation);
  _svc->set_current_state(engine::service::state_critical);
  _svc->set_last_hard_state_change(20000 - 400);
  _svc->set_first_notification_delay(5);
  ASSERT_EQ(_svc->notify(notifier::reason_normal, "", "",
                         notifier::notification_option_none),
            OK);
  ASSERT_EQ(id + 1, _svc->get_next_notification_id());
}

TEST_F(ServiceNotification,
       SimpleNormalServiceNotificationNotifierDelayTooShort) {
  /* We are using a local time() function defined in tests/timeperiod/utils.cc.
   * If we call time(), it is not the glibc time() function that will be called.
   */
  set_time(43200);
  std::unique_ptr<engine::timeperiod> tperiod{
      new_timeperiod_with_timeranges("tperiod", "alias")};

  std::unique_ptr<engine::serviceescalation> service_escalation{
      new engine::serviceescalation("test_host", "test_svc", 0, 1, 1.0,
                                    "tperiod", 7, 12345)};

  _svc->set_current_state(engine::service::state_critical);
  _svc->set_last_state(engine::service::state_critical);
  _svc->set_last_hard_state_change(43200);
  _svc->set_state_type(checkable::hard);
  ASSERT_TRUE(service_escalation);
  uint64_t id{_svc->get_next_notification_id()};
  /* We configure the notification interval to 2 minutes */
  _svc->set_notification_interval(2);
  _svc->set_notification_period_ptr(tperiod.get());
  ASSERT_EQ(_svc->notify(notifier::reason_normal, "", "",
                         notifier::notification_option_none),
            OK);
  ASSERT_EQ(id + 1, _svc->get_next_notification_id());

  /* Only 100 seconds since the previous notification. */
  set_time(43300);
  id = _svc->get_next_notification_id();
  /* Because of the notification not totally implemented, we must force the
   * notification number to be greater than 0 */
  ASSERT_EQ(_svc->notify(notifier::reason_normal, "", "",
                         notifier::notification_option_none),
            OK);

  /* No notification, because the delay is too short */
  ASSERT_EQ(id, _svc->get_next_notification_id());
}

TEST_F(ServiceNotification, SimpleCheck) {
  set_time(50000);
  _svc->set_current_state(engine::service::state_ok);
  _svc->set_last_hard_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(50000);
  _svc->set_state_type(checkable::hard);
  _svc->set_accept_passive_checks(true);
  testing::internal::CaptureStdout();
  for (int i = 0; i < 3; i++) {
    // When i == 0, the state_down is soft => no notification
    // When i == 1, the state_down is soft => no notification
    // When i == 2, the state_down is hard down => notification
    set_time(50500 + i * 500);
    _svc->set_last_state(_svc->get_current_state());
    if (notifier::hard == _svc->get_state_type())
      _svc->set_last_hard_state(_svc->get_current_state());

    std::time_t now{std::time(nullptr)};
    std::string cmd{fmt::format(
        "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;service down",
        now)};
    process_external_command(cmd.c_str());
    checks::checker::instance().reap();
  }

  for (int i = 0; i < 2; i++) {
    // When i == 0, the state_up is hard (return to up) => Recovery notification
    // When i == 1, the state_up is still here (no change) => no notification
    set_time(56500 + i * 500);
    std::time_t now{std::time(nullptr)};
    std::string cmd{fmt::format(
        "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;service ok",
        now)};
    process_external_command(cmd.c_str());
    checks::checker::instance().reap();
  }
  std::string out{testing::internal::GetCapturedStdout()};
  size_t step1{out.find(
      "SERVICE ALERT: test_host;test_svc;CRITICAL;HARD;3;service down")};
  size_t step2{
      out.find("SERVICE NOTIFICATION: "
               "admin;test_host;test_svc;CRITICAL;cmd;service down",
               step1 + 1)};
  size_t step3{
      out.find("SERVICE NOTIFICATION: "
               "admin;test_host;test_svc;CRITICAL;cmd;service down",
               step2 + 1)};
  // Sent when i == 0 on the second loop.
  size_t step4{
      out.find("SERVICE NOTIFICATION: admin;test_host;test_svc;RECOVERY "
               "(OK);cmd;service ok",
               step2 + 1)};
  ASSERT_EQ(step3, std::string::npos);
  ASSERT_NE(step4, std::string::npos);
}

TEST_F(ServiceNotification, CheckFirstNotificationDelay) {
  set_time(50000);
  _svc->set_current_state(engine::service::state_ok);
  _svc->set_last_hard_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(50000);
  _svc->set_state_type(checkable::hard);
  _svc->set_first_notification_delay(3);
  testing::internal::CaptureStdout();
  std::cout << "notification interval: " << _svc->get_notification_interval()
            << std::endl;
  for (int i = 1; i < 40; i++) {
    // When i == 0, the state_down is soft => no notification
    // When i == 1, the state_down is soft => no notification
    // When i == 2, the state_down is hard down => notification
    std::cout << "Step " << i << ":";
    set_time(50000 + i * 60);
    _svc->set_last_state(_svc->get_current_state());
    if (notifier::hard == _svc->get_state_type())
      _svc->set_last_hard_state(_svc->get_current_state());
    std::time_t now{std::time(nullptr)};
    std::string cmd{fmt::format(
        "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;service "
        "critical",
        now)};
    process_external_command(cmd.c_str());
    checks::checker::instance().reap();
  }

  for (int i = 0; i < 3; i++) {
    // When i == 0, the state_up is hard (return to up) => Recovery notification
    // When i == 1, the state_up is still here (no change) => no notification
    std::cout << "New step " << i << std::endl;
    set_time(50600 + i * 60);
    std::time_t now{std::time(nullptr)};
    std::string cmd{fmt::format(
        "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;service ok",
        now)};
    process_external_command(cmd.c_str());
    checks::checker::instance().reap();
  }
  std::string out{testing::internal::GetCapturedStdout()};
  size_t m1{out.find("Step 5:")};
  size_t m2{
      out.find(" SERVICE NOTIFICATION: "
               "admin;test_host;test_svc;DOWN;cmd;service critical",
               m1 + 1)};
  size_t m3{out.find("Step 35:", m2 + 1)};
  size_t m4{
      out.find(" SERVICE NOTIFICATION: "
               "admin;test_host;test_svc;DOWN;cmd;service critical",
               m3 + 1)};
  size_t m5{
      out.find(" SERVICE NOTIFICATION: admin;test_host;test_svc;RECOVERY "
               "(OK);cmd;service ok",
               m4 + 1)};
  ASSERT_NE(m5, std::string::npos);
}

// Given a service with a notification interval = 0 and a
// first_delay_notification = 0
// When a normal notification should be sent, it is sent only one time.
TEST_F(ServiceNotification, CheckNotifIntervZero) {
  set_time(50000);
  _svc->set_current_state(engine::service::state_ok);
  _svc->set_last_hard_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(50000);
  _svc->set_state_type(checkable::hard);
  _svc->set_notification_interval(0);
  _svc->set_accept_passive_checks(true);
  testing::internal::CaptureStdout();
  for (int i = 0; i < 10; i++) {
    // When i == 0, the state_down is soft => no notification
    // When i == 1, the state_down is soft => no notification
    // When i == 2, the state_down is hard down => notification
    set_time(50500 + i * 500);
    _svc->set_last_state(_svc->get_current_state());
    if (notifier::hard == _svc->get_state_type())
      _svc->set_last_hard_state(_svc->get_current_state());

    std::time_t now{std::time(nullptr)};
    std::ostringstream oss;
    oss << '[' << now << ']'
        << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;service down";
    std::string cmd{oss.str()};
    process_external_command(cmd.c_str());
    checks::checker::instance().reap();
  }

  for (int i = 0; i < 2; i++) {
    // When i == 0, the state_up is hard (return to up) => Recovery notification
    // When i == 1, the state_up is still here (no change) => no notification
    set_time(56500 + i * 500);
    std::time_t now{std::time(nullptr)};
    std::ostringstream oss;
    oss << '[' << now << ']'
        << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;service ok";
    std::string cmd{oss.str()};
    process_external_command(cmd.c_str());
    checks::checker::instance().reap();
  }
  std::string out{testing::internal::GetCapturedStdout()};
  size_t step1{out.find(
      "SERVICE ALERT: test_host;test_svc;CRITICAL;HARD;3;service down")};
  size_t step2{
      out.find("SERVICE NOTIFICATION: "
               "admin;test_host;test_svc;CRITICAL;cmd;service down",
               step1 + 1)};
  size_t step3{
      out.find("SERVICE NOTIFICATION: "
               "admin;test_host;test_svc;CRITICAL;cmd;service down",
               step2 + 1)};
  // Sent when i == 0 on the second loop.
  size_t step4{
      out.find("SERVICE NOTIFICATION: admin;test_host;test_svc;RECOVERY "
               "(OK);cmd;service ok",
               step2 + 1)};
  ASSERT_EQ(step3, std::string::npos);
  ASSERT_NE(step4, std::string::npos);
}

// Given a service with a notification interval = 0 and a
// first_delay_notification = 0
// When a normal notification is sent and then it is recovered.
// Then at the next problem, a new normal notification is sent.
TEST_F(ServiceNotification, NormalRecoveryTwoTimes) {
  int now{50000};
  set_time(now);
  _svc->set_current_state(engine::service::state_ok);
  _svc->set_last_hard_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(50000);
  _svc->set_state_type(checkable::hard);
  _svc->set_notification_interval(0);
  _svc->set_accept_passive_checks(true);
  testing::internal::CaptureStdout();

  for (int j = 0; j < 2; j++) {
    for (int i = 0; i < 3; i++) {
      // When i == 0, the state_critical is soft => no notification
      // When i == 1, the state_critical is soft => no notification
      // When i == 2, the state_critical is hard down => notification
      now += 500;
      set_time(now);
      _svc->set_last_state(_svc->get_current_state());
      if (notifier::hard == _svc->get_state_type())
        _svc->set_last_hard_state(_svc->get_current_state());

      std::ostringstream oss;
      std::time_t now{std::time(nullptr)};
      oss << '[' << now << ']'
          << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;service "
             "critical";
      std::string cmd{oss.str()};
      process_external_command(cmd.c_str());
      checks::checker::instance().reap();
    }

    for (int i = 0; i < 2; i++) {
      // When i == 0, the state_ok is hard (return to up) => Recovery
      // notification When i == 1, the state_ok is still here (no change) => no
      // notification
      now += 500;
      set_time(now);
      std::ostringstream oss;
      std::time_t now{std::time(nullptr)};
      oss << '[' << now << ']'
          << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;service ok";
      std::string cmd{oss.str()};
      process_external_command(cmd.c_str());
      checks::checker::instance().reap();
    }
  }

  std::string out{testing::internal::GetCapturedStdout()};
  size_t step1{
      out.find("SERVICE NOTIFICATION: "
               "admin;test_host;test_svc;CRITICAL;cmd;service critical")};
  size_t step2{
      out.find("SERVICE NOTIFICATION: admin;test_host;test_svc;RECOVERY "
               "(OK);cmd;service ok",
               step1 + 1)};
  size_t step3{
      out.find("SERVICE NOTIFICATION: "
               "admin;test_host;test_svc;CRITICAL;cmd;service critical",
               step2 + 1)};
  size_t step4{
      out.find("SERVICE NOTIFICATION: admin;test_host;test_svc;RECOVERY "
               "(OK);cmd;service ok",
               step3 + 1)};
  ASSERT_NE(step4, std::string::npos);
}

// Given a service with a notification interval = 2, a
// first_delay_notification = 0, an escalation from 2 to 12 with a contactgroup
// and notification_interval = 4
// When a normal notification is sent 11 times,
// Then contacts from the escalation are notified when notification number
// is in [2,6] and are separated by at less 4*60s.
TEST_F(ServiceNotification, ServiceEscalationCG) {
  init_macros();
  configuration::applier::contact ct_aply;
  configuration::Contact ctct{
      new_pb_configuration_contact("test_contact", false)};
  ct_aply.add_object(ctct);
  ct_aply.expand_objects(pb_config);
  error_cnt err;
  ct_aply.resolve_object(ctct, err);

  configuration::applier::contactgroup cg_aply;
  configuration::Contactgroup cg;
  configuration::contactgroup_helper cg_hlp(&cg);
  fill_pb_configuration_contactgroup(&cg_hlp, "test_cg", "test_contact");
  cg_aply.add_object(cg);
  cg_aply.expand_objects(pb_config);
  cg_aply.resolve_object(cg, err);

  configuration::applier::serviceescalation se_aply;
  configuration::Serviceescalation se{new_pb_configuration_serviceescalation(
      "test_host", "test_svc", "test_cg")};
  se_aply.add_object(se);
  se_aply.expand_objects(pb_config);
  se_aply.resolve_object(se, err);

  int now{50000};
  set_time(now);

  _svc->set_current_state(engine::service::state_ok);
  _svc->set_notification_interval(1);
  _svc->set_last_hard_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(50000);
  _svc->set_state_type(checkable::hard);
  _svc->set_accept_passive_checks(true);
  _svc->set_last_hard_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(now);
  _svc->set_state_type(checkable::hard);
  _svc->set_accept_passive_checks(true);

  nagios_macros* mac(get_global_macros());

  testing::internal::CaptureStdout();
  for (int i = 0; i < 12; i++) {
    // When i == 0, the state_critical is soft => no notification
    // When i == 1, the state_critical is soft => no notification
    // When i == 2, the state_critical is hard down => notification
    now += 300;
    std::cout << "NOW = " << now << std::endl;
    set_time(now);
    _svc->set_last_state(_svc->get_current_state());
    if (notifier::hard == _svc->get_state_type())
      _svc->set_last_hard_state(_svc->get_current_state());

    std::ostringstream oss;
    std::time_t now{std::time(nullptr)};
    oss << '[' << now << ']'
        << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;service "
           "critical";
    std::string cmd{oss.str()};
    process_external_command(cmd.c_str());
    checks::checker::instance().reap();

    std::string outNOTIFICATIONTYPE;
    std::string outNOTIFICATIONNUMBER;
    std::string outNOTIFICATIONISESCALATED;
    std::string outSERVICENOTIFICATIONNUMBER;
    ASSERT_EQ(
        _svc->notify(notifier::reason_normal, "test_author", "test_comment",
                     notifier::notification_option_forced),
        OK);
    process_macros_r(mac, "$NOTIFICATIONTYPE$", outNOTIFICATIONTYPE, 0);
    process_macros_r(mac, "$NOTIFICATIONNUMBER$", outNOTIFICATIONNUMBER, 0);
    process_macros_r(mac, "$NOTIFICATIONISESCALATED$",
                     outNOTIFICATIONISESCALATED, 0);
    process_macros_r(mac, "$SERVICENOTIFICATIONNUMBER$",
                     outSERVICENOTIFICATIONNUMBER, 0);

    std::cout << " NOTIFICATIONTYPE: " << outNOTIFICATIONTYPE
              << " NOTIFICATIONNUMBER: " << outNOTIFICATIONNUMBER
              << " NOTIFICATIONISESCALATED: " << outNOTIFICATIONISESCALATED
              << " SERVICENOTIFICATIONNUMBER: " << outSERVICENOTIFICATIONNUMBER
              << std::endl;
  }

  // When i == 0, the state_ok is hard (return to up) => Recovery
  // notification When i == 1, the state_ok is still here (no change) => no
  // notification
  now += 300;
  set_time(now);
  std::ostringstream oss;
  oss << '[' << now << ']'
      << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;service ok";
  std::string cmd{oss.str()};
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();

  std::string out{testing::internal::GetCapturedStdout()};
  size_t step1{out.find("NOW = 50900")};
  size_t step2{
      out.find("SERVICE NOTIFICATION: "
               "admin;test_host;test_svc;CRITICAL;cmd;service critical",
               step1 + 1)};
  size_t step3{
      out.find("NOTIFICATIONTYPE: PROBLEM NOTIFICATIONNUMBER: 1 "
               "NOTIFICATIONISESCALATED: 0 SERVICENOTIFICATIONNUMBER: 1",
               step2 + 1)};
  size_t step4{out.find("NOW = 51200", step3 + 1)};
  size_t step5{
      out.find("SERVICE NOTIFICATION: "
               "test_contact;test_host;test_svc;CRITICAL;cmd;service critical",
               step4 + 1)};
  size_t step6{
      out.find("NOTIFICATIONTYPE: PROBLEM NOTIFICATIONNUMBER: 2 "
               "NOTIFICATIONISESCALATED: 1 SERVICENOTIFICATIONNUMBER: 2",
               step5 + 1)};
  size_t step7{out.find("NOW = 51800", step6 + 1)};
  size_t step8{
      out.find("SERVICE NOTIFICATION: "
               "test_contact;test_host;test_svc;CRITICAL;cmd;service critical",
               step7 + 1)};
  size_t step9{
      out.find("NOTIFICATIONTYPE: PROBLEM NOTIFICATIONNUMBER: 3 "
               "NOTIFICATIONISESCALATED: 1 SERVICENOTIFICATIONNUMBER: 3",
               step8 + 1)};
  size_t step10{out.find("NOW = 52400", step9 + 1)};
  size_t step11{
      out.find("SERVICE NOTIFICATION: "
               "test_contact;test_host;test_svc;CRITICAL;cmd;service critical",
               step10 + 1)};
  size_t step12{
      out.find("NOTIFICATIONTYPE: PROBLEM NOTIFICATIONNUMBER: 4 "
               "NOTIFICATIONISESCALATED: 1 SERVICENOTIFICATIONNUMBER: 4",
               step11 + 1)};
  size_t step13{out.find("NOW = 53000", step12 + 1)};
  ASSERT_NE(step13, std::string::npos);
}

TEST_F(ServiceNotification, NoServiceNotificationWhenHostDown) {
  set_time(50000);
  _svc->set_current_state(engine::service::state_ok);
  _svc->set_last_hard_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(50000);
  _svc->set_state_type(checkable::hard);
  _svc->set_accept_passive_checks(true);
  _host->set_last_hard_state_change(50000);
  _host->set_last_hard_state(engine::host::state_down);
  _host->set_state_type(checkable::hard);
  _host->set_current_state(engine::host::state_down);
  testing::internal::CaptureStdout();
  for (int i = 0; i < 3; i++) {
    // When i == 0, the state_down is soft => no notification
    // When i == 1, the state_down is soft => no notification
    // When i == 2, the state_down is hard down => no notification because host
    // down
    set_time(50500 + i * 500);
    _svc->set_last_state(_svc->get_current_state());
    if (notifier::hard == _svc->get_state_type())
      _svc->set_last_hard_state(_svc->get_current_state());

    std::time_t now{std::time(nullptr)};
    std::string cmd{fmt::format(
        "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;service down",
        now)};
    process_external_command(cmd.c_str());
    checks::checker::instance().reap();
  }

  std::string out{testing::internal::GetCapturedStdout()};
  size_t step{
      out.find("SERVICE NOTIFICATION: "
               "admin;test_host;test_svc;CRITICAL;cmd;service down")};
  ASSERT_EQ(step, std::string::npos);
}

TEST_F(ServiceNotification, WarnCritServiceNotification) {
  /* We are using a local time() function defined in tests/timeperiod/utils.cc.
   * If we call time(), it is not the glibc time() function that will be called.
   */
  ASSERT_EQ(_host->services.size(), 1u);
  set_time(43200);
  std::unique_ptr<engine::timeperiod> tperiod{
      new_timeperiod_with_timeranges("tperiod", "alias")};

  std::unique_ptr<engine::serviceescalation> service_escalation{
      new engine::serviceescalation("test_host", "test_svc", 0, 1, 1.0,
                                    "tperiod", 7, 12345)};
  _svc->set_current_state(engine::service::state_critical);
  _svc->set_last_state(engine::service::state_critical);
  _svc->set_last_hard_state_change(43200);
  _svc->set_state_type(checkable::hard);

  ASSERT_TRUE(service_escalation);
  uint64_t id{_svc->get_next_notification_id()};
  _svc->set_notification_period_ptr(tperiod.get());
  ASSERT_EQ(_svc->notify(notifier::reason_normal, "", "",
                         notifier::notification_option_none),
            OK);

  _svc->set_current_state(engine::service::state_warning);
  _svc->set_last_state(engine::service::state_warning);
  _svc->set_last_hard_state_change(43500);
  _svc->set_state_type(checkable::hard);

  ASSERT_EQ(_svc->notify(notifier::reason_normal, "", "",
                         notifier::notification_option_none),
            OK);
  ASSERT_EQ(id + 2, _svc->get_next_notification_id());
}

TEST_F(ServiceNotification, SimpleNormalVolatileServiceNotification) {
  /* We are using a local time() function defined in tests/timeperiod/utils.cc.
   * If we call time(), it is not the glibc time() function that will be called.
   */
  ASSERT_EQ(_host->services.size(), 1u);
  set_time(43200);
  std::unique_ptr<engine::timeperiod> tperiod{
      new_timeperiod_with_timeranges("tperiod", "alias")};

  std::unique_ptr<engine::serviceescalation> service_escalation{
      new engine::serviceescalation("test_host", "test_svc", 0, 1, 1.0,
                                    "tperiod", 7, 12345)};
  _svc->set_current_state(engine::service::state_critical);
  _svc->set_last_state(engine::service::state_critical);
  _svc->set_last_hard_state_change(43200);
  _svc->set_state_type(checkable::hard);
  _svc->set_is_volatile(true);

  /* Volatile => notification */
  ASSERT_TRUE(service_escalation);
  uint64_t id{_svc->get_next_notification_id()};
  _svc->set_notification_period_ptr(tperiod.get());
  ASSERT_EQ(_svc->notify(notifier::reason_normal, "", "",
                         notifier::notification_option_none),
            OK);
  ASSERT_EQ(id + 1, _svc->get_next_notification_id());

  /* Except if notifications disabled */
  id = _svc->get_next_notification_id();
  _svc->set_notification_period_ptr(tperiod.get());
  _svc->set_notifications_enabled(false);
  ASSERT_EQ(_svc->notify(notifier::reason_normal, "", "",
                         notifier::notification_option_none),
            OK);
  ASSERT_EQ(id, _svc->get_next_notification_id());

  /* or notifications are disabled globally */
  id = _svc->get_next_notification_id();
  _svc->set_notification_period_ptr(tperiod.get());
  _svc->set_notifications_enabled(true);
  pb_config.set_enable_notifications(false);
  ASSERT_EQ(_svc->notify(notifier::reason_normal, "", "",
                         notifier::notification_option_none),
            OK);
  ASSERT_EQ(id, _svc->get_next_notification_id());
}

TEST_F(ServiceNotification, RecoveryNotifEvenIfServiceAcknowledged) {
  /* We are using a local time() function defined in tests/timeperiod/utils.cc.
   * If we call time(), it is not the glibc time() function that will be called.
   */
  ASSERT_EQ(_host->services.size(), 1u);
  set_time(43200);
  std::unique_ptr<engine::timeperiod> tperiod{
      new_timeperiod_with_timeranges("tperiod", "alias")};

  std::unique_ptr<engine::serviceescalation> service_escalation{
      new engine::serviceescalation("test_host", "test_svc", 0, 1, 1.0,
                                    "tperiod", 7, 12345)};
  _svc->set_current_state(engine::service::state_critical);
  _svc->set_last_state(engine::service::state_critical);
  _svc->set_last_hard_state_change(43200);
  _svc->set_state_type(checkable::hard);

  /* Critical notification is sent */
  uint64_t id{_svc->get_next_notification_id()};
  ASSERT_EQ(_svc->notify(notifier::reason_normal, "", "",
                         notifier::notification_option_none),
            OK);
  ASSERT_EQ(id + 1, _svc->get_next_notification_id());

  set_time(43700);
  /* The service is acknowledged */
  _svc->set_acknowledgement(AckType::NORMAL);
  time_t now = time(nullptr);
  _svc->set_last_acknowledgement(now);

  id = _svc->get_next_notification_id();
  ASSERT_EQ(_svc->notify(notifier::reason_acknowledgement, "", "",
                         notifier::notification_option_none),
            OK);
  ASSERT_EQ(id + 1, _svc->get_next_notification_id());

  set_time(44000);
  /* The service is acknowledged => no more normal notification */
  id = _svc->get_next_notification_id();
  ASSERT_EQ(_svc->notify(notifier::reason_normal, "", "",
                         notifier::notification_option_none),
            OK);
  ASSERT_EQ(id, _svc->get_next_notification_id());

  set_time(44500);
  _svc->set_current_state(engine::service::state_ok);
  _svc->set_last_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(44500);
  _svc->set_state_type(checkable::hard);

  /* Critical notification is sent */
  id = _svc->get_next_notification_id();
  ASSERT_EQ(_svc->notify(notifier::reason_recovery, "", "",
                         notifier::notification_option_none),
            OK);
  ASSERT_EQ(id + 1, _svc->get_next_notification_id());
}

TEST_F(ServiceNotification, SimpleVolatileServiceNotificationWithDowntime) {
  std::unique_ptr<engine::timeperiod> tperiod{
      new_timeperiod_with_timeranges("tperiod", "alias")};
  set_time(20000);

  _svc->set_scheduled_downtime_depth(30);
  _svc->set_is_volatile(true);
  uint64_t id{_svc->get_next_notification_id()};

  std::unique_ptr<engine::serviceescalation> service_escalation{
      new engine::serviceescalation("test_host", "test_svc", 0, 1, 1.0, "", 7,
                                    12345)};
  _svc->set_notification_period_ptr(tperiod.get());

  ASSERT_TRUE(service_escalation);
  ASSERT_EQ(_svc->notify(notifier::reason_normal, "", "",
                         notifier::notification_option_none),
            OK);
  ASSERT_EQ(id, _svc->get_next_notification_id());
}

TEST_F(ServiceNotification, WarningAndTwoUsers) {
  /* admin is notified on all whereas admin1 is notified only on critical and
   * recovery. So in case of a warning notification, only admin should be
   * notified for a recovery.
   */
  set_time(50000);
  _svc->set_current_state(engine::service::state_ok);
  _svc->set_last_hard_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(50000);
  _svc->set_state_type(checkable::hard);
  _svc->set_accept_passive_checks(true);
  testing::internal::CaptureStdout();
  for (int i = 0; i < 3; i++) {
    // When i == 0, the state_down is soft => no notification
    // When i == 1, the state_down is soft => no notification
    // When i == 2, the state_down is hard down => notification ; just for admin
    set_time(50500 + i * 500);
    _svc->set_last_state(_svc->get_current_state());
    if (notifier::hard == _svc->get_state_type())
      _svc->set_last_hard_state(_svc->get_current_state());

    std::time_t now{std::time(nullptr)};
    std::string cmd(fmt::format(
        "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;1;service warn",
        now));
    process_external_command(cmd.c_str());
    checks::checker::instance().reap();
  }

  for (int i = 0; i < 2; i++) {
    // When i == 0, the state_up is hard (return to up) => Recovery notification
    // only for admin, not admin1.
    // When i == 1, the state_up is still here (no
    // change) => no notification
    set_time(56500 + i * 500);
    std::time_t now{std::time(nullptr)};
    std::string cmd(fmt::format(
        "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;service ok",
        now));
    process_external_command(cmd.c_str());
    checks::checker::instance().reap();
  }
  std::string out{testing::internal::GetCapturedStdout()};
  std::cout << out << std::endl;
  size_t warn_admin1{
      out.find("SERVICE NOTIFICATION: "
               "admin1;test_host;test_svc;WARNING;cmd;service warn")};
  ASSERT_EQ(warn_admin1, std::string::npos);
  size_t rec_admin1{
      out.find("SERVICE NOTIFICATION: admin1;test_host;test_svc;RECOVERY "
               "(OK);cmd;service ok")};
  ASSERT_EQ(rec_admin1, std::string::npos);
}

TEST_F(ServiceNotification, RecoveryTwoUsers) {
  /* admin is notified on all whereas admin1 is notified only on critical and
   * recovery. So in case of critical notification all user should be notified,
   * on warning notification only admin should be notified,
   * on recovery notification all user should be notified.
   */
  int now{50000};
  set_time(now);
  _svc->set_current_state(engine::service::state_ok);
  _svc->set_last_hard_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(50000);
  _svc->set_state_type(checkable::hard);
  _svc->set_accept_passive_checks(true);
  testing::internal::CaptureStdout();
  for (int i = 0; i < 3; i++) {
    // When i == 0, the state_down is soft => no notification
    // When i == 1, the state_down is soft => no notification
    // When i == 2, the state_down is hard down => notification ; just for admin
    now += 3000;
    std::cout << "NOW = " << now << std::endl;
    set_time(now);
    _svc->set_last_state(_svc->get_current_state());
    if (notifier::hard == _svc->get_state_type())
      _svc->set_last_hard_state(_svc->get_current_state());

    std::string cmd(fmt::format(
        "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;service crit",
        now));
    process_external_command(cmd.c_str());
    checks::checker::instance().reap();
  }

  now += 3000;
  std::cout << "NOW = " << now << std::endl;
  set_time(now);
  std::string cmd(fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;1;service warn",
      now));
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();

  now += 3000;
  std::cout << "NOW = " << now << std::endl;
  set_time(now);
  cmd = (fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;service ok",
      now));
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();
  now += 3000;
  std::cout << "NOW = " << now << std::endl;

  std::string out{testing::internal::GetCapturedStdout()};
  std::cout << out << std::endl;
  size_t step1{out.find("NOW = 59000")};
  size_t step2{
      out.find("SERVICE NOTIFICATION: "
               "admin;test_host;test_svc;CRITICAL;cmd;service crit",
               step1 + 1)};
  size_t step3{
      out.find("SERVICE NOTIFICATION: "
               "admin1;test_host;test_svc;CRITICAL;cmd;service crit",
               step2 + 1)};
  size_t step4{out.find("NOW = 62000", step3 + 1)};
  size_t step5{out.find(
      "SERVICE NOTIFICATION: admin;test_host;test_svc;WARNING;cmd;service warn",
      step4 + 1)};
  size_t step6{out.find("NOW = 65000", step5 + 1)};
  size_t step7{
      out.find("SERVICE NOTIFICATION: admin;test_host;test_svc;RECOVERY "
               "(OK);cmd;service ok",
               step6 + 1)};
  ASSERT_NE(step7, std::string::npos);
  size_t step8{
      out.find("SERVICE NOTIFICATION: admin1;test_host;test_svc;RECOVERY "
               "(OK);cmd;service ok",
               step6 + 1)};
  ASSERT_NE(step8, std::string::npos);
}
