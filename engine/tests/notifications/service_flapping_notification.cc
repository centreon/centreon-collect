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

#include "../test_engine.hh"
#include "../timeperiod/utils.hh"
#include "com/centreon/engine/checks/checker.hh"
#include "com/centreon/engine/commands/commands.hh"
#include "com/centreon/engine/configuration/applier/command.hh"
#include "com/centreon/engine/configuration/applier/contact.hh"
#include "com/centreon/engine/configuration/applier/host.hh"
#include "com/centreon/engine/configuration/applier/service.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/retention/dump.hh"
#include "com/centreon/engine/serviceescalation.hh"
#include "com/centreon/engine/timezone_manager.hh"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;
using namespace com::centreon::engine::configuration::applier;
using namespace com::centreon::engine::retention;

class ServiceFlappingNotification : public TestEngine {
 public:
  void SetUp() override {
    error_cnt err;
    init_config_state();

    configuration::applier::contact ct_aply;
    configuration::Contact ctct{new_pb_configuration_contact("admin", true)};
    ct_aply.add_object(ctct);
    ct_aply.expand_objects(pb_config);
    ct_aply.resolve_object(ctct, err);

    configuration::applier::command cmd_aply;
    configuration::Command cmd;
    configuration::command_helper cmd_hlp(&cmd);
    cmd.set_command_name("cmd");
    cmd.set_command_line("echo 1");
    cmd_aply.add_object(cmd);

    configuration::applier::host hst_aply;
    configuration::Host hst;
    configuration::host_helper hst_hlp(&hst);
    hst.set_host_name("test_host");
    hst.set_address("127.0.0.1");
    hst.set_host_id(12);
    hst.set_check_command("cmd");
    hst.set_checks_active(false);
    hst.set_checks_passive(true);
    hst_aply.add_object(hst);
    hst_aply.resolve_object(hst, err);

    configuration::applier::service svc_aply;
    configuration::Service svc;
    configuration::service_helper svc_hlp(&svc);
    svc.set_host_name("test_host");
    svc.set_service_description("test_description");
    svc.set_service_id(12);
    svc.set_check_command("cmd");
    svc_hlp.hook("contacts", "admin");

    // We fake here the expand_object on configuration::service
    svc.set_host_id(12);

    svc_aply.add_object(svc);
    svc_aply.resolve_object(svc, err);

    service_map const& sv{engine::service::services};

    _service = sv.begin()->second;
    _service->set_current_state(engine::service::state_ok);
    _service->set_state_type(checkable::hard);
    _service->set_acknowledgement(AckType::NONE);
    _service->set_notify_on(static_cast<uint32_t>(-1));

    host_map const& hm{engine::host::hosts};

    _host = hm.begin()->second;
    _host->set_current_state(engine::host::state_up);
    _host->set_state_type(checkable::hard);
    _host->set_acknowledgement(AckType::NONE);
    _host->set_notify_on(static_cast<uint32_t>(-1));
    _host->set_check_type(checkable::check_type::check_passive);
  }

  void TearDown() override {
    _service.reset();
    _host.reset();
    deinit_config_state();
  }

 protected:
  std::shared_ptr<engine::host> _host;
  std::shared_ptr<engine::service> _service;
};

