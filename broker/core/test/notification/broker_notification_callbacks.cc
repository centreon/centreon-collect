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

#include <algorithm>
#include <vector>

#include "broker/core/bbdo/internal.hh"
#include "broker/core/cache/broker_cache.hh"
#include "broker/core/config/applier/broker_state.hh"
#include "broker/core/config/applier/init.hh"
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
 * @brief merge(State) populates the contacts and contactgroups caches; a
 * contact is read back with its notification fields and a contactgroup is
 * expanded to its member contact names at query time.
 */
TEST_F(BrokerNotificationCallbacksTest, ContactsAndContactgroupsFromState) {
  namespace cfg = com::centreon::engine::configuration;

  cfg::State st;
  st.set_poller_id(1);

  auto* c1 = st.mutable_contacts()->Add();
  c1->set_contact_name("John_Doe");
  c1->set_host_notifications_enabled(true);
  c1->set_service_notifications_enabled(false);
  c1->set_host_notification_period("24x7");
  c1->set_service_notification_period("workhours");
  c1->set_host_notification_options(cfg::action_hst_down);
  c1->set_service_notification_options(cfg::action_svc_critical);
  c1->set_timezone(":Europe/Paris");

  auto* c2 = st.mutable_contacts()->Add();
  c2->set_contact_name("Jane_Doe");

  auto* cg = st.mutable_contactgroups()->Add();
  cg->set_contactgroup_name("admins");
  cg->mutable_members()->add_data("John_Doe");
  cg->mutable_members()->add_data("Jane_Doe");

  _cache->merge(st);

  auto john = _cache->contact_config("John_Doe");
  ASSERT_TRUE(john.has_value());
  EXPECT_EQ(john->name, "John_Doe");
  EXPECT_TRUE(john->host_notifications_enabled);
  EXPECT_FALSE(john->service_notifications_enabled);
  EXPECT_EQ(john->host_notification_period, "24x7");
  EXPECT_EQ(john->service_notification_period, "workhours");
  EXPECT_EQ(john->host_notification_options, cfg::action_hst_down);
  EXPECT_EQ(john->service_notification_options, cfg::action_svc_critical);
  EXPECT_EQ(john->timezone, ":Europe/Paris");

  /* An unknown contact yields no value. */
  EXPECT_FALSE(_cache->contact_config("nobody").has_value());

  /* The contactgroup expands to its two members (order-independent). */
  auto members = _cache->contactgroup_members("admins");
  std::sort(members.begin(), members.end());
  EXPECT_EQ(members, (std::vector<std::string>{"Jane_Doe", "John_Doe"}));
  /* An unknown contactgroup expands to nothing. */
  EXPECT_TRUE(_cache->contactgroup_members("unknown").empty());
}

/**
 * @brief apply(DiffState) adds, modifies and removes contacts/contactgroups
 * incrementally, mirroring the DiffContact/DiffContactgroup added/modified/
 * removed lists.
 */
TEST_F(BrokerNotificationCallbacksTest, ContactsDiffAddModifyRemove) {
  namespace cfg = com::centreon::engine::configuration;

  cfg::State st;
  st.set_poller_id(1);
  auto* c = st.mutable_contacts()->Add();
  c->set_contact_name("John_Doe");
  c->set_host_notifications_enabled(true);
  auto* cg = st.mutable_contactgroups()->Add();
  cg->set_contactgroup_name("admins");
  cg->mutable_members()->add_data("John_Doe");
  _cache->merge(st);

  cfg::DiffState diff;
  diff.set_poller_id(1);
  /* Add a contact, modify John_Doe, remove nothing yet. */
  auto* added = diff.mutable_contacts()->mutable_added()->Add();
  added->set_contact_name("Jane_Doe");
  auto* modified = diff.mutable_contacts()->mutable_modified()->Add();
  modified->set_contact_name("John_Doe");
  modified->set_host_notifications_enabled(false);
  /* Add a member to the contactgroup (modified replaces the whole group). */
  auto* mcg = diff.mutable_contactgroups()->mutable_modified()->Add();
  mcg->set_contactgroup_name("admins");
  mcg->mutable_members()->add_data("John_Doe");
  mcg->mutable_members()->add_data("Jane_Doe");
  _cache->apply(diff);

  EXPECT_TRUE(_cache->contact_config("Jane_Doe").has_value());
  auto john = _cache->contact_config("John_Doe");
  ASSERT_TRUE(john.has_value());
  EXPECT_FALSE(john->host_notifications_enabled);
  auto admins = _cache->contactgroup_members("admins");
  std::sort(admins.begin(), admins.end());
  EXPECT_EQ(admins, (std::vector<std::string>{"Jane_Doe", "John_Doe"}));

  /* Remove Jane_Doe and the whole contactgroup. */
  cfg::DiffState diff2;
  diff2.set_poller_id(1);
  diff2.mutable_contacts()->add_removed("Jane_Doe");
  diff2.mutable_contactgroups()->add_removed("admins");
  _cache->apply(diff2);

  EXPECT_FALSE(_cache->contact_config("Jane_Doe").has_value());
  EXPECT_TRUE(_cache->contact_config("John_Doe").has_value());
  EXPECT_TRUE(_cache->contactgroup_members("admins").empty());
}

