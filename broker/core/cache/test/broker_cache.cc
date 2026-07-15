/**
 * Copyright 2025-2026 Centreon (https://www.centreon.com/)
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
#include "broker/core/cache/broker_cache.hh"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "broker/core/config/applier/broker_state.hh"
#include "common/engine_conf/hostdependency_helper.hh"
#include "gmock/gmock.h"

using namespace com::centreon::broker;
using namespace com::centreon::broker::cache;

class BrokerCacheTest : public ::testing::Test {
 protected:
  std::unique_ptr<broker_cache> _cache;
  std::shared_ptr<spdlog::logger> _logger;

 public:
  void SetUp() override {
    _logger = spdlog::default_logger();

    config::applier::state::load<config::applier::broker_state>("unittest");
    _cache = std::make_unique<broker_cache>(_logger);
    _cache->enable_section(
        com::centreon::broker::cache::broker_cache::CACHE_ALL);
  }
  void TearDown() override {}
  void publish_hosts(uint32_t from, uint32_t to, uint64_t poller_id = 1);
  void publish_services(uint64_t from, uint64_t to, uint64_t host_id = 1);
};

/**
 * @brief Publish a range of hosts to the cache, with the given poller ID. The
 * host IDs will be from `from` to `to`, and the host names will be "host_1",
 * "host_2", etc.
 *
 * @param from The starting host ID (inclusive)
 * @param to The ending host ID (inclusive)
 * @param poller_id The poller ID to set for the hosts
 */
void BrokerCacheTest::publish_hosts(uint32_t from,
                                    uint32_t to,
                                    uint64_t poller_id) {
  for (uint32_t id = from; id <= to; ++id) {
    auto h = std::make_shared<neb::pb_host>();
    auto& obj = h->mut_obj();
    obj.set_host_id(id);
    obj.set_name(fmt::format("host_{}", id));
    obj.set_enabled(true);
    obj.set_instance_id(poller_id);
    _cache->publish(h);
  }
}

/**
 * @brief Publish a range of services to the cache, on the given host ID. The
 * service IDs will be from `from` to `to`, and the service descriptions will be
 * "service_1", "service_2", etc.
 *
 * @param from The starting service ID (inclusive)
 * @param to The ending service ID (inclusive)
 * @param host_id The host ID to set for the services.
 */
void BrokerCacheTest::publish_services(uint64_t from,
                                       uint64_t to,
                                       uint64_t host_id) {
  for (uint32_t id = from; id <= to; ++id) {
    auto s = std::make_shared<neb::pb_service>();
    auto& obj = s->mut_obj();
    obj.set_host_id(host_id);
    obj.set_service_id(id);
    obj.set_host_name(fmt::format("host_{}", host_id));
    obj.set_description(fmt::format("service_{}", id));
    obj.set_enabled(true);
    _cache->publish(s);
  }
}

TEST_F(BrokerCacheTest, ApplyHostsWithTwoPollers) {
  auto diff = std::make_shared<neb::pb_global_diff_state>();
  auto& d_obj = diff->mut_obj();
  for (int i = 0; i < 4; i++) {
    auto* h = d_obj.mutable_hosts()->add_added();
    h->set_host_name(fmt::format("host_{}", i + 1));
    h->set_host_id(i + 1);
    h->set_address(fmt::format("127.0.0.{}", i + 1));
    h->set_poller_id(i % 2 + 1);
  }
  _cache->publish(diff);
  ASSERT_THAT(_cache->host_ids(), ::testing::ElementsAre(1u, 2u, 3u, 4u));

  // Let's remove poller 2. This should remove host 2 and 4 from the cache, as
  // they are linked to poller 2.
  _cache->remove_instance(2);
  ASSERT_THAT(_cache->host_ids(), ::testing::ElementsAre(1u, 3u));
}

TEST_F(BrokerCacheTest, ApplySeverities) {
  publish_hosts(1, 2, 1);
  publish_hosts(3, 4, 2);
  auto hh = _cache->host(1);
  ASSERT_EQ(hh->obj().host_id(), 1);
  ASSERT_EQ(hh->obj().name(), "host_1");
  ASSERT_EQ(hh->obj().enabled(), true);
  ASSERT_EQ(hh->obj().instance_id(), 1);

  auto diff = std::make_shared<neb::pb_global_diff_state>();
  auto& d_obj = diff->mut_obj();
  auto* s = d_obj.mutable_severities()->add_added();
  s->set_severity_name("severity_1");
  s->set_level(3);
  auto* key = s->mutable_key();
  key->set_id(18);
  key->set_type(Severity_Type_HOST);

  auto* mh = d_obj.mutable_hosts()->add_modified();
  mh->set_host_id(1);
  mh->set_host_name("host_1");
  mh->set_poller_id(1);
  mh->set_severity_id(18);

  mh = d_obj.mutable_hosts()->add_modified();
  mh->set_host_id(3);
  mh->set_host_name("host_3");
  mh->set_poller_id(1);
  mh->set_severity_id(18);

  _cache->publish(diff);
  /* The two hosts should now have severity_id 18 in the cache, with a level
   * of 3. */
  ASSERT_EQ(_cache->severity(1, 0), 3);

  diff = std::make_shared<neb::pb_global_diff_state>();
  {
    auto& d_obj = diff->mut_obj();
    d_obj.mutable_hosts()->add_removed(1);
    d_obj.mutable_hosts()->add_removed(2);
    auto* r_s = d_obj.mutable_severities()->add_removed();
    r_s->set_id(18);
    r_s->set_type(0);
    _cache->publish(diff);
    ASSERT_THAT(_cache->host_ids(), ::testing::ElementsAre(3u, 4u));
    ASSERT_EQ(_cache->severity(3, 0), 3);
  }

  diff = std::make_shared<neb::pb_global_diff_state>();
  {
    auto& d_obj = diff->mut_obj();
    d_obj.mutable_hosts()->add_removed(3);
    d_obj.mutable_hosts()->add_removed(4);
    auto* r_s = d_obj.mutable_severities()->add_removed();
    r_s->set_id(18);
    r_s->set_type(0);
    _cache->publish(diff);
    ASSERT_THAT(_cache->host_ids(), ::testing::IsEmpty());
    ASSERT_EQ(_cache->severity(3, 0), 0);
  }
}

