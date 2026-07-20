/**
 * Copyright 2026 Centreon
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

#include "com/centreon/broker/broker_notification_callbacks.hh"

#include <gtest/gtest.h>

#include "broker/core/cache/broker_cache.hh"
#include "broker/core/config/applier/broker_state.hh"

using namespace com::centreon::broker;

/**
 * @brief Fixture exercising broker_notification_callbacks::{get_config,
 * get_state} against the singleton Broker cache these callbacks read from.
 *
 * The cache is fed with neb objects directly (rather than through merge()) so
 * every field the callbacks consult — including the runtime ones merge() never
 * sets (state, flapping, acknowledged, ...) — is controlled by the test.
 */
class BrokerNotificationCallbacksTest : public ::testing::Test {
 protected:
  cache::broker_cache* _cache = nullptr;
  std::unique_ptr<broker_notification_callbacks> _cb;

 public:
  void SetUp() override {
    config::applier::state::load<config::applier::broker_state>("unittest");
    auto& st = config::applier::state::instance();
    st.initialize_cache();
    _cache = &st.cache();
    _cache->enable_section(cache::broker_cache::CACHE_ALL);
    _cb = std::make_unique<broker_notification_callbacks>();
  }

  void TearDown() override {
    _cb.reset();
    config::applier::state::unload();
  }

  void publish_host(uint64_t host_id, uint64_t poller_id, bool notify) {
    auto h = std::make_shared<neb::pb_host>();
    Host& o = h->mut_obj();
    o.set_host_id(host_id);
    o.set_name(fmt::format("host_{}", host_id));
    o.set_instance_id(poller_id);
    o.set_enabled(true);
    o.set_notify(notify);
    _cache->publish(h);
  }
};

/**
 * @brief get_config folds the poller-wide switch (default enabled when the
 * poller is unknown to the cache) with the resource's own notify flag.
 */
TEST_F(BrokerNotificationCallbacksTest, GetConfigHostFoldsResourceNotify) {
  publish_host(1, 1, /*notify=*/true);
  publish_host(2, 1, /*notify=*/false);

  EXPECT_TRUE(_cb->get_config(1, 0).enabled);
  EXPECT_FALSE(_cb->get_config(2, 0).enabled);
}

/**
 * @brief A resource unknown to the cache yields disabled notifications rather
 * than crashing.
 */
TEST_F(BrokerNotificationCallbacksTest, GetConfigUnknownHostDisabled) {
  EXPECT_FALSE(_cb->get_config(42, 0).enabled);
}

/**
 * @brief get_state maps the cached service snapshot onto resource_state,
 * converting the interval-unit values to seconds through the poller's
 * interval_length (defaulting to 60s when the poller is unknown).
 */
TEST_F(BrokerNotificationCallbacksTest, GetStateServiceMapping) {
  publish_host(1, 1, /*notify=*/true);

  auto s = std::make_shared<neb::pb_service>();
  Service& o = s->mut_obj();
  o.set_host_id(1);
  o.set_service_id(5);
  o.set_host_name("host_1");
  o.set_description("service_5");
  o.set_enabled(true);
  o.set_notify(true);
  o.set_is_volatile(true);
  o.set_state(Service::CRITICAL);
  o.set_state_type(Service::HARD);
  o.set_acknowledged(true);
  o.set_flapping(false);
  o.set_scheduled_downtime_depth(2);
  o.set_notify_on_critical(true);
  o.set_notify_on_warning(false);
  o.set_notification_interval(3);
  _cache->publish(s);

  notifications::resource_state state = _cb->get_state(1, 5);

  EXPECT_TRUE(state.is_volatile);
  EXPECT_TRUE(state.hard_state);
  EXPECT_TRUE(state.acknowledged);
  EXPECT_FALSE(state.flapping);
  EXPECT_EQ(state.scheduled_downtime_depth, 2);
  EXPECT_EQ(state.current_state, Service::CRITICAL);
  EXPECT_EQ(state.current_state_as_string, "CRITICAL");
  EXPECT_TRUE(state.notify_on & notifications::critical);
  EXPECT_FALSE(state.notify_on & notifications::warning);
  EXPECT_TRUE(state.notify_on_current_state);
  /* 3 interval units * 60s (default interval_length) = 180s. */
  EXPECT_EQ(state.notification_interval, std::chrono::seconds{180});
}