/**
 * @brief Contacts and contactgroups are reference-counted per poller: an entry
 * shared by two pollers survives the removal of one of them and is dropped only
 * when the last referencing poller goes away. A full re-merge rebuilds a
 * poller's contribution, dropping the contacts it no longer defines.
 */
TEST_F(BrokerNotificationCallbacksTest, ContactsReferenceCountedPerPoller) {
  namespace cfg = com::centreon::engine::configuration;

  auto merge_shared = [this](uint32_t poller_id) {
    cfg::State st;
    st.set_poller_id(poller_id);
    st.mutable_contacts()->Add()->set_contact_name("shared");
    auto* cg = st.mutable_contactgroups()->Add();
    cg->set_contactgroup_name("grp");
    cg->mutable_members()->add_data("shared");
    _cache->merge(st);
  };
  merge_shared(1);
  merge_shared(2);

  /* Poller 1 leaves: the shared contact/group is still referenced by poller 2. */
  _cache->remove_instance(1);
  EXPECT_TRUE(_cache->contact_config("shared").has_value());
  EXPECT_FALSE(_cache->contactgroup_members("grp").empty());

  /* Poller 2 leaves too: now nothing references them. */
  _cache->remove_instance(2);
  EXPECT_FALSE(_cache->contact_config("shared").has_value());
  EXPECT_TRUE(_cache->contactgroup_members("grp").empty());

  /* A full re-merge without a previously present contact drops it. */
  cfg::State st;
  st.set_poller_id(1);
  st.mutable_contacts()->Add()->set_contact_name("first");
  _cache->merge(st);
  EXPECT_TRUE(_cache->contact_config("first").has_value());

  cfg::State st2;
  st2.set_poller_id(1);
  st2.mutable_contacts()->Add()->set_contact_name("second");
  _cache->merge(st2);
  EXPECT_FALSE(_cache->contact_config("first").has_value());
  EXPECT_TRUE(_cache->contact_config("second").has_value());
}

/**
 * @brief merge(State) links each resource to its direct contacts and
 * contactgroups; notification_contact_names resolves that link at query time to
 * the deduplicated union of direct contacts and contactgroup members.
 */
TEST_F(BrokerNotificationCallbacksTest, ResourceContactsResolvedFromState) {
  namespace cfg = com::centreon::engine::configuration;

  cfg::State st;
  st.set_poller_id(1);

  /* The contacts a resource references directly must be defined (a resource
   * stores non-owning pointers into the contacts cache). */
  for (const char* name : {"John_Doe", "Jane_Doe", "Bob"})
    st.mutable_contacts()->Add()->set_contact_name(name);

  /* host_1: one direct contact + one contactgroup. */
  auto* h = st.mutable_hosts()->Add();
  h->set_host_id(1);
  h->set_host_name("host_1");
  h->mutable_contacts()->add_data("John_Doe");
  h->mutable_contactgroups()->add_data("admins");

  /* service (1,5): only a direct contact, no group. */
  auto* s = st.mutable_services()->Add();
  s->set_host_id(1);
  s->set_service_id(5);
  s->set_host_name("host_1");
  s->set_service_description("service_1");
  s->mutable_contacts()->add_data("Bob");

  auto* cg = st.mutable_contactgroups()->Add();
  cg->set_contactgroup_name("admins");
  cg->mutable_members()->add_data("Jane_Doe");
  cg->mutable_members()->add_data("John_Doe");

  _cache->merge(st);

  /* Host: direct John_Doe + group members {Jane_Doe, John_Doe}, deduplicated. */
  EXPECT_EQ(_cache->notification_contact_names(1, 0),
            (absl::flat_hash_set<std::string>{"Jane_Doe", "John_Doe"}));

  /* Service: only its direct contact. */
  EXPECT_EQ(_cache->notification_contact_names(1, 5),
            (absl::flat_hash_set<std::string>{"Bob"}));

  /* Unknown resource: empty. */
  EXPECT_TRUE(_cache->notification_contact_names(42, 0).empty());

  /* Purge on poller removal. */
  _cache->remove_instance(1);
  EXPECT_TRUE(_cache->notification_contact_names(1, 0).empty());
  EXPECT_TRUE(_cache->notification_contact_names(1, 5).empty());
}