TEST_F(BrokerCacheTest, ApplyHostgroups) {
  publish_hosts(1, 2, 1);
  publish_hosts(3, 4, 2);
  auto hh = _cache->host(1);
  ASSERT_EQ(hh->obj().host_id(), 1);
  ASSERT_EQ(hh->obj().name(), "host_1");
  ASSERT_EQ(hh->obj().enabled(), true);
  ASSERT_EQ(hh->obj().instance_id(), 1);

  auto diff = std::make_shared<neb::pb_global_diff_state>();
  auto& d_obj = diff->mut_obj();
  auto* hg = d_obj.mutable_hostgroups()->add_added();
  hg->set_hostgroup_name("hostgroup_1");
  hg->set_hostgroup_id(19);
  hg->set_poller_id(1);
  com::centreon::engine::configuration::fill_string_group(hg->mutable_members(),
                                                          "host_1");

  hg = d_obj.mutable_hostgroups()->add_added();
  hg->set_hostgroup_name("hostgroup_1");
  hg->set_hostgroup_id(19);
  hg->set_poller_id(2);
  com::centreon::engine::configuration::fill_string_group(hg->mutable_members(),
                                                          "host_4");

  _cache->publish(diff);
  /* Four hosts are defined in the cache, with host_id 1 to 4. One hostgroup
   * is added, but on both pollers 1 and 2.
   * The first definition has host_1 as member on poller_id 1, while the
   * second one has host_4 as member on poller_id 2. The hostgroup should be
   * added to the cache, and both host_1 and host_4 should be linked to it. */
  auto result1 = _cache->hostgroups(1);
  auto result2 = _cache->hostgroups(4);
  ASSERT_EQ(result1.size(), 1);
  ASSERT_EQ(result2.size(), 1);
  ASSERT_THAT(_cache->hostgroup_members(19), ::testing::ElementsAre(1u, 4u));

  /* The hostgroup is removed from poller 2. */
  diff = std::make_shared<neb::pb_global_diff_state>();
  auto& dd_obj = diff->mut_obj();
  dd_obj.mutable_hosts()->add_removed(4);
  auto* hgp = dd_obj.mutable_hostgroups()->add_removed();
  hgp->set_poller_id(2);
  hgp->set_group_name("hostgroup_1");
  _cache->publish(diff);

  result1 = _cache->hostgroups(1);
  ASSERT_EQ(result1.size(), 1);
  ASSERT_THAT(_cache->hostgroup_members(19), ::testing::ElementsAre(1u));

  /* The hostgroup is removed from poller 1. */
  diff = std::make_shared<neb::pb_global_diff_state>();
  auto& ddd_obj = diff->mut_obj();
  ddd_obj.mutable_hosts()->add_removed(1);
  hgp = ddd_obj.mutable_hostgroups()->add_removed();
  hgp->set_poller_id(1);
  hgp->set_group_name("hostgroup_1");
  _cache->publish(diff);

  ASSERT_THAT(_cache->hostgroup_members(19), ::testing::IsEmpty());
}

TEST_F(BrokerCacheTest, ApplyHostgroupMembersModification) {
  /* Three hosts, one hostgroup with both hosts as members are set up in the
   * cache. */
  publish_hosts(1, 3, 1);

  auto hg = std::make_shared<neb::pb_host_group>();
  auto& hg_obj = hg->mut_obj();
  hg_obj.set_hostgroup_id(1);
  hg_obj.set_name("hostgroup_1");
  hg_obj.set_enabled(true);
  hg_obj.set_poller_id(1);
  _cache->publish(hg);

  auto hgm = std::make_shared<neb::pb_host_group_member>();
  auto& hgm_obj = hgm->mut_obj();
  hgm_obj.set_hostgroup_id(1);
  hgm_obj.set_host_id(1);
  hgm_obj.set_name("hostgroup_1");
  hgm_obj.set_enabled(true);
  hgm_obj.set_poller_id(1);
  _cache->publish(hgm);

  auto hgm2 = std::make_shared<neb::pb_host_group_member>();
  auto& hgm2_obj = hgm2->mut_obj();
  hgm2_obj.set_hostgroup_id(1);
  hgm2_obj.set_host_id(2);
  hgm2_obj.set_name("hostgroup_1");
  hgm2_obj.set_enabled(true);
  hgm2_obj.set_poller_id(1);
  _cache->publish(hgm2);

  /* The hostgroup should have two members in the cache, with host_id 1 and 2.
   */
  ASSERT_EQ(_cache->hostgroups(1).size(), 1);
  ASSERT_EQ(_cache->hostgroups(2).size(), 1);
  ASSERT_EQ(_cache->hostgroups(3).size(), 0);

  /* A global diff state is created with a modification of the hostgroup, which
   * is then applied to the cache. The modification consists in removing one of
   * the hosts from the hostgroup. The modification is applied with the apply
   * method, which should lead to the removal of the hostgroup member from the
   * cache. */
  auto diff = std::make_shared<neb::pb_global_diff_state>();
  auto& d_obj = diff->mut_obj();
  auto* m_hg = d_obj.mutable_hostgroups()->add_modified();

  /* host_2 removed from hostgroup_1. */
  m_hg->set_hostgroup_id(1);
  m_hg->set_hostgroup_name("hostgroup_1");
  m_hg->set_poller_id(1);
  com::centreon::engine::configuration::fill_string_group(
      m_hg->mutable_members(), "host_1");

  auto* n_hg = d_obj.mutable_hostgroups()->add_added();
  n_hg->set_hostgroup_id(3);
  n_hg->set_hostgroup_name("hostgroup_3");
  n_hg->set_poller_id(2);
  com::centreon::engine::configuration::fill_string_group(
      n_hg->mutable_members(), "host_3");

  _cache->publish(diff);

  /* The hostgroup member with host_id 2 should have been removed from the
   * cache, while the one with host_id 1 should still be there. */
  ASSERT_EQ(_cache->hostgroups(1).size(), 1);
  ASSERT_EQ(_cache->hostgroups(2).size(), 0);
  ASSERT_EQ(_cache->hostgroups(3).size(), 1);

  diff = std::make_shared<neb::pb_global_diff_state>();
  auto& dd_obj = diff->mut_obj();
  auto* mhg = dd_obj.mutable_hostgroups()->add_modified();
  mhg->set_hostgroup_id(3);
  mhg->set_hostgroup_name("new_name_3");
  mhg->set_poller_id(2);
  com::centreon::engine::configuration::fill_string_group(
      mhg->mutable_members(), "host_3");
  _cache->publish(diff);

  ASSERT_EQ(_cache->hostgroups(3).size(), 1);
  ASSERT_EQ(_cache->hostgroup(3)->obj().name(), "new_name_3");
}