// Given a Service OK
// When it is flapping
// Then it can throw a flappingstart notification followed by a recovery
// notification.
// When it is no more flapping
// Then it can throw a flappingstop notification followed by a recovery
// notification. And no recovery is sent since the notification number is 0.
TEST_F(ServiceFlappingNotification, SimpleServiceFlapping) {
  /* We are using a local time() function defined in tests/timeperiod/utils.cc.
   * If we call time(), it is not the glibc time() function that will be called.
   */
  set_time(43000);
  // FIXME DBR: should not we find a better solution than fixing this each time?
  _service->set_last_hard_state_change(43000);
  std::unique_ptr<engine::timeperiod> tperiod{
      new_timeperiod_with_timeranges("tperiod", "alias")};

  std::unique_ptr<engine::serviceescalation> service_escalation{
      new engine::serviceescalation("host_name", "test_description", 0, 1, 1.0,
                                    "tperiod", 7, 12345)};

  ASSERT_TRUE(service_escalation);
  uint64_t id{_service->get_next_notification_id()};
  _service->set_notification_period_ptr(tperiod.get());
  _service->set_is_flapping(true);
  testing::internal::CaptureStdout();
  ASSERT_EQ(_service->notify(notifier::reason_flappingstart, "", "",
                             notifier::notification_option_none),
            OK);
  ASSERT_EQ(id + 1, _service->get_next_notification_id());
  set_time(43500);
  _service->set_is_flapping(false);
  ASSERT_EQ(_service->notify(notifier::reason_flappingstop, "", "",
                             notifier::notification_option_none),
            OK);
  ASSERT_EQ(id + 2, _service->get_next_notification_id());

  ASSERT_EQ(_service->notify(notifier::reason_recovery, "", "",
                             notifier::notification_option_none),
            OK);

  std::string out{testing::internal::GetCapturedStdout()};
  size_t step1{
      out.find("SERVICE NOTIFICATION: "
               "admin;test_host;test_description;FLAPPINGSTART (OK);cmd;")};
  size_t step2{
      out.find("SERVICE NOTIFICATION: "
               "admin;test_host;test_description;FLAPPINGSTART (OK);cmd;")};
  ASSERT_NE(step1, std::string::npos);
  ASSERT_NE(step2, std::string::npos);
  ASSERT_LE(step1, step2);
}

// Given a Service OK
// When it is flapping
// Then it can throw a flappingstart notification followed by a recovery
// notification.
// When a second flappingstart notification is sent
// Then no notification is sent (because already sent).
TEST_F(ServiceFlappingNotification, SimpleServiceFlappingStartTwoTimes) {
  /* We are using a local time() function defined in tests/timeperiod/utils.cc.
   * If we call time(), it is not the glibc time() function that will be called.
   */
  set_time(43000);
  _service->set_notification_interval(2);
  std::unique_ptr<engine::timeperiod> tperiod{
      new_timeperiod_with_timeranges("tperiod", "alias")};

  std::unique_ptr<engine::serviceescalation> service_escalation{
      new engine::serviceescalation("host_name", "test_description", 0, 1, 1.0,
                                    "tperiod", 7, 12345)};

  ASSERT_TRUE(service_escalation);
  uint64_t id{_service->get_next_notification_id()};
  _service->set_notification_period_ptr(tperiod.get());
  _service->set_is_flapping(true);
  ASSERT_EQ(_service->notify(notifier::reason_flappingstart, "", "",
                             notifier::notification_option_none),
            OK);
  ASSERT_EQ(id + 1, _service->get_next_notification_id());

  set_time(43050);
  /* Notification already sent, no notification should be sent. */
  ASSERT_EQ(_service->notify(notifier::reason_flappingstart, "", "",
                             notifier::notification_option_none),
            OK);
  ASSERT_EQ(id + 1, _service->get_next_notification_id());
}

