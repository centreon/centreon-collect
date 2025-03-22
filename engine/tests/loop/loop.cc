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

#include "com/centreon/engine/events/loop.hh"
#include <gtest/gtest.h>
#include "../test_engine.hh"
#include "../timeperiod/utils.hh"
#include "com/centreon/engine/checks/checker.hh"
#include "com/centreon/engine/configuration/applier/contact.hh"
#include "com/centreon/engine/configuration/applier/host.hh"
#include "com/centreon/engine/configuration/applier/service.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/serviceescalation.hh"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;

class LoopTest : public TestEngine {
 public:
  void SetUp() override {
    error_cnt err;
    init_config_state();
    events::loop::instance().clear();

    configuration::applier::contact ct_aply;
    configuration::Contact ctct{new_pb_configuration_contact("admin", true)};
    ct_aply.add_object(ctct);
    ct_aply.expand_objects(pb_indexed_config.state());
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

    service_map const& sm{engine::service::services};
    _svc = sm.begin()->second;
    _svc->set_current_state(engine::service::state_ok);
    _svc->set_state_type(checkable::hard);
    _svc->set_acknowledgement(AckType::NONE);
    _svc->set_notify_on(static_cast<uint32_t>(-1));
  }

  void TearDown() override {
    _host.reset();
    _svc.reset();
    deinit_config_state();
  }

 protected:
  std::shared_ptr<engine::host> _host;
  std::shared_ptr<engine::service> _svc;
};

TEST_F(LoopTest, Simple) {
  // Run an empty loop
  events::loop::instance().run();
  // The loop is stopped immediatly
}

TEST_F(LoopTest, ServiceCheck) {
  timeval tv;
  gettimeofday(&tv, nullptr);
  // A little hack so that time() gives something near of gettimeofday.
  // Do not forget time is modified in the test...
  time_t now = tv.tv_sec;
  uint32_t options = 0;
  set_time(now);
  auto new_event{std::make_unique<timed_event>(timed_event::EVENT_SERVICE_CHECK,
                                               now, false, 0L, nullptr, true,
                                               _svc.get(), nullptr, options)};
  ASSERT_EQ(new_event->name(), "EVENT_SERVICE_CHECK");
  events::loop::instance().reschedule_event(std::move(new_event),
                                            events::loop::low);
  /* The interest of this test is just to verify that the timed_event is
   * consumed by the loop. */
  ASSERT_NO_THROW(events::loop::instance().run());
  // It must exit
}

TEST_F(LoopTest, HostCheck) {
  timeval tv;
  gettimeofday(&tv, nullptr);
  // A little hack so that time() gives something near of gettimeofday.
  // Do not forget time is modified in the test...
  time_t now = tv.tv_sec;
  uint32_t options = 0;
  set_time(now);
  auto new_event{std::make_unique<timed_event>(timed_event::EVENT_HOST_CHECK,
                                               now, false, 0L, nullptr, true,
                                               _host.get(), nullptr, options)};
  ASSERT_EQ(new_event->name(), "EVENT_HOST_CHECK");
  events::loop::instance().reschedule_event(std::move(new_event),
                                            events::loop::low);
  /* The interest of this test is just to verify that the timed_event is
   * consumed by the loop. */
  ASSERT_NO_THROW(events::loop::instance().run());
}

TEST_F(LoopTest, ScheduledDowntime) {
  uint64_t* new_downtime_id = new uint64_t(23);
  timeval tv;
  gettimeofday(&tv, nullptr);
  // A little hack so that time() gives something near of gettimeofday.
  // Do not forget time is modified in the test...
  time_t now = tv.tv_sec;
  set_time(now);
  auto new_event{std::make_unique<timed_event>(
      timed_event::EVENT_SCHEDULED_DOWNTIME, now, false, 0L, nullptr, false,
      new_downtime_id, nullptr, 0)};
  ASSERT_EQ(new_event->name(), "EVENT_SCHEDULED_DOWNTIME");
  events::loop::instance().schedule(std::move(new_event), true);
  new_event = std::make_unique<timed_event>(timed_event::EVENT_EXPIRE_DOWNTIME,
                                            now, false, 0, nullptr, false,
                                            nullptr, nullptr, 0);
  events::loop::instance().schedule(std::move(new_event), true);
  /* The interest of this test is just to verify that the timed_event is
   * consumed by the loop. */
  ASSERT_NO_THROW(events::loop::instance().run());
}