TEST_F(BrokerCacheTest, ApplyServicegroups) {
  publish_hosts(1, 2, 1);
  publish_hosts(3, 4, 2);
  publish_services(1, 5, 1);
  publish_services(6, 10, 3);
  auto ss = _cache->service(1, 1);
  ASSERT_EQ(ss->obj().host_id(), 1);
  ASSERT_EQ(ss->obj().service_id(), 1);
  ASSERT_EQ(ss->obj().description(), "service_1");
  ASSERT_EQ(ss->obj().enabled(), true);

  ss = _cache->service(3, 6);
  ASSERT_EQ(ss->obj().host_id(), 3);
  ASSERT_EQ(ss->obj().service_id(), 6);
  ASSERT_EQ(ss->obj().description(), "service_6");
  ASSERT_EQ(ss->obj().enabled(), true);

  auto diff = std::make_shared<neb::pb_global_diff_state>();
  auto& d_obj = diff->mut_obj();
  auto* sg = d_obj.mutable_servicegroups()->add_added();
  sg->set_servicegroup_name("servicegroup_1");
  sg->set_servicegroup_id(19);
  sg->set_poller_id(1);
  com::centreon::engine::configuration::fill_pair_string_group(
      sg->mutable_members(), "host_1, service_1,host_1,service_2");

  sg = d_obj.mutable_servicegroups()->add_added();
  sg->set_servicegroup_name("servicegroup_1");
  sg->set_servicegroup_id(19);
  sg->set_poller_id(2);
  com::centreon::engine::configuration::fill_pair_string_group(
      sg->mutable_members(), "host_3, service_6");

  _cache->publish(diff);
  auto result1 = _cache->servicegroups(1, 1);
  auto result2 = _cache->servicegroups(3, 6);
  ASSERT_EQ(result1.size(), 1);
  ASSERT_EQ(result2.size(), 1);
  ASSERT_THAT(_cache->servicegroup_members(19),
              ::testing::ElementsAre(std::make_pair(1, 1), std::pair(1, 2),
                                     std::pair(3, 6)));

  /* The servicegroup is removed from poller 2. */
  diff = std::make_shared<neb::pb_global_diff_state>();
  auto& dd_obj = diff->mut_obj();
  auto* svc = dd_obj.mutable_services()->add_removed();
  svc->set_host_id(3);
  svc->set_service_id(6);
  auto* sgp = dd_obj.mutable_servicegroups()->add_removed();
  sgp->set_poller_id(2);
  sgp->set_group_name("servicegroup_1");
  _cache->publish(diff);

  result1 = _cache->servicegroups(1, 1);
  ASSERT_EQ(result1.size(), 1);
  ASSERT_THAT(_cache->servicegroup_members(19),
              ::testing::ElementsAre(std::pair(1, 1), std::pair(1, 2)));

  /* The servicegroup is removed from poller 1. */
  diff = std::make_shared<neb::pb_global_diff_state>();
  auto& ddd_obj = diff->mut_obj();
  svc = ddd_obj.mutable_services()->add_removed();
  svc->set_host_id(1);
  svc->set_service_id(1);
  svc = ddd_obj.mutable_services()->add_removed();
  svc->set_host_id(1);
  svc->set_service_id(2);
  sgp = ddd_obj.mutable_servicegroups()->add_removed();
  sgp->set_poller_id(1);
  sgp->set_group_name("servicegroup_1");
  auto sss = ddd_obj.mutable_services()->add_modified();
  sss->set_host_id(2);
  sss->set_service_id(4);
  sss->set_service_description("foo_4");
  sss->set_host_name("host_1");

  sss = ddd_obj.mutable_services()->add_added();
  sss->set_host_id(2);
  sss->set_service_id(50);
  sss->set_service_description("service_50");
  sss->set_host_name("host_2");

  _cache->publish(diff);

  ASSERT_THAT(_cache->servicegroup_members(19), ::testing::IsEmpty());
  ASSERT_EQ(_cache->service(2, 4)->obj().description(), "foo_4");
  ASSERT_EQ(_cache->service(2, 50)->obj().description(), "service_50");

  diff = std::make_shared<neb::pb_global_diff_state>();
  auto& last_obj = diff->mut_obj();
  sg = last_obj.mutable_servicegroups()->add_modified();
  sg->set_servicegroup_id(19);
  sg->set_servicegroup_name("new_name");
  sg->set_poller_id(1);
  fill_pair_string_group(sg->mutable_members(), "host_2, service_50");
  _cache->publish(diff);

  ASSERT_THAT(_cache->servicegroup_members(19),
              ::testing::ElementsAre(std::make_pair(2, 50)));
}