/**
 * @brief apply(DiffState) feeds the resource->contacts link incrementally; a
 * modification that clears a resource's contacts drops the link, and a removed
 * resource drops it too.
 */
TEST_F(BrokerNotificationCallbacksTest, ResourceContactsDiffAddModifyRemove) {
  namespace cfg = com::centreon::engine::configuration;

  cfg::State st;
  st.set_poller_id(1);
  st.mutable_contacts()->Add()->set_contact_name("John_Doe");
  auto* h = st.mutable_hosts()->Add();
  h->set_host_id(1);
  h->set_host_name("host_1");
  h->mutable_contacts()->add_data("John_Doe");
  _cache->merge(st);
  EXPECT_EQ(_cache->notification_contact_names(1, 0),
            (absl::flat_hash_set<std::string>{"John_Doe"}));

  /* Add a service with a contact. The contact is added in the same diff; the
   * contacts block is applied before the services block, so the resource can
   * resolve its pointer. */
  cfg::DiffState diff;
  diff.set_poller_id(1);
  diff.mutable_contacts()->mutable_added()->Add()->set_contact_name("Bob");
  auto* s = diff.mutable_services()->mutable_added()->Add();
  s->set_host_id(1);
  s->set_service_id(5);
  s->set_host_name("host_1");
  s->set_service_description("service_1");
  s->mutable_contacts()->add_data("Bob");
  _cache->apply(diff);
  EXPECT_EQ(_cache->notification_contact_names(1, 5),
            (absl::flat_hash_set<std::string>{"Bob"}));

  /* Modify host_1 to have no contact anymore: the link is dropped. */
  cfg::DiffState diff2;
  diff2.set_poller_id(1);
  auto* mh = diff2.mutable_hosts()->mutable_modified()->Add();
  mh->set_host_id(1);
  mh->set_host_name("host_1");
  _cache->apply(diff2);
  EXPECT_TRUE(_cache->notification_contact_names(1, 0).empty());

  /* Remove the service: its link is dropped. */
  cfg::DiffState diff3;
  diff3.set_poller_id(1);
  auto* rk = diff3.mutable_services()->mutable_removed()->Add();
  rk->set_host_id(1);
  rk->set_service_id(5);
  _cache->apply(diff3);
  EXPECT_TRUE(_cache->notification_contact_names(1, 5).empty());
}

/**
 * @brief Fixture for broker_notification_callbacks::deliver(): it must, on the
 * Broker side, publish a pb_notification_execute event so the poller that
 * supervises the resource runs the notification command (model C, brick 1).
 *
 * deliver() does not broadcast through the multiplexing engine: it queues the
 * pb_notification_execute on broker_state for targeted delivery to the poller
 * supervising the resource (drained in that poller's ENGINE-connected stream
 * read()). The fixture therefore captures the emission by draining that
 * per-poller pending queue.
 */
class BrokerNotificationDeliverTest : public ::testing::Test {
 protected:
  cache::broker_cache* _cache = nullptr;
  config::applier::broker_state* _state = nullptr;
  std::unique_ptr<broker_notification_callbacks> _cb;