// Given a service OK
// When it is flapping
// Then it can throw a flappingstart notification followed by a recovery
// notification.
// When a flappingstop notification is sent
// Then it is sent.
// When a second flappingstop notification is sent
// Then nothing is sent.
TEST_F(ServiceFlappingNotification, SimpleServiceFlappingStopTwoTimes) {
  /* We are using a local time() function defined in tests/timeperiod/utils.cc.
   * If we call time(), it is not the glibc time() function that will be called.
   */
  set_time(43000);
  _service->set_notification_interval(2);
  std::unique_ptr<engine::timeperiod> tperiod{
      new_timeperiod_with_timeranges("tperiod", "alias")};

  std::unique_ptr<engine::serviceescalation> service_escalation{
      new engine::serviceescalation("host_name", "test_description", 0, 1, 1.0,
                                    "tperiod", 7, 12345)};

  ASSERT_TRUE(service_escalation);
  uint64_t id{_service->get_next_notification_id()};
  _service->set_notification_period_ptr(tperiod.get());
  _service->set_is_flapping(true);
  ASSERT_EQ(_service->notify(notifier::reason_flappingstart, "", "",
                             notifier::notification_option_none),
            OK);
  ASSERT_EQ(id + 1, _service->get_next_notification_id());

  set_time(43050);
  /* Flappingstop notification: sent. */
  ASSERT_EQ(_service->notify(notifier::reason_flappingstop, "", "",
                             notifier::notification_option_none),
            OK);
  ASSERT_EQ(id + 2, _service->get_next_notification_id());

  set_time(43100);
  /* Second flappingstop notification: not sent. */
  ASSERT_EQ(_service->notify(notifier::reason_flappingstop, "", "",
                             notifier::notification_option_none),
            OK);
  ASSERT_EQ(id + 2, _service->get_next_notification_id());
}

TEST_F(ServiceFlappingNotification, CheckFlapping) {
  pb_config.set_enable_flap_detection(true);
  _service->set_flap_detection_enabled(true);
  _service->add_flap_detection_on(engine::service::ok);
  _service->add_flap_detection_on(engine::service::down);
  _service->set_notification_interval(1);
  time_t now = 45000;
  set_time(now);
  _service->set_current_state(engine::service::state_ok);
  _service->set_last_hard_state(engine::service::state_ok);
  _service->set_last_hard_state_change(50000);
  _service->set_state_type(checkable::hard);
  _service->set_first_notification_delay(3);
  _service->set_max_attempts(1);

  // This loop is to store many OK in the state history.
  for (int i = 1; i < 22; i++) {
    now += 300;
    std::cout << "NOW = " << now << std::endl;
    set_time(now);
    _service->set_last_state(_service->get_current_state());
    if (notifier::hard == _service->get_state_type())
      _service->set_last_hard_state(_service->get_current_state());

    std::ostringstream oss;
    std::time_t now{std::time(nullptr)};
    oss << '[' << now << ']'
        << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_description;0;service "
           "ok";
    std::string cmd{oss.str()};
    process_external_command(cmd.c_str());
    checks::checker::instance().reap();
  }

  testing::internal::CaptureStdout();
  // This loop is to store many CRITICAL or OK in the state history to start the
  // flapping.
  for (int i = 1; i < 8; i++) {
    now += 300;
    std::cout << "NOW = " << now << std::endl;
    set_time(now);
    _service->set_last_state(_service->get_current_state());
    if (notifier::hard == _service->get_state_type())
      _service->set_last_hard_state(_service->get_current_state());

    std::ostringstream oss;
    std::time_t now{std::time(nullptr)};
    oss << '[' << now << ']'
        << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_description;"
        << ((i % 2 == 1) ? "2;service critical" : "0;service ok");
    std::string cmd{oss.str()};
    process_external_command(cmd.c_str());
    checks::checker::instance().reap();
  }

  // This loop is to store many CRITICAL in the state history to stop the
  // flapping.
  for (int i = 1; i < 18; i++) {
    std::cout << "Step " << i << ":";
    now += 300;
    std::cout << "NOW = " << now << std::endl;
    set_time(now);
    _service->set_last_state(_service->get_current_state());
    if (notifier::hard == _service->get_state_type())
      _service->set_last_hard_state(_service->get_current_state());
    std::ostringstream oss;
    std::time_t now{std::time(nullptr)};
    oss << '[' << now << ']'
        << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_description;2;service "
           "critical";
    std::string cmd{oss.str()};
    process_external_command(cmd.c_str());
    checks::checker::instance().reap();
  }

  std::string out{testing::internal::GetCapturedStdout()};
  size_t m1{out.find("NOW = 53100")};
  size_t m2{
      out.find("SERVICE NOTIFICATION: "
               "admin;test_host;test_description;FLAPPINGSTART (CRITICAL);cmd;",
               m1)};
  size_t m3{out.find("Step 7:", m2)};
  size_t m4{out.find("Step 16:", m3)};
  size_t m5{out.find(
      "SERVICE FLAPPING ALERT: test_host;test_description;STOPPED;", m4)};
  size_t m6{
      out.find("SERVICE NOTIFICATION: "
               "admin;test_host;test_description;FLAPPINGSTOP (CRITICAL);cmd;",
               m5)};
  ASSERT_NE(m6, std::string::npos);
}