TEST_F(BrokerCacheTest, UpdateHostgroup) {
  publish_hosts(1, 1, 1);

  auto hg = std::make_shared<neb::pb_host_group>();
  auto& obj = hg->mut_obj();
  obj.set_hostgroup_id(1);
  obj.set_name("hg1");
  obj.set_alias("alias hg1");
  obj.set_enabled(true);
  obj.set_poller_id(1);
  _cache->update_hostgroup(hg);

  ASSERT_EQ(_cache->hostgroup(1)->obj().name(), "hg1");
  ASSERT_EQ(_cache->hostgroup(1)->obj().alias(), "alias hg1");

  auto hgm = std::make_shared<neb::pb_host_group_member>();
  {
    auto& hgm_obj = hgm->mut_obj();
    hgm_obj.set_hostgroup_id(1);
    hgm_obj.set_host_id(1);
    hgm_obj.set_name("hg1");
    hgm_obj.set_poller_id(1);
    hgm_obj.set_enabled(true);
  }
  _cache->update_hostgroup_member(hgm);

  hg = std::make_shared<neb::pb_host_group>();
  {
    auto& nobj = hg->mut_obj();
    nobj.set_hostgroup_id(1);
    nobj.set_name("new_hg1");
    nobj.set_poller_id(1);
    nobj.set_enabled(true);
    nobj.set_alias("alias new hg1");
  }
  _cache->update_hostgroup(hg);

  hgm = std::make_shared<neb::pb_host_group_member>();
  {
    auto& hgm_obj = hgm->mut_obj();
    hgm_obj.set_hostgroup_id(1);
    hgm_obj.set_host_id(1);
    hgm_obj.set_name("new_hg1");
    hgm_obj.set_poller_id(1);
    hgm_obj.set_enabled(true);
  }
  _cache->update_hostgroup_member(hgm);

  ASSERT_EQ(_cache->hostgroup(1)->obj().name(), "new_hg1");
  ASSERT_EQ(_cache->hostgroup(1)->obj().alias(), "alias new hg1");

  hgm = std::make_shared<neb::pb_host_group_member>();
  {
    auto& hgm_obj = hgm->mut_obj();
    hgm_obj.set_hostgroup_id(1);
    hgm_obj.set_host_id(1);
    hgm_obj.set_name("new_hg1");
    hgm_obj.set_poller_id(1);
    hgm_obj.set_enabled(false);
  }

  _cache->update_hostgroup_member(hgm);

  hg = std::make_shared<neb::pb_host_group>();
  {
    auto& nobj = hg->mut_obj();
    nobj.set_hostgroup_id(1);
    nobj.set_name("new_hg1");
    nobj.set_poller_id(1);
    nobj.set_enabled(false);
    nobj.set_alias("alias new hg1");
  }
  _cache->update_hostgroup(hg);

  ASSERT_TRUE(!_cache->hostgroup(1));

  ASSERT_THAT(_cache->hostgroup_members(1), ::testing::IsEmpty());
}

TEST_F(BrokerCacheTest, UpdateHostgroupMemberWithoutHostgroup) {
  publish_hosts(1, 1, 1);

  auto hgm = std::make_shared<neb::pb_host_group_member>();
  {
    auto& hgm_obj = hgm->mut_obj();
    hgm_obj.set_hostgroup_id(1);
    hgm_obj.set_host_id(1);
    hgm_obj.set_name("hg1");
    hgm_obj.set_poller_id(1);
    hgm_obj.set_enabled(true);
  }
  _cache->update_hostgroup_member(hgm);

  ASSERT_EQ(_cache->hostgroup(1)->obj().name(), "hg1");
  ASSERT_THAT(_cache->hostgroup_members(1), ::testing::ElementsAre(1u));

  hgm = std::make_shared<neb::pb_host_group_member>();
  {
    auto& hgm_obj = hgm->mut_obj();
    hgm_obj.set_hostgroup_id(1);
    hgm_obj.set_host_id(1);
    hgm_obj.set_name("new_hg1");
    hgm_obj.set_poller_id(1);
    hgm_obj.set_enabled(true);
  }
  _cache->update_hostgroup_member(hgm);

  ASSERT_EQ(_cache->hostgroup(1)->obj().name(), "new_hg1");
  ASSERT_THAT(_cache->hostgroup_members(1), ::testing::ElementsAre(1u));
}

/**
 * @brief A hostgroup is stored once but referenced twice: by _hostgroups and by
 * the host/hostgroup relations. A rename must be visible through both, and
 * through the by_name index.
 */
TEST_F(BrokerCacheTest, UpdateHostgroupRenameSeenThroughRelations) {
  publish_hosts(1, 1, 1);

  auto hgm = std::make_shared<neb::pb_host_group_member>();
  {
    auto& hgm_obj = hgm->mut_obj();
    hgm_obj.set_hostgroup_id(1);
    hgm_obj.set_host_id(1);
    hgm_obj.set_name("hg1");
    hgm_obj.set_poller_id(1);
    hgm_obj.set_enabled(true);
  }
  _cache->update_hostgroup_member(hgm);

  ASSERT_EQ(_cache->hostgroups(1).size(), 1u);
  ASSERT_EQ(_cache->hostgroups(1)[0]->obj().name(), "hg1");

  auto hg = std::make_shared<neb::pb_host_group>();
  {
    auto& obj = hg->mut_obj();
    obj.set_hostgroup_id(1);
    obj.set_name("hg1_renamed");
    obj.set_alias("alias renamed");
    obj.set_enabled(true);
    obj.set_poller_id(1);
  }
  _cache->update_hostgroup(hg);

  ASSERT_EQ(_cache->hostgroup(1)->obj().name(), "hg1_renamed");
  /* The relations must not keep pointing at the previous version. */
  ASSERT_EQ(_cache->hostgroups(1).size(), 1u);
  ASSERT_EQ(_cache->hostgroups(1)[0]->obj().name(), "hg1_renamed");
  ASSERT_EQ(_cache->hostgroups(1)[0]->obj().alias(), "alias renamed");
  /* And the by_name index must have been re-keyed. */
  ASSERT_TRUE(_cache->hostgroup("hg1_renamed"));
  ASSERT_EQ(_cache->hostgroup("hg1_renamed")->obj().hostgroup_id(), 1u);
  ASSERT_FALSE(_cache->hostgroup("hg1"));
}

