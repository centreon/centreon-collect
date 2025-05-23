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

#include <cstring>

#include <regex>

#include "../test_engine.hh"
#include "../timeperiod/utils.hh"
#include "com/centreon/engine/configuration/applier/contact.hh"
#include "com/centreon/engine/configuration/applier/host.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/host.hh"
#include "com/centreon/engine/hostescalation.hh"
#include "com/centreon/engine/timezone_manager.hh"
#include "common/engine_conf/contact_helper.hh"
#include "common/engine_conf/host_helper.hh"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;
using namespace com::centreon::engine::configuration::applier;

class HostFlappingNotification : public TestEngine {
 public:
  void SetUp() override {
    configuration::error_cnt err;
    init_config_state();

    configuration::applier::contact ct_aply;
    configuration::Contact ctct{new_pb_configuration_contact("admin", true)};
    ct_aply.add_object(ctct);
    ct_aply.expand_objects(pb_config);
    ct_aply.resolve_object(ctct, err);

    configuration::applier::host hst_aply;
    configuration::Host hst;
    configuration::host_helper hst_hlp(&hst);
    hst.set_host_name("test_host");
    hst.set_address("127.0.0.1");
    hst.set_host_id(12);
    hst_hlp.hook("contacts", "admin");
    hst_aply.add_object(hst);
    hst_aply.resolve_object(hst, err);
    host_map const& hm{engine::host::hosts};
    _host = hm.begin()->second;
    _host->set_current_state(engine::host::state_up);
    _host->set_state_type(checkable::hard);
    _host->set_acknowledgement(AckType::NONE);
    _host->set_notify_on(static_cast<uint32_t>(-1));

    configuration::Host hst_child;
    configuration::host_helper hst_child_hlp(&hst_child);
    hst_child.set_host_name("child_host");
    hst_child.set_address("127.0.0.1");
    hst_child_hlp.hook("parents", "test_host");
    hst_child.set_host_id(13);
    hst_child_hlp.hook("contacts", "admin");
    hst_aply.add_object(hst_child);
    hst_aply.resolve_object(hst_child, err);

    _host2 = hm.begin()->second;
    _host2->set_current_state(engine::host::state_up);
    _host2->set_state_type(checkable::hard);
    _host2->set_acknowledgement(AckType::NONE);
    _host2->set_notify_on(static_cast<uint32_t>(-1));
  }

  void TearDown() override {
    _host.reset();
    _host2.reset();
    deinit_config_state();
  }

 protected:
  std::shared_ptr<engine::host> _host;
  std::shared_ptr<engine::host> _host2;
};

// Given a host UP
// When it is flapping
// Then it can throw a flappingstart notification followed by a recovery
// notification.
// When it is no more flapping
// Then it can throw a flappingstop notification followed by a recovery
// notification. And no recovery is sent since the notification number is 0.
TEST_F(HostFlappingNotification, SimpleHostFlapping) {
  /* We are using a local time() function defined in tests/timeperiod/utils.cc.
   * If we call time(), it is not the glibc time() function that will be called.
   */
  set_time(43000);
  _host->set_last_hard_state_change(43000);
  std::unique_ptr<engine::timeperiod> tperiod{
      new_timeperiod_with_timeranges("tperiod", "alias")};

  std::unique_ptr<engine::hostescalation> host_escalation =
      std::make_unique<engine::hostescalation>("host_name", 0, 1, 1.0,
                                               "tperiod", 7, 12345);

  ASSERT_TRUE(host_escalation);
  uint64_t id{_host->get_next_notification_id()};
  _host->set_notification_period_ptr(tperiod.get());
  _host->set_is_flapping(true);
  testing::internal::CaptureStdout();
  ASSERT_EQ(_host->notify(notifier::reason_flappingstart, "", "",
                          notifier::notification_option_none),
            OK);
  ASSERT_EQ(id + 1, _host->get_next_notification_id());
  set_time(43500);
  _host->set_is_flapping(false);
  ASSERT_EQ(_host->notify(notifier::reason_flappingstop, "", "",
                          notifier::notification_option_none),
            OK);
  ASSERT_EQ(id + 2, _host->get_next_notification_id());

  ASSERT_EQ(_host->notify(notifier::reason_recovery, "", "",
                          notifier::notification_option_none),
            OK);

  std::string out{testing::internal::GetCapturedStdout()};
  size_t step1{
      out.find("HOST NOTIFICATION: admin;test_host;FLAPPINGSTART (UP);cmd;")};
  size_t step2{
      out.find("HOST NOTIFICATION: admin;test_host;FLAPPINGSTART (UP);cmd;")};
  ASSERT_NE(step1, std::string::npos);
  ASSERT_NE(step2, std::string::npos);
  ASSERT_LE(step1, step2);
}

