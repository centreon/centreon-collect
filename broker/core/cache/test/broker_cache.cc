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
#include <filesystem>
#include "broker/core/config/applier/broker_state.hh"
#include "com/centreon/broker/neb/internal.hh"
#include "common/engine_conf/message_helper.hh"
#include "gmock/gmock.h"
#include "neb.pb.h"

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