TEST_F(ServiceFlappingNotification, CheckFlappingWithVolatile) {
  pb_config.set_enable_flap_detection(true);
  _service->set_flap_detection_enabled(true);
  _service->set_is_volatile(true);
  _service->add_flap_detection_on(engine::service::ok);
  _service->add_flap_detection_on(engine::service::down);
  _service->set_notification_interval(1);
  time_t now = 45000;
  set_time(now);
  _service->set_current_state(engine::service::state_ok);
  _service->set_last_hard_state(engine::service::state_ok);
  _service->set_last_hard_state_change(50000);
  _service->set_state_type(checkable::hard);
  _service->set_first_notification_delay(3);
  _service->set_max_attempts(1);

  // This loop is to store many OK in the state history.
  for (int i = 1; i < 22; i++) {
    now += 300;
    std::cout << "NOW = " << now << std::endl;
    set_time(now);
    _service->set_last_state(_service->get_current_state());
    if (notifier::hard == _service->get_state_type())
      _service->set_last_hard_state(_service->get_current_state());

    std::ostringstream oss;
    std::time_t now{std::time(nullptr)};
    oss << '[' << now << ']'
        << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_description;0;service "
           "ok";
    std::string cmd{oss.str()};
    process_external_command(cmd.c_str());
    checks::checker::instance().reap();
  }

  testing::internal::CaptureStdout();
  // This loop is to store many CRITICAL or OK in the state history to start the
  // flapping.
  for (int i = 1; i < 8; i++) {
    now += 300;
    std::cout << "NOW = " << now << std::endl;
    set_time(now);
    _service->set_last_state(_service->get_current_state());
    if (notifier::hard == _service->get_state_type())
      _service->set_last_hard_state(_service->get_current_state());

    std::ostringstream oss;
    std::time_t now{std::time(nullptr)};
    oss << '[' << now << ']'
        << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_description;"
        << ((i % 2 == 1) ? "2;service critical" : "0;service ok");
    std::string cmd{oss.str()};
    process_external_command(cmd.c_str());
    checks::checker::instance().reap();
  }

  // This loop is to store many CRITICAL in the state history to stop the
  // flapping.
  for (int i = 1; i < 18; i++) {
    std::cout << "Step " << i << ":";
    now += 300;
    std::cout << "NOW = " << now << std::endl;
    set_time(now);
    _service->set_last_state(_service->get_current_state());
    if (notifier::hard == _service->get_state_type())
      _service->set_last_hard_state(_service->get_current_state());
    std::ostringstream oss;
    std::time_t now{std::time(nullptr)};
    oss << '[' << now << ']'
        << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_description;2;service "
           "critical";
    std::string cmd{oss.str()};
    process_external_command(cmd.c_str());
    checks::checker::instance().reap();
  }

  std::string out{testing::internal::GetCapturedStdout()};
  size_t m1{out.find("NOW = 53100")};
  size_t m2{
      out.find("SERVICE NOTIFICATION: "
               "admin;test_host;test_description;FLAPPINGSTART (CRITICAL);cmd;",
               m1)};
  size_t m3{out.find("Step 7:", m2)};
  size_t m4{out.find("Step 16:", m3)};
  size_t m5{out.find(
      "SERVICE FLAPPING ALERT: test_host;test_description;STOPPED;", m4)};
  size_t m6{
      out.find("SERVICE NOTIFICATION: "
               "admin;test_host;test_description;FLAPPINGSTOP (CRITICAL);cmd;",
               m5)};
  ASSERT_NE(m6, std::string::npos);
  size_t m7{out.find("NOW = 57300")};
  size_t m8{
      out.find("SERVICE NOTIFICATION: "
               "admin;test_host;test_description;CRITICAL;cmd;service critical",
               m7)};
  size_t m9{out.find("NOW = 57600", m8)};
  ASSERT_EQ(m9, std::string::npos);
}