// Given a host UP
// When it is flapping
// Then it can throw a flappingstart notification followed by a recovery
// notification.
// When a second flappingstart notification is sent
// Then no notification is sent (because already sent).
TEST_F(HostFlappingNotification, SimpleHostFlappingStartTwoTimes) {
  /* We are using a local time() function defined in tests/timeperiod/utils.cc.
   * If we call time(), it is not the glibc time() function that will be called.
   */
  set_time(43000);
  _host->set_notification_interval(2);
  configuration::Timeperiod tp;
  configuration::timeperiod_helper tp_hlp(&tp);
  tp.set_timeperiod_name("tperiod");
  tp.set_alias("alias");
#define add_day(day)                                \
  {                                                 \
    auto* d = tp.mutable_timeranges()->add_##day(); \
    d->set_range_start(0);                          \
    d->set_range_end(86400);                        \
  }

  add_day(sunday);
  add_day(monday);
  add_day(tuesday);
  add_day(wednesday);
  add_day(thursday);
  add_day(friday);
  add_day(saturday);

  std::unique_ptr<engine::timeperiod> tperiod{new engine::timeperiod(tp)};

  std::unique_ptr<engine::hostescalation> host_escalation{
      new engine::hostescalation("host_name", 0, 1, 1.0, "tperiod", 7, 12345)};

  ASSERT_TRUE(host_escalation);
  uint64_t id{_host->get_next_notification_id()};
  _host->set_notification_period_ptr(tperiod.get());
  _host->set_is_flapping(true);
  ASSERT_EQ(_host->notify(notifier::reason_flappingstart, "", "",
                          notifier::notification_option_none),
            OK);
  ASSERT_EQ(id + 1, _host->get_next_notification_id());

  set_time(43050);
  /* Notification already sent, no notification should be sent. */
  ASSERT_EQ(_host->notify(notifier::reason_flappingstart, "", "",
                          notifier::notification_option_none),
            OK);
  ASSERT_EQ(id + 1, _host->get_next_notification_id());
}

// Given a host UP
// When it is flapping
// Then it can throw a flappingstart notification followed by a recovery
// notification.
// When a flappingstop notification is sent
// Then it is sent.
// When a second flappingstop notification is sent
// Then nothing is sent.
TEST_F(HostFlappingNotification, SimpleHostFlappingStopTwoTimes) {
  /* We are using a local time() function defined in tests/timeperiod/utils.cc.
   * If we call time(), it is not the glibc time() function that will be called.
   */
  set_time(43000);
  _host->set_notification_interval(2);
  configuration::Timeperiod tp;
  configuration::timeperiod_helper tp_hlp(&tp);
  tp.set_timeperiod_name("tperiod");
  tp.set_alias("alias");
#define add_day(day)                                \
  {                                                 \
    auto* d = tp.mutable_timeranges()->add_##day(); \
    d->set_range_start(0);                          \
    d->set_range_end(86400);                        \
  }

  add_day(sunday);
  add_day(monday);
  add_day(tuesday);
  add_day(wednesday);
  add_day(thursday);
  add_day(friday);
  add_day(saturday);

  std::unique_ptr<engine::timeperiod> tperiod{new engine::timeperiod(tp)};

  std::unique_ptr<engine::hostescalation> host_escalation{
      new engine::hostescalation("host_name", 0, 1, 1.0, "tperiod", 7, 12345)};

  ASSERT_TRUE(host_escalation);
  uint64_t id{_host->get_next_notification_id()};
  _host->set_notification_period_ptr(tperiod.get());
  _host->set_is_flapping(true);
  ASSERT_EQ(_host->notify(notifier::reason_flappingstart, "", "",
                          notifier::notification_option_none),
            OK);
  ASSERT_EQ(id + 1, _host->get_next_notification_id());

  set_time(43050);
  /* Flappingstop notification: sent. */
  ASSERT_EQ(_host->notify(notifier::reason_flappingstop, "", "",
                          notifier::notification_option_none),
            OK);
  ASSERT_EQ(id + 2, _host->get_next_notification_id());

  set_time(43100);
  /* Second flappingstop notification: not sent. */
  ASSERT_EQ(_host->notify(notifier::reason_flappingstop, "", "",
                          notifier::notification_option_none),
            OK);
  ASSERT_EQ(id + 2, _host->get_next_notification_id());
}

