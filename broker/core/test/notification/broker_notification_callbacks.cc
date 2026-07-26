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

#include <thread>

#include "broker/core/bbdo/internal.hh"
#include "broker/core/cache/broker_cache.hh"
#include "broker/core/config/applier/init.hh"
#include "com/centreon/broker/multiplexing/muxer.hh"
#include "common/engine_conf/hostdependency_helper.hh"

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

  /**
   * @brief Publish a host to the cache with its notify flag set.
   *
   * @param host_id The host id.
   * @param poller_id The poller (instance) id the host belongs to.
   * @param notify The resource's own notification-enable flag.
   */
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

  /**
   * @brief Republish a host with the given runtime state.
   *
   * Replaces the cached host object (update_host replaces by id), used to set a
   * dependency master's live state before evaluating a notification dependency.
   *
   * @param host_id The host id.
   * @param poller_id The poller (instance) id the host belongs to.
   * @param state The current host state.
   * @param state_type SOFT or HARD.
   * @param checked Whether the host has already been checked (drives the
   * fail-on-pending path).
   */
  void publish_host_state(uint64_t host_id,
                          uint64_t poller_id,
                          Host::State state,
                          Host::StateType state_type,
                          bool checked) {
    auto h = std::make_shared<neb::pb_host>();
    Host& o = h->mut_obj();
    o.set_host_id(host_id);
    o.set_name(fmt::format("host_{}", host_id));
    o.set_instance_id(poller_id);
    o.set_enabled(true);
    o.set_state(state);
    o.set_state_type(state_type);
    o.set_last_hard_state(state);
    o.set_checked(checked);
    _cache->publish(h);
  }

  /**
   * @brief Republish a service with the given runtime state.
   *
   * Service counterpart of publish_host_state: replaces the cached service
   * object to set a dependency master's live state.
   *
   * @param host_id The service's host id.
   * @param service_id The service id.
   * @param state The current service state.
   * @param state_type SOFT or HARD.
   * @param checked Whether the service has already been checked (drives the
   * fail-on-pending path).
   */
  void publish_service_state(uint64_t host_id,
                             uint64_t service_id,
                             Service::State state,
                             Service::StateType state_type,
                             bool checked) {
    auto s = std::make_shared<neb::pb_service>();
    Service& o = s->mut_obj();
    o.set_host_id(host_id);
    o.set_service_id(service_id);
    o.set_host_name(fmt::format("host_{}", host_id));
    o.set_description(fmt::format("service_{}", service_id));
    o.set_enabled(true);
    o.set_state(state);
    o.set_state_type(state_type);
    o.set_last_hard_state(state);
    o.set_checked(checked);
    _cache->publish(s);
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
  o.set_acknowledgement_type(AckType::NORMAL);
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

/**
 * @brief A notification host dependency blocks the notification when the master
 * host is in a failing state, and get_state reflects it.
 */
TEST_F(BrokerNotificationCallbacksTest, AuthorizedByDependenciesHost) {
  namespace cfg = com::centreon::engine::configuration;

  cfg::State st;
  st.set_poller_id(1);
  for (uint64_t id : {1u, 2u}) {
    auto* h = st.mutable_hosts()->Add();
    h->set_host_id(id);
    h->set_host_name(fmt::format("host_{}", id));
  }
  /* host_1 depends on host_2 (master); the dependency fails on master DOWN. */
  auto* hd = st.mutable_hostdependencies()->Add();
  hd->set_dependency_type(cfg::notification_dependency);
  hd->mutable_dependent_hosts()->add_data("host_1");
  hd->mutable_hosts()->add_data("host_2");
  hd->set_notification_failure_options(cfg::action_hd_down);
  _cache->merge(st);

  /* Master UP (hard, checked): the dependency does not fail. */
  publish_host_state(2, 1, Host::UP, Host::HARD, /*checked=*/true);
  EXPECT_TRUE(_cache->notification_authorized_by_dependencies(1, 0));

  /* Master DOWN (hard): the dependency fails -> notification not authorized,
   * and get_state carries it through. */
  publish_host_state(2, 1, Host::DOWN, Host::HARD, /*checked=*/true);
  EXPECT_FALSE(_cache->notification_authorized_by_dependencies(1, 0));
  EXPECT_FALSE(_cb->get_state(1, 0).authorized_by_dependencies);

  /* A host with no dependency is always authorized. */
  EXPECT_TRUE(_cache->notification_authorized_by_dependencies(2, 0));
}

/**
 * @brief Service dependency evaluation maps the master state onto the correct
 * action_sd_* bit (whose order differs from the neb service state enum).
 */
TEST_F(BrokerNotificationCallbacksTest,
       AuthorizedByDependenciesServiceBitOrder) {
  namespace cfg = com::centreon::engine::configuration;

  cfg::State st;
  st.set_poller_id(1);
  auto* h = st.mutable_hosts()->Add();
  h->set_host_id(1);
  h->set_host_name("host_1");
  for (uint64_t svc_id : {10u, 20u}) {
    auto* s = st.mutable_services()->Add();
    s->set_host_id(1);
    s->set_service_id(svc_id);
    s->set_host_name("host_1");
    s->set_service_description(fmt::format("service_{}", svc_id));
  }
  /* (1,20) depends on (1,10); fails only on master CRITICAL. */
  auto* sd = st.mutable_servicedependencies()->Add();
  sd->set_dependency_type(cfg::notification_dependency);
  sd->mutable_dependent_hosts()->add_data("host_1");
  sd->mutable_dependent_service_description()->add_data("service_20");
  sd->mutable_hosts()->add_data("host_1");
  sd->mutable_service_description()->add_data("service_10");
  sd->set_notification_failure_options(cfg::action_sd_critical);
  _cache->merge(st);

  /* Master CRITICAL matches action_sd_critical -> not authorized. */
  publish_service_state(1, 10, Service::CRITICAL, Service::HARD, true);
  EXPECT_FALSE(_cache->notification_authorized_by_dependencies(1, 20));

  /* Master WARNING does not match (proves the bit is not read positionally,
   * where CRITICAL=2 would collide with action_sd_warning). */
  publish_service_state(1, 10, Service::WARNING, Service::HARD, true);
  EXPECT_TRUE(_cache->notification_authorized_by_dependencies(1, 20));
}

/**
 * @brief Fixture for broker_notification_callbacks::deliver(): it must, on the
 * Broker side, publish a pb_notification_execute event so the poller that
 * supervises the resource runs the notification command (model C, brick 1).
 *
 * Unlike the read-only callbacks above, deliver() publishes through the
 * multiplexing engine, so this fixture brings the engine up and subscribes a
 * muxer filtered on the pb_notification_execute type to capture the emission.
 */
class BrokerNotificationDeliverTest : public ::testing::Test {
 protected:
  cache::broker_cache* _cache = nullptr;
  std::unique_ptr<broker_notification_callbacks> _cb;
  std::shared_ptr<multiplexing::muxer> _mux;

 public:
  void SetUp() override {
    config::applier::init<config::applier::broker_state>("", 0, "test_broker",
                                                         0);
    auto& st = config::applier::state::instance();
    st.initialize_cache();
    _cache = &st.cache();
    _cache->enable_section(cache::broker_cache::CACHE_ALL);
    _cb = std::make_unique<broker_notification_callbacks>();

    multiplexing::muxer_filter f{bbdo::pb_notification_execute::static_type()};
    _mux = multiplexing::muxer::create(
        "test-notif-exec", multiplexing::engine::instance_ptr(), f, f, false);
    multiplexing::engine::instance_ptr()->start();
  }

  void TearDown() override {
    _mux->unsubscribe();
    _mux.reset();
    _cb.reset();
    config::applier::deinit();
  }

  /** @brief Read the next event from the muxer, retrying briefly since the
   * multiplexing engine dispatches asynchronously. */
  std::shared_ptr<io::data> read_one() {
    std::shared_ptr<io::data> d;
    for (int i = 0; i < 100 && !d; ++i) {
      _mux->read(d, 0);
      if (!d)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return d;
  }
};

/**
 * @brief deliver() publishes a pb_notification_execute carrying the
 * notification parameters and the selected contacts, addressed (destination_id)
 * to the poller that supervises the resource.
 */
TEST_F(BrokerNotificationDeliverTest, DispatchesNotificationExecute) {
  auto h = std::make_shared<neb::pb_host>();
  Host& o = h->mut_obj();
  o.set_host_id(1);
  o.set_name("host_1");
  o.set_instance_id(7);
  o.set_enabled(true);
  o.set_notify(true);
  _cache->publish(h);

  notifications::delivery_result res =
      _cb->deliver(1, 5, notifications::cat_normal, notifications::reason_normal,
                   42, 3, "admin", "the service is down",
                   notifications::notification_option_none);

  /* deliver() reports the (injected, brick 1) contact set as notified. */
  EXPECT_EQ(res.notified_contacts.size(), 1u);
  EXPECT_EQ(res.notified_contacts.count("John_Doe"), 1u);

  /* and it published the dispatch event for poller 7. */
  std::shared_ptr<io::data> d = read_one();
  ASSERT_TRUE(d);
  ASSERT_EQ(d->type(), bbdo::pb_notification_execute::static_type());
  auto evt = std::static_pointer_cast<bbdo::pb_notification_execute>(d);
  const NotificationExecute& n = evt->obj();
  EXPECT_EQ(n.host_id(), 1u);
  EXPECT_EQ(n.service_id(), 5u);
  EXPECT_EQ(n.category(), static_cast<uint32_t>(notifications::cat_normal));
  EXPECT_EQ(n.reason_type(), static_cast<uint32_t>(notifications::reason_normal));
  EXPECT_EQ(n.notification_id(), 42u);
  EXPECT_EQ(n.notification_number(), 3u);
  EXPECT_EQ(n.author(), "admin");
  EXPECT_EQ(n.message(), "the service is down");
  ASSERT_EQ(n.contacts_size(), 1);
  EXPECT_EQ(n.contacts(0), "John_Doe");
  EXPECT_EQ(evt->destination_id, 7u);
}

/**
 * @brief A notification for a host unknown to the cache is dropped: no dispatch
 * event is published and no contact is reported as notified.
 */
TEST_F(BrokerNotificationDeliverTest, UnknownHostDropped) {
  notifications::delivery_result res =
      _cb->deliver(999, 0, notifications::cat_normal, notifications::reason_normal,
                   1, 1, "admin", "msg",
                   notifications::notification_option_none);

  EXPECT_TRUE(res.notified_contacts.empty());
  EXPECT_FALSE(read_one());
}