 public:
  void SetUp() override {
    config::applier::init<config::applier::broker_state>("", 0, "test_broker",
                                                         0);
    auto& st = config::applier::state::instance();
    st.initialize_cache();
    _cache = &st.cache();
    _cache->enable_section(cache::broker_cache::CACHE_ALL);
    _state = &static_cast<config::applier::broker_state&>(st);
    _cb = std::make_unique<broker_notification_callbacks>();
  }

  void TearDown() override {
    _cb.reset();
    config::applier::deinit();
  }

  /** @brief Pop the single notification execute queued for @p poller_id, or
   * nullptr when the poller's queue is empty. */
  std::shared_ptr<io::data> pop_one(uint64_t poller_id) {
    auto pending = _state->pop_pending_notification_executes(poller_id);
    if (pending.empty())
      return nullptr;
    return pending.front();
  }
};

/**
 * @brief Feed a poller-7 configuration with service (1,5) owned by contact
 * John_Doe, whose service notifications are enabled or not per @p enabled.
 */
static void merge_service_with_contact(cache::broker_cache* cache,
                                       bool enabled) {
  namespace cfg = com::centreon::engine::configuration;
  cfg::State st;
  st.set_poller_id(7);
  auto* c = st.mutable_contacts()->Add();
  c->set_contact_name("John_Doe");
  c->set_service_notifications_enabled(enabled);
  auto* h = st.mutable_hosts()->Add();
  h->set_host_id(1);
  h->set_host_name("host_1");
  auto* s = st.mutable_services()->Add();
  s->set_host_id(1);
  s->set_service_id(5);
  s->set_host_name("host_1");
  s->set_service_description("service_1");
  s->mutable_contacts()->add_data("John_Doe");
  cache->merge(st);
}

/**
 * @brief deliver() selects the resource's contacts (filtered by viability) and
 * queues a pb_notification_execute carrying them, addressed (destination_id) to
 * the poller that supervises the resource.
 */
TEST_F(BrokerNotificationDeliverTest, DispatchesNotificationExecute) {
  merge_service_with_contact(_cache, /*enabled=*/true);

  notifications::delivery_result res = _cb->deliver(
      1, 5, notifications::cat_acknowledgement,
      notifications::reason_acknowledgement, 42, 3, "admin",
      "the service is acknowledged", notifications::notification_option_none);

  /* deliver() reports the selected contact as notified. */
  EXPECT_EQ(res.notified_contacts.size(), 1u);
  EXPECT_EQ(res.notified_contacts.count("John_Doe"), 1u);

  /* and it queued the dispatch event for poller 7 (host_1's supervisor). */
  std::shared_ptr<io::data> d = pop_one(7);
  ASSERT_TRUE(d);
  ASSERT_EQ(d->type(), bbdo::pb_notification_execute::static_type());
  auto evt = std::static_pointer_cast<bbdo::pb_notification_execute>(d);
  const NotificationExecute& n = evt->obj();
  EXPECT_EQ(n.host_id(), 1u);
  EXPECT_EQ(n.service_id(), 5u);
  EXPECT_EQ(n.category(),
            static_cast<uint32_t>(notifications::cat_acknowledgement));
  EXPECT_EQ(n.reason_type(),
            static_cast<uint32_t>(notifications::reason_acknowledgement));
  EXPECT_EQ(n.notification_id(), 42u);
  EXPECT_EQ(n.notification_number(), 3u);
  EXPECT_EQ(n.author(), "admin");
  EXPECT_EQ(n.message(), "the service is acknowledged");
  ASSERT_EQ(n.contacts_size(), 1);
  EXPECT_EQ(n.contacts(0), "John_Doe");
  EXPECT_EQ(evt->destination_id, 7u);
}

/**
 * @brief A contact whose notifications are disabled is filtered out: nobody is
 * notified and no dispatch event is queued.
 */
TEST_F(BrokerNotificationDeliverTest, ContactFilteredOutNotDispatched) {
  merge_service_with_contact(_cache, /*enabled=*/false);

  notifications::delivery_result res = _cb->deliver(
      1, 5, notifications::cat_acknowledgement,
      notifications::reason_acknowledgement, 1, 1, "admin", "msg",
      notifications::notification_option_none);

  EXPECT_TRUE(res.notified_contacts.empty());
  EXPECT_FALSE(pop_one(7));
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
  /* Nothing was queued for any poller (the resource has no supervisor). */
  EXPECT_FALSE(pop_one(7));
}