TEST_F(HostFlappingNotification, CheckFlapping) {
  pb_config.set_enable_flap_detection(true);
  _host->set_flap_detection_enabled(true);
  _host->add_flap_detection_on(engine::host::up);
  _host->add_flap_detection_on(engine::host::down);
  _host->set_notification_interval(1);
  set_time(45000);
  _host->set_current_state(engine::host::state_up);
  _host->set_last_hard_state(engine::host::state_up);
  _host->set_last_hard_state_change(50000);
  _host->set_state_type(checkable::hard);
  _host->set_first_notification_delay(3);

  // This loop is to store many UP in the state history.
  for (int i = 1; i < 22; i++) {
    set_time(45000 + i * 60);
    _host->set_last_state(_host->get_current_state());
    if (notifier::hard == _host->get_state_type())
      _host->set_last_hard_state(_host->get_current_state());
    _host->process_check_result_3x(engine::host::state_up, "The host is up",
                                   CHECK_OPTION_NONE, 0, true, 0);
  }

  // This loop is to store many UP or DOWN in the state history to start
  // flapping.
  testing::internal::CaptureStdout();
  for (int i = 1; i < 12; i++) {
    std::cout << "Step " << i << ":";
    set_time(50000 + i * 60);
    _host->set_last_state(_host->get_current_state());
    if (notifier::hard == _host->get_state_type())
      _host->set_last_hard_state(_host->get_current_state());
    _host->process_check_result_3x(
        i % 3 == 0 ? engine::host::state_up : engine::host::state_down,
        "The host is flapping", CHECK_OPTION_NONE, 0, true, 0);
  }

  // This loop is to store many DOWN in the state history to stop flapping.
  for (int i = 1; i < 18; i++) {
    std::cout << "Step " << i << "  :";
    set_time(50420 + i * 60);
    _host->set_last_state(_host->get_current_state());
    if (notifier::hard == _host->get_state_type())
      _host->set_last_hard_state(_host->get_current_state());
    _host->process_check_result_3x(engine::host::state_down,
                                   "The host is flapping", CHECK_OPTION_NONE, 0,
                                   true, 0);
  }

  std::string out{testing::internal::GetCapturedStdout()};
  size_t m1{out.find("Step 6:")};
  size_t m2{out.find(
      "HOST NOTIFICATION: admin;test_host;FLAPPINGSTART (DOWN);cmd;", m1)};
  size_t m3{out.find("Step 11:", m2)};
  size_t m4{out.find("Step 13  :", m3)};
  size_t m5{out.find("HOST FLAPPING ALERT: test_host;STOPPED;", m4)};
  size_t m6{out.find(
      "HOST NOTIFICATION: admin;test_host;FLAPPINGSTOP (DOWN);cmd;", m5)};
  ASSERT_NE(m6, std::string::npos);
}

TEST_F(HostFlappingNotification, CheckFlappingWithHostParentDown) {
  pb_config.set_enable_flap_detection(true);
  _host->set_current_state(engine::host::state_down);
  _host->set_last_hard_state(engine::host::state_down);
  _host->set_state_type(checkable::hard);
  _host2->set_flap_detection_enabled(true);
  _host2->add_flap_detection_on(engine::host::up);
  _host2->add_flap_detection_on(engine::host::down);
  _host2->add_flap_detection_on(engine::host::unreachable);
  _host2->set_notification_interval(1);
  set_time(45000);
  _host2->set_current_state(engine::host::state_up);
  _host2->set_last_hard_state(engine::host::state_up);
  _host2->set_last_hard_state_change(50000);
  _host2->set_state_type(checkable::hard);
  _host2->set_first_notification_delay(3);

  // This loop is to store many UP in the state history.
  for (int i = 1; i < 22; i++) {
    set_time(45000 + i * 60);
    _host2->set_last_state(_host2->get_current_state());
    if (notifier::hard == _host2->get_state_type())
      _host2->set_last_hard_state(_host2->get_current_state());
    _host2->process_check_result_3x(engine::host::state_up, "The host is up",
                                    CHECK_OPTION_NONE, 0, true, 0);
  }

  // This loop is to store many UP or DOWN in the state history to start
  // flapping.
  testing::internal::CaptureStdout();
  for (int i = 1; i < 12; i++) {
    std::cout << "Step " << i << ":";
    set_time(50000 + i * 60);
    _host2->set_last_state(_host2->get_current_state());
    if (notifier::hard == _host2->get_state_type())
      _host2->set_last_hard_state(_host2->get_current_state());
    _host2->process_check_result_3x(
        i % 3 == 0 ? engine::host::state_up : engine::host::state_unreachable,
        "The host is flapping", CHECK_OPTION_NONE, 0, true, 0);
  }

  // This loop is to store many DOWN in the state history to stop flapping.
  for (int i = 1; i < 18; i++) {
    std::cout << "Step " << i << "  :";
    set_time(50420 + i * 60);
    _host2->set_last_state(_host2->get_current_state());
    if (notifier::hard == _host2->get_state_type())
      _host2->set_last_hard_state(_host2->get_current_state());
    _host2->process_check_result_3x(engine::host::state_down,
                                    "The host is flapping", CHECK_OPTION_NONE,
                                    0, true, 0);
  }

  std::string out{testing::internal::GetCapturedStdout()};
  size_t m1{out.find(
      "HOST NOTIFICATION: admin;child_host;FLAPPINGSTART (DOWN);cmd;")};
  size_t m2{out.find("HOST FLAPPING ALERT: child_host;STOPPED;")};
  size_t m3{
      out.find("HOST NOTIFICATION: admin;child_host;FLAPPINGSTOP (DOWN);cmd;")};

  ASSERT_EQ(m1, std::string::npos);
  ASSERT_EQ(m2, std::string::npos);
  ASSERT_EQ(m3, std::string::npos);
}