/**
 * @brief Renaming a hostgroup to a name already used by another one must be
 * refused, leaving both groups untouched instead of dropping one of them.
 */
TEST_F(BrokerCacheTest, UpdateHostgroupMemberRenameCollision) {
  publish_hosts(1, 1, 1);

  const std::pair<uint64_t, std::string> groups[] = {{1, "hg1"}, {2, "hg2"}};
  for (const auto& [hg_id, name] : groups) {
    auto hgm = std::make_shared<neb::pb_host_group_member>();
    auto& hgm_obj = hgm->mut_obj();
    hgm_obj.set_hostgroup_id(hg_id);
    hgm_obj.set_host_id(1);
    hgm_obj.set_name(name);
    hgm_obj.set_poller_id(1);
    hgm_obj.set_enabled(true);
    _cache->update_hostgroup_member(hgm);
  }

  ASSERT_EQ(_cache->hostgroups(1).size(), 2u);

  /* Group 2 claims the name of group 1: the by_name index cannot hold both. */
  auto hgm = std::make_shared<neb::pb_host_group_member>();
  {
    auto& hgm_obj = hgm->mut_obj();
    hgm_obj.set_hostgroup_id(2);
    hgm_obj.set_host_id(1);
    hgm_obj.set_name("hg1");
    hgm_obj.set_poller_id(1);
    hgm_obj.set_enabled(true);
  }
  _cache->update_hostgroup_member(hgm);

  /* Both groups are still there, with their original names. */
  ASSERT_TRUE(_cache->hostgroup(1));
  ASSERT_EQ(_cache->hostgroup(1)->obj().name(), "hg1");
  ASSERT_TRUE(_cache->hostgroup(2));
  ASSERT_EQ(_cache->hostgroup(2)->obj().name(), "hg2");
  ASSERT_EQ(_cache->hostgroup("hg1")->obj().hostgroup_id(), 1u);
  ASSERT_EQ(_cache->hostgroup("hg2")->obj().hostgroup_id(), 2u);
  ASSERT_EQ(_cache->hostgroups(1).size(), 2u);
}

TEST_F(BrokerCacheTest, UpdateServicegroup) {
  publish_hosts(1, 1, 1);
  publish_services(1, 1, 1);

  auto sg = std::make_shared<neb::pb_service_group>();
  {
    auto& obj = sg->mut_obj();
    obj.set_servicegroup_id(1);
    obj.set_name("sg1");
    obj.set_alias("alias sg1");
    obj.set_poller_id(1);
    obj.set_enabled(true);
  }
  _cache->update_servicegroup(sg);

  ASSERT_EQ(_cache->servicegroup(1)->obj().name(), "sg1");
  ASSERT_EQ(_cache->servicegroup(1)->obj().alias(), "alias sg1");

  sg = std::make_shared<neb::pb_service_group>();
  {
    auto& obj = sg->mut_obj();
    obj.set_servicegroup_id(1);
    obj.set_name("new_sg1");
    obj.set_enabled(true);
    obj.set_poller_id(1);
    obj.set_alias("alias new sg1");
  }
  _cache->update_servicegroup(sg);

  ASSERT_EQ(_cache->servicegroup(1)->obj().name(), "new_sg1");

  auto sgm = std::make_shared<neb::pb_service_group_member>();
  {
    auto& sgm_obj = sgm->mut_obj();
    sgm_obj.set_servicegroup_id(1);
    sgm_obj.set_host_id(1);
    sgm_obj.set_service_id(1);
    sgm_obj.set_name("sg1");
    sgm_obj.set_enabled(true);
    sgm_obj.set_poller_id(1);
  }
  _cache->update_servicegroup_member(sgm);

  ASSERT_THAT(_cache->servicegroup_members(1),
              ::testing::ElementsAre(std::make_pair(1, 1)));
  ASSERT_EQ(_cache->servicegroup(1)->obj().name(), "sg1");
  ASSERT_EQ(_cache->servicegroup(1)->obj().alias(), "alias new sg1");

  sgm = std::make_shared<neb::pb_service_group_member>();
  {
    auto& sgm_obj = sgm->mut_obj();
    sgm_obj.set_servicegroup_id(1);
    sgm_obj.set_host_id(1);
    sgm_obj.set_service_id(1);
    sgm_obj.set_name("new_sg1");
    sgm_obj.set_enabled(true);
    sgm_obj.set_poller_id(1);
  }
  _cache->update_servicegroup_member(sgm);

  ASSERT_THAT(_cache->servicegroup_members(1),
              ::testing::ElementsAre(std::make_pair(1, 1)));
  ASSERT_EQ(_cache->servicegroup(1)->obj().name(), "new_sg1");

  sgm = std::make_shared<neb::pb_service_group_member>();
  {
    auto& sgm_obj = sgm->mut_obj();
    sgm_obj.set_servicegroup_id(1);
    sgm_obj.set_host_id(1);
    sgm_obj.set_service_id(1);
    sgm_obj.set_name("new_sg1");
    sgm_obj.set_enabled(false);
    sgm_obj.set_poller_id(1);
  }
  _cache->update_servicegroup_member(sgm);

  sg = std::make_shared<neb::pb_service_group>();
  {
    auto& sg_obj = sg->mut_obj();
    sg_obj.set_servicegroup_id(1);
    sg_obj.set_name("new_sg1");
    sg_obj.set_enabled(false);
    sg_obj.set_poller_id(1);
  }
  _cache->update_servicegroup(sg);
  ASSERT_THAT(_cache->servicegroup_members(1), ::testing::IsEmpty());
  ASSERT_TRUE(!_cache->servicegroup(1));
}