/**
 * @brief Given a host down, we generate a flapping service and notifications
 * should not be called
 *
 */
TEST_F(ServiceFlappingNotification, CheckFlappingWithHostDown) {
  _host->set_current_state(engine::host::state_down);
  _host->set_state_type(checkable::hard);
  _host->set_check_type(checkable::check_type::check_passive);
  pb_config.set_enable_flap_detection(true);
  _service->set_flap_detection_enabled(true);
  _service->add_flap_detection_on(engine::service::ok);
  _service->add_flap_detection_on(engine::service::down);
  _service->set_notification_interval(1);
  time_t now = 45000;
  set_time(now);
  _service->set_current_state(engine::service::state_ok);
  _service->set_last_hard_state(engine::service::state_ok);
  _service->set_last_hard_state_change(50000);
  _service->set_state_type(checkable::hard);
  _service->set_first_notification_delay(3);
  _service->set_max_attempts(1);

  commands_logger->set_level(spdlog::level::trace);

  // This loop is to store many OK in the state history.
  for (int i = 1; i < 22; i++) {
    now += 300;
    std::cout << "NOW = " << now << std::endl;
    set_time(now);
    _service->set_last_state(_service->get_current_state());
    if (notifier::hard == _service->get_state_type())
      _service->set_last_hard_state(_service->get_current_state());

    std::ostringstream oss;
    std::time_t now{std::time(nullptr)};
    oss << '[' << now << ']'
        << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_description;0;service "
           "ok";
    std::string cmd{oss.str()};
    process_external_command(cmd.c_str());
    checks::checker::instance().reap();
  }

  testing::internal::CaptureStdout();
  // This loop is to store many CRITICAL or OK in the state history to start the
  // flapping.
  for (int i = 1; i < 8; i++) {
    now += 300;
    std::cout << "NOW = " << now << std::endl;
    set_time(now);
    _service->set_last_state(_service->get_current_state());
    if (notifier::hard == _service->get_state_type())
      _service->set_last_hard_state(_service->get_current_state());

    std::ostringstream oss;
    std::time_t now{std::time(nullptr)};
    oss << '[' << now << ']'
        << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_description;"
        << ((i % 2 == 1) ? "2;service critical" : "0;service ok");
    std::string cmd{oss.str()};
    process_external_command(cmd.c_str());
    checks::checker::instance().reap();
  }

  // This loop is to store many CRITICAL in the state history to stop the
  // flapping.
  for (int i = 1; i < 18; i++) {
    std::cout << "Step " << i << ":";
    now += 300;
    std::cout << "NOW = " << now << std::endl;
    set_time(now);
    _service->set_last_state(_service->get_current_state());
    if (notifier::hard == _service->get_state_type())
      _service->set_last_hard_state(_service->get_current_state());
    std::ostringstream oss;
    std::time_t now{std::time(nullptr)};
    oss << '[' << now << ']'
        << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_description;2;service "
           "critical";
    std::string cmd{oss.str()};
    process_external_command(cmd.c_str());
    checks::checker::instance().reap();
  }

  std::string out{testing::internal::GetCapturedStdout()};
  size_t m1{out.find(
      "SERVICE NOTIFICATION: "
      "admin;test_host;test_description;FLAPPINGSTART (CRITICAL);cmd;")};
  size_t m2{
      out.find("SERVICE FLAPPING ALERT: test_host;test_description;STOPPED;")};
  size_t m3{out.find(
      "SERVICE NOTIFICATION: "
      "admin;test_host;test_description;FLAPPINGSTOP (CRITICAL);cmd;")};
  ASSERT_EQ(m1, std::string::npos);
  ASSERT_EQ(m2, std::string::npos);
  ASSERT_EQ(m3, std::string::npos);
}

