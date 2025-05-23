/**
 * Copyright 2017 - 2019 Centreon (https://www.centreon.com/)
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

#include <cstring>

#include "../helper.hh"
#include "../timeperiod/utils.hh"
#include "com/centreon/engine/configuration/applier/host.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/host.hh"
#include "com/centreon/engine/hostescalation.hh"
#include "common/engine_conf/host_helper.hh"
#include "test_engine.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;
using namespace com::centreon::engine::configuration::applier;

class HostRecovery : public TestEngine {
 public:
  void SetUp() override {
    init_config_state();
    // Do not unload this in the tear down function, it is done by the
    // other unload function... :-(

    configuration::applier::host hst_aply;
    configuration::Host hst;
    configuration::host_helper hst_hlp(&hst);
    hst.set_host_name("test_host");
    hst.set_address("127.0.0.1");
    hst.set_host_id(12);
    hst_aply.add_object(hst);
    host_map const& hm{engine::host::hosts};
    _host = hm.begin()->second;
    /* Let's set state down. */
    _host->set_current_state(engine::host::state_down);
    _host->set_state_type(checkable::hard);
    _host->set_acknowledgement(AckType::NONE);
    _host->set_notify_on(static_cast<uint32_t>(-1));
    _current_time = 43200;
    set_time(_current_time);

    _tperiod = new_timeperiod_with_timeranges("tperiod", "alias");
    for (size_t i = 0; i < _tperiod->days.size(); ++i)
      _tperiod->days[i].emplace_back(0, 86400);

    /* 12345 is here to simulate a key. It won't allow any look up */
    std::unique_ptr<engine::hostescalation> host_escalation{
        new engine::hostescalation("host_name", 0, 1, 1.0, "tperiod", 7,
                                   12345)};

    _host->get_next_notification_id();
    _host->set_notification_period_ptr(_tperiod.get());
    /* Sending a notification */
    _host->notify(notifier::reason_normal, "", "",
                  notifier::notification_option_none);
  }

  void TearDown() override {
    _host.reset();
    deinit_config_state();
  }

 protected:
  std::unique_ptr<engine::timeperiod> _tperiod;
  std::shared_ptr<engine::host> _host;
  std::time_t _current_time;
};

// Given a host in hard state down,
// When a call to notify is done five minutes later, it is not sent
// because, the state is always down.
TEST_F(HostRecovery, SimpleRecoveryHostNotificationWithDownState) {
  /* We are using a local time() function defined in tests/timeperiod/utils.cc.
   * If we call time(), it is not the glibc time() function that will be called.
   */
  set_time(_current_time + 300);

  uint64_t id{_host->get_next_notification_id()};
  ASSERT_EQ(_host->notify(notifier::reason_recovery, "", "",
                          notifier::notification_option_none),
            OK);
  ASSERT_EQ(id, _host->get_next_notification_id());
}

// Given a host in hard state down
// When the host is reset to hard up state,
// and a call to notify is done five minutes later, it is sent
// because, the state is always OK.
TEST_F(HostRecovery, SimpleRecoveryHostNotificationWithHardUpState) {
  /* We are using a local time() function defined in tests/timeperiod/utils.cc.
   * If we call time(), it is not the glibc time() function that will be called.
   */
  set_time(_current_time + 300);

  _host->set_current_state(engine::host::state_up);
  _host->set_state_type(engine::host::hard);
  _host->set_last_hard_state_change(_current_time);
  uint64_t id{_host->get_next_notification_id()};
  ASSERT_EQ(_host->notify(notifier::reason_recovery, "", "",
                          notifier::notification_option_none),
            OK);
  ASSERT_EQ(id + 1, _host->get_next_notification_id());
}

// Given a host in hard state down
// When the host is reset to soft up state,
// and a call to notify is done five minutes later, no recovery notification
// is sent.
TEST_F(HostRecovery, SimpleRecoveryHostNotificationWithSoftUpState) {
  /* We are using a local time() function defined in tests/timeperiod/utils.cc.
   * If we call time(), it is not the glibc time() function that will be called.
   */
  set_time(_current_time + 300);

  _host->set_current_state(engine::host::state_up);
  _host->set_state_type(engine::host::soft);
  uint64_t id{_host->get_next_notification_id()};
  ASSERT_EQ(_host->notify(notifier::reason_recovery, "", "",
                          notifier::notification_option_none),
            OK);
  ASSERT_EQ(id, _host->get_next_notification_id());
}

// Given a host in hard state down
// When the host is reset to hard up state, and a recovery notification delay >
// 0
// is given to it, and a call to notify is done before the
// recovery_notification_delay
// end,
// Then no notification is sent.
TEST_F(HostRecovery,
       SimpleRecoveryHostNotificationWithSoftUpStateRecoveryDelay) {
  /* We are using a local time() function defined in tests/timeperiod/utils.cc.
   * If we call time(), it is not the glibc time() function that will be called.
   */

  _host->set_current_state(engine::host::state_up);
  _host->set_state_type(engine::host::hard);
  _host->set_last_hard_state_change(_current_time);
  _host->set_recovery_notification_delay(600);
  // Time too short. No notification will be sent.
  set_time(_current_time + 300);
  uint64_t id{_host->get_next_notification_id()};
  ASSERT_EQ(_host->notify(notifier::reason_recovery, "", "",
                          notifier::notification_option_none),
            OK);
  ASSERT_EQ(id, _host->get_next_notification_id());
}

TEST_F(HostRecovery, SimpleRecoveryHostNotificationAfterDelay) {
  /* We are using a local time() function defined in tests/timeperiod/utils.cc.
   * If we call time(), it is not the glibc time() function that will be called.
   */
  _host->set_current_state(engine::host::state_down);
  _host->set_state_type(engine::host::hard);
  _host->set_last_hard_state_change(_current_time);
  _host->set_recovery_notification_delay(5);
  // Time too short. No notification will be sent.
  _current_time += 100;
  set_time(_current_time);
  uint64_t id{_host->get_next_notification_id()};
  _host->set_current_state(engine::host::state_up);
  _host->set_last_hard_state_change(_current_time);

  ASSERT_EQ(_host->notify(notifier::reason_recovery, "", "",
                          notifier::notification_option_none),
            OK);
  ASSERT_EQ(id, _host->get_next_notification_id());
  _current_time += 350;
  set_time(_current_time);
  ASSERT_EQ(_host->notify(notifier::reason_recovery, "", "",
                          notifier::notification_option_none),
            OK);

  ASSERT_EQ(id + 1, _host->get_next_notification_id());
}