TEST_F(BrokerCacheTest, UpdateServicegroupMemberWithoutServicegroup) {
  publish_hosts(1, 1, 1);
  publish_services(1, 1, 1);

  auto sgm = std::make_shared<neb::pb_service_group_member>();
  {
    auto& sgm_obj = sgm->mut_obj();
    sgm_obj.set_servicegroup_id(1);
    sgm_obj.set_host_id(1);
    sgm_obj.set_service_id(1);
    sgm_obj.set_name("sg1");
    sgm_obj.set_poller_id(1);
    sgm_obj.set_enabled(true);
  }
  _cache->update_servicegroup_member(sgm);

  ASSERT_EQ(_cache->servicegroup(1)->obj().name(), "sg1");
  ASSERT_THAT(_cache->servicegroup_members(1),
              ::testing::ElementsAre(std::pair{1u, 1u}));

  sgm = std::make_shared<neb::pb_service_group_member>();
  {
    auto& sgm_obj = sgm->mut_obj();
    sgm_obj.set_servicegroup_id(1);
    sgm_obj.set_host_id(1);
    sgm_obj.set_service_id(1);
    sgm_obj.set_name("new_sg1");
    sgm_obj.set_poller_id(1);
    sgm_obj.set_enabled(true);
  }
  _cache->update_servicegroup_member(sgm);

  ASSERT_EQ(_cache->servicegroup(1)->obj().name(), "new_sg1");
  ASSERT_THAT(_cache->servicegroup_members(1),
              ::testing::ElementsAre(std::pair{1u, 1u}));
}

TEST_F(BrokerCacheTest, Merge) {
  com::centreon::engine::configuration::State state;
  auto* hg = state.mutable_hostgroups()->Add();
  hg->set_hostgroup_id(1);
  hg->set_hostgroup_name("hg1");
  hg->set_alias("alias hg1");
  hg->set_poller_id(1);

  hg = state.mutable_hostgroups()->Add();
  hg->set_hostgroup_id(2);
  hg->set_hostgroup_name("hg2");
  hg->set_alias("alias hg2");
  hg->set_poller_id(1);

  hg = state.mutable_hostgroups()->Add();
  hg->set_hostgroup_id(3);
  hg->set_hostgroup_name("hg3");
  hg->set_alias("alias hg3");
  hg->set_poller_id(1);

  auto* sg = state.mutable_servicegroups()->Add();
  sg->set_servicegroup_id(1);
  sg->set_servicegroup_name("sg1");
  sg->set_alias("alias sg1");
  sg->set_poller_id(1);

  sg = state.mutable_servicegroups()->Add();
  sg->set_servicegroup_id(2);
  sg->set_servicegroup_name("sg2");
  sg->set_alias("alias sg2");
  sg->set_poller_id(1);

  sg = state.mutable_servicegroups()->Add();
  sg->set_servicegroup_id(3);
  sg->set_servicegroup_name("sg3");
  sg->set_alias("alias sg3");
  sg->set_poller_id(1);
  _cache->merge(state);

  ASSERT_EQ(_cache->hostgroup(1)->obj().name(), "hg1");
  ASSERT_EQ(_cache->hostgroup(2)->obj().name(), "hg2");
  ASSERT_EQ(_cache->hostgroup(3)->obj().name(), "hg3");

  ASSERT_EQ(_cache->servicegroup(1)->obj().name(), "sg1");
  ASSERT_EQ(_cache->servicegroup(2)->obj().name(), "sg2");
  ASSERT_EQ(_cache->servicegroup(3)->obj().name(), "sg3");

  state.mutable_hostgroups(0)->set_hostgroup_name("new_hg1");
  hg = state.mutable_hostgroups()->Add();
  hg->set_hostgroup_id(4);
  hg->set_hostgroup_name("hg4");
  hg->set_alias("alias hg4");
  hg->set_poller_id(1);

  state.mutable_servicegroups(0)->set_servicegroup_name("new_sg1");
  sg = state.mutable_servicegroups()->Add();
  sg->set_servicegroup_id(5);
  sg->set_servicegroup_name("sg5");
  sg->set_alias("alias sg5");
  sg->set_poller_id(1);

  _cache->merge(state);

  ASSERT_EQ(_cache->hostgroup(1)->obj().name(), "new_hg1");
  ASSERT_EQ(_cache->hostgroup(4)->obj().name(), "hg4");
  ASSERT_EQ(_cache->servicegroup(1)->obj().name(), "new_sg1");
  ASSERT_EQ(_cache->servicegroup(5)->obj().name(), "sg5");
}

TEST_F(BrokerCacheTest, InstanceIntervalLength) {
  using namespace std::chrono_literals;

  /* Unknown poller: the Engine default. */
  ASSERT_EQ(_cache->interval_length(1), 60s);

  /* merge(State) feeds the poller's interval_length. */
  com::centreon::engine::configuration::State state;
  state.set_poller_id(1);
  state.set_poller_name("poller1");
  state.set_interval_length(30);
  _cache->merge(state);
  ASSERT_EQ(_cache->instance(1), "poller1");
  ASSERT_EQ(_cache->interval_length(1), 30s);

  /* A diff can hot-change it without resending the whole state. */
  com::centreon::engine::configuration::DiffState diff;
  diff.set_poller_id(1);
  diff.set_interval_length(10);
  _cache->apply(diff);
  ASSERT_EQ(_cache->interval_length(1), 10s);

  /* A diff leaving interval_length at 0 (not a valid value, meaning "not part
   * of the diff") leaves the cached value untouched. */
  com::centreon::engine::configuration::DiffState diff_no_field;
  diff_no_field.set_poller_id(1);
  _cache->apply(diff_no_field);
  ASSERT_EQ(_cache->interval_length(1), 10s);

  /* The neb Instance event carries no interval_length: it refreshes the
   * poller's name but preserves the cached value. */
  auto inst = std::make_shared<neb::pb_instance>();
  inst->mut_obj().set_instance_id(1);
  inst->mut_obj().set_name("renamed");
  inst->mut_obj().set_running(true);
  _cache->update_instance(inst);
  ASSERT_EQ(_cache->instance(1), "renamed");
  ASSERT_EQ(_cache->interval_length(1), 10s);

  /* A State not carrying interval_length (proto3 zero) falls back to the
   * default instead of storing a zero multiplier. */
  com::centreon::engine::configuration::State state2;
  state2.set_poller_id(2);
  state2.set_poller_name("poller2");
  _cache->merge(state2);
  ASSERT_EQ(_cache->interval_length(2), 60s);
}