TEST_F(ServiceFlappingNotification, CheckFlappingWithSoftState) {
  pb_config.set_enable_flap_detection(true);
  _service->set_flap_detection_enabled(true);
  _service->add_flap_detection_on(engine::service::ok);
  _service->add_flap_detection_on(engine::service::down);
  _service->set_notification_interval(1);
  time_t now = 45000;
  set_time(now);
  _service->set_current_state(engine::service::state_ok);
  _service->set_last_hard_state(engine::service::state_ok);
  _service->set_last_hard_state_change(50000);
  _service->set_state_type(checkable::hard);
  _service->set_first_notification_delay(0);
  _service->set_max_attempts(3);

  // This loop is to store many OK in the state history.
  for (int i = 1; i < 22; i++) {
    now += 300;
    std::cout << "NOW = " << now << std::endl;
    set_time(now);
    _service->set_last_state(_service->get_current_state());
    if (notifier::hard == _service->get_state_type())
      _service->set_last_hard_state(_service->get_current_state());

    std::ostringstream oss;
    std::time_t now{std::time(nullptr)};
    oss << '[' << now << ']'
        << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_description;0;service "
           "ok";
    std::string cmd{oss.str()};
    process_external_command(cmd.c_str());
    checks::checker::instance().reap();
  }

  testing::internal::CaptureStdout();
  // This loop is to store many CRITICAL or OK in the state history to start the
  // flapping.
  for (int i = 1; i < 8; i++) {
    now += 300;
    std::cout << "NOW = " << now << std::endl;
    set_time(now);
    _service->set_last_state(_service->get_current_state());
    if (notifier::hard == _service->get_state_type())
      _service->set_last_hard_state(_service->get_current_state());

    std::ostringstream oss;
    std::time_t now{std::time(nullptr)};
    oss << '[' << now << ']'
        << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_description;"
        << ((i % 2 == 1) ? "2;service critical" : "0;service ok");
    std::string cmd{oss.str()};
    process_external_command(cmd.c_str());
    checks::checker::instance().reap();
  }

  // This loop is to store many CRITICAL in the state history to stop the
  // flapping.
  for (int i = 1; i < 18; i++) {
    std::cout << "Step " << i << ":";
    now += 300;
    std::cout << "NOW = " << now << std::endl;
    set_time(now);
    _service->set_last_state(_service->get_current_state());
    if (notifier::hard == _service->get_state_type())
      _service->set_last_hard_state(_service->get_current_state());
    std::ostringstream oss;
    std::time_t now{std::time(nullptr)};
    oss << '[' << now << ']'
        << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_description;2;service "
           "critical";
    std::string cmd{oss.str()};
    process_external_command(cmd.c_str());
    checks::checker::instance().reap();
  }

  std::string out{testing::internal::GetCapturedStdout()};
  size_t m1{out.find(
      "SERVICE NOTIFICATION: "
      "admin;test_host;test_description;FLAPPINGSTART (CRITICAL);cmd;")};
  size_t m2{
      out.find("SERVICE FLAPPING ALERT: test_host;test_description;STOPPED;")};
  size_t m3{out.find(
      "SERVICE NOTIFICATION: "
      "admin;test_host;test_description;FLAPPINGSTOP (CRITICAL);cmd;")};
  ASSERT_NE(m1, std::string::npos);
  ASSERT_NE(m2, std::string::npos);
  ASSERT_NE(m3, std::string::npos);
}