/**
 * @brief The cache resolves a resource's notification timeperiod by name.
 *
 * in_notification_period() answers whether a given instant falls in a
 * timeperiod. An empty or unknown name means "notify at any time"; a known
 * timeperiod is evaluated against its timeranges (a 24x7 one is always valid,
 * an empty one never is). This also checks that the set is kept in sync by
 * both feeding paths: the full state (merge()) and an incremental diff
 * (apply(), covering add, modify and remove).
 */
TEST_F(BrokerCacheTest, NotificationPeriod) {
  std::time_t now = std::time(nullptr);

  /* An empty or unknown period name means "notify at any time". */
  ASSERT_TRUE(_cache->in_notification_period("", "", now));
  ASSERT_TRUE(_cache->in_notification_period("nonexistent", "", now));

  /* Feed a 24x7 timeperiod and an empty (never valid) one via merge(State). */
  com::centreon::engine::configuration::State state;
  state.set_poller_id(1);
  auto* always = state.mutable_timeperiods()->Add();
  always->set_timeperiod_name("always");
  always->set_alias("always");
  auto add_full_day = [](auto* day) {
    auto* r = day->Add();
    r->set_range_start(0);
    r->set_range_end(86400);
  };
  auto* days = always->mutable_timeranges();
  add_full_day(days->mutable_sunday());
  add_full_day(days->mutable_monday());
  add_full_day(days->mutable_tuesday());
  add_full_day(days->mutable_wednesday());
  add_full_day(days->mutable_thursday());
  add_full_day(days->mutable_friday());
  add_full_day(days->mutable_saturday());
  auto* never = state.mutable_timeperiods()->Add();
  never->set_timeperiod_name("never");
  never->set_alias("never");
  _cache->merge(state);

  ASSERT_TRUE(_cache->in_notification_period("always", "", now));
  ASSERT_FALSE(_cache->in_notification_period("never", "", now));

  /* A diff can add, modify and remove timeperiods. */
  com::centreon::engine::configuration::DiffState diff;
  diff.set_poller_id(1);
  diff.mutable_timeperiods()->add_removed("never");
  auto* modified = diff.mutable_timeperiods()->add_modified();
  modified->set_timeperiod_name("always");
  modified->set_alias("always"); /* all timeranges gone: never valid */
  _cache->apply(diff);

  /* Removed: no longer constrains anything. Modified: emptied, never valid. */
  ASSERT_TRUE(_cache->in_notification_period("never", "", now));
  ASSERT_FALSE(_cache->in_notification_period("always", "", now));
}

/**
 * @brief Notification timeperiods are reference-counted per poller.
 *
 * When several pollers define the same timeperiod, it stays in the cache as
 * long as at least one poller references it, and is dropped only when the last
 * reference disappears. This checks both removal paths: a diff removing the
 * timeperiod from one poller (it must survive while another still references
 * it) and a poller disconnection through remove_instance() (the last reference
 * gone, the timeperiod must be dropped, so an unknown name again means "notify
 * at any time").
 */
TEST_F(BrokerCacheTest, NotificationPeriodPollerRefCount) {
  std::time_t now = std::time(nullptr);

  auto make_never_tp = [](auto* state) {
    auto* tp = state->mutable_timeperiods()->Add();
    tp->set_timeperiod_name("shared");
    tp->set_alias("shared"); /* no timerange: never valid */
  };

  /* Two pollers define the same "shared" (never-valid) timeperiod. */
  com::centreon::engine::configuration::State state1;
  state1.set_poller_id(1);
  make_never_tp(&state1);
  _cache->merge(state1);

  com::centreon::engine::configuration::State state2;
  state2.set_poller_id(2);
  make_never_tp(&state2);
  _cache->merge(state2);

  ASSERT_FALSE(_cache->in_notification_period("shared", "", now));

  /* Poller 1 drops it via a diff: still referenced by poller 2, so it stays. */
  com::centreon::engine::configuration::DiffState diff;
  diff.set_poller_id(1);
  diff.mutable_timeperiods()->add_removed("shared");
  _cache->apply(diff);
  ASSERT_FALSE(_cache->in_notification_period("shared", "", now));

  /* Poller 2 disconnects: last reference gone, the timeperiod is dropped and
   * an unknown name means "notify at any time" again. */
  _cache->remove_instance(2);
  ASSERT_TRUE(_cache->in_notification_period("shared", "", now));
}

/**
 * @brief Notification dependencies are cached per poller, resolved to ids.
 *
 * merge(State) rebuilds a poller's notification host/service dependencies,
 * resolving the referenced names to ids via the hosts/services carried by the
 * same state; only notification-typed dependencies are kept. A DiffState then
 * adds/removes them incrementally (removal matched by the engine_conf key), and
 * remove_instance() purges the poller's whole set.
 */
TEST_F(BrokerCacheTest, NotificationDependencies) {
  namespace cfg = com::centreon::engine::configuration;

  cfg::State state;
  state.set_poller_id(1);
  state.set_soft_state_dependencies(true);

  for (uint64_t id : {1u, 2u}) {
    auto* h = state.mutable_hosts()->Add();
    h->set_host_id(id);
    h->set_host_name(fmt::format("host_{}", id));
  }
  for (uint64_t svc_id : {10u, 20u}) {
    auto* s = state.mutable_services()->Add();
    s->set_host_id(1);
    s->set_service_id(svc_id);
    s->set_host_name("host_1");
    s->set_service_description(fmt::format("service_{}", svc_id));
  }

  /* Notification host dependency: host_2 depends on host_1. */
  auto* hd = state.mutable_hostdependencies()->Add();
  hd->set_dependency_type(cfg::notification_dependency);
  hd->mutable_dependent_hosts()->add_data("host_2");
  hd->mutable_hosts()->add_data("host_1");
  hd->set_inherits_parent(true);
  hd->set_notification_failure_options(4);
  hd->set_dependency_period("24x7");

  /* Execution dependency: must be ignored. */
  auto* hd_exec = state.mutable_hostdependencies()->Add();
  hd_exec->set_dependency_type(cfg::execution_dependency);
  hd_exec->mutable_dependent_hosts()->add_data("host_2");
  hd_exec->mutable_hosts()->add_data("host_1");

  /* Notification service dependency: (1,20) depends on (1,10). */
  auto* sd = state.mutable_servicedependencies()->Add();
  sd->set_dependency_type(cfg::notification_dependency);
  sd->mutable_dependent_hosts()->add_data("host_1");
  sd->mutable_dependent_service_description()->add_data("service_20");
  sd->mutable_hosts()->add_data("host_1");
  sd->mutable_service_description()->add_data("service_10");
  sd->set_notification_failure_options(8);

  _cache->merge(state);

  ASSERT_TRUE(_cache->soft_state_dependencies(1));

  auto hdeps = _cache->host_notif_dependencies(2);
  ASSERT_EQ(hdeps.size(), 1u); /* the execution dependency was dropped */
  ASSERT_EQ(hdeps[0].dependent_host_id, 2u);
  ASSERT_EQ(hdeps[0].master_host_id, 1u);
  ASSERT_EQ(hdeps[0].poller_id, 1u);
  ASSERT_TRUE(hdeps[0].inherits_parent);
  ASSERT_EQ(hdeps[0].notification_failure_options, 4u);
  ASSERT_EQ(hdeps[0].dependency_period, "24x7");

  auto sdeps = _cache->service_notif_dependencies(1, 20);
  ASSERT_EQ(sdeps.size(), 1u);
  ASSERT_EQ(sdeps[0].master_host_id, 1u);
  ASSERT_EQ(sdeps[0].master_service_id, 10u);
  ASSERT_EQ(sdeps[0].notification_failure_options, 8u);

  /* A diff removes the host dependency by its key and adds a new one where
   * host_1 depends on host_2. */
  cfg::DiffState diff;
  diff.set_poller_id(1);
  diff.mutable_hostdependencies()->add_removed(cfg::hostdependency_key(*hd));
  auto* hd2 = diff.mutable_hostdependencies()->add_added();
  hd2->set_dependency_type(cfg::notification_dependency);
  hd2->mutable_dependent_hosts()->add_data("host_1");
  hd2->mutable_hosts()->add_data("host_2");
  _cache->apply(diff);

  ASSERT_TRUE(_cache->host_notif_dependencies(2).empty());
  auto hdeps2 = _cache->host_notif_dependencies(1);
  ASSERT_EQ(hdeps2.size(), 1u);
  ASSERT_EQ(hdeps2[0].master_host_id, 2u);

  /* remove_instance purges the poller's whole dependency set. */
  _cache->remove_instance(1);
  ASSERT_TRUE(_cache->host_notif_dependencies(1).empty());
  ASSERT_TRUE(_cache->service_notif_dependencies(1, 20).empty());
}

/**
 * @brief Notification data is gated behind CACHE_NOTIFICATIONS.
 *
 * A cache with only CACHE_HOSTS | CACHE_SERVICES enabled (a typical unified_sql
 * central broker that does not compute notifications) must not store the
 * notification-only data — timeperiods and notification dependencies — even
 * though the referenced hosts/services are cached. So an unknown timeperiod
 * name means "notify at any time" and the dependency lookups stay empty.
 */
TEST_F(BrokerCacheTest, NotificationSectionGating) {
  namespace cfg = com::centreon::engine::configuration;

  broker_cache local_cache(_logger);
  local_cache.enable_section(broker_cache::CACHE_HOSTS |
                             broker_cache::CACHE_SERVICES);

  cfg::State state;
  state.set_poller_id(1);

  auto* h1 = state.mutable_hosts()->Add();
  h1->set_host_id(1);
  h1->set_host_name("host_1");
  auto* h2 = state.mutable_hosts()->Add();
  h2->set_host_id(2);
  h2->set_host_name("host_2");

  /* A never-valid timeperiod: if it were stored, in_notification_period() would
   * answer false. */
  auto* never = state.mutable_timeperiods()->Add();
  never->set_timeperiod_name("never");
  never->set_alias("never");

  auto* hd = state.mutable_hostdependencies()->Add();
  hd->set_dependency_type(cfg::notification_dependency);
  hd->mutable_dependent_hosts()->add_data("host_2");
  hd->mutable_hosts()->add_data("host_1");

  local_cache.merge(state);

  /* Hosts are cached (their section is on) ... */
  ASSERT_TRUE(local_cache.host(1u) != nullptr);
  /* ... but the notification-only data is not: an unknown timeperiod means
   * "notify at any time", and the dependency set is empty. */
  ASSERT_TRUE(
      local_cache.in_notification_period("never", "", std::time(nullptr)));
  ASSERT_TRUE(local_cache.host_notif_dependencies(2).empty());
}