TEST_F(ServiceFlappingNotification, RetentionFlappingNotification) {
  pb_config.set_enable_flap_detection(true);
  _service->set_flap_detection_enabled(true);
  _service->add_flap_detection_on(engine::service::ok);
  _service->add_flap_detection_on(engine::service::down);
  _service->set_notification_interval(1);
  time_t now = 45000;
  set_time(now);
  _service->set_current_state(engine::service::state_ok);
  _service->set_last_hard_state(engine::service::state_ok);
  _service->set_last_hard_state_change(50000);
  _service->set_state_type(checkable::hard);
  _service->set_first_notification_delay(3);
  _service->set_max_attempts(1);

  // This loop is to store many OK in the state history.
  for (int i = 1; i < 22; i++) {
    now += 300;
    std::cout << "NOW = " << now << std::endl;
    set_time(now);
    _service->set_last_state(_service->get_current_state());
    if (notifier::hard == _service->get_state_type())
      _service->set_last_hard_state(_service->get_current_state());

    std::ostringstream oss;
    std::time_t now{std::time(nullptr)};
    oss << '[' << now << ']'
        << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_description;0;service "
           "ok";
    std::string cmd{oss.str()};
    process_external_command(cmd.c_str());
    checks::checker::instance().reap();
  }

  // This loop is to store many CRITICAL or OK in the state history to start the
  // flapping.
  for (int i = 1; i < 8; i++) {
    now += 300;
    std::cout << "NOW = " << now << std::endl;
    set_time(now);
    _service->set_last_state(_service->get_current_state());
    if (notifier::hard == _service->get_state_type())
      _service->set_last_hard_state(_service->get_current_state());

    std::ostringstream oss;
    std::time_t now{std::time(nullptr)};
    oss << '[' << now << ']'
        << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_description;"
        << ((i % 2 == 1) ? "2;service critical" : "0;service ok");
    std::string cmd{oss.str()};
    process_external_command(cmd.c_str());
    checks::checker::instance().reap();
  }

  // This loop is to store many CRITICAL in the state history to stop the
  // flapping.
  for (int i = 1; i < 18; i++) {
    std::cout << "Step " << i << ":";
    now += 300;
    std::cout << "NOW = " << now << std::endl;
    set_time(now);
    _service->set_last_state(_service->get_current_state());
    if (notifier::hard == _service->get_state_type())
      _service->set_last_hard_state(_service->get_current_state());
    std::ostringstream oss;
    std::time_t now{std::time(nullptr)};
    oss << '[' << now << ']'
        << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_description;2;service "
           "critical";
    std::string cmd{oss.str()};
    process_external_command(cmd.c_str());
    checks::checker::instance().reap();
  }

  std::ostringstream oss;
  dump::service(oss, *_service);
  std::string retention{oss.str()};

  std::size_t pos =
      retention.find("notification_3=") + strlen("notification_3=");
  std::size_t end = retention.find("\n", pos + 1);
  std::string notification0 = retention.substr(pos, end - pos);
  _service->set_notification(0, notification0);
  oss.str("");

  dump::service(oss, *_service);
  retention = oss.str();
  pos = retention.find("notification_3=") + strlen("notification_3=");
  end = retention.find("\n", pos + 1);
  std::string notification1 = retention.substr(pos, end - pos);

  ASSERT_EQ(notification0, notification1);

  notification0 =
      "type: 4, author: admin, options: 3, escalated: 1, id: 9, number: 2, "
      "interval: 3, contacts: ";
  _service->set_notification(3, notification0);
  oss.str("");

  dump::service(oss, *_service);
  retention = oss.str();
  pos = retention.find("notification_3=") + strlen("notification_3=");
  end = retention.find("\n", pos + 1);
  notification1 = retention.substr(pos, end - pos);

  ASSERT_EQ(notification0, notification1);
}
