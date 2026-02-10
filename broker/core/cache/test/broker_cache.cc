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
#include "com/centreon/broker/neb/internal.hh"
#include "common/engine_conf/message_helper.hh"
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
    _cache = std::make_unique<broker_cache>(_logger);
  }
  void TearDown() override {}
  void publish_hosts(uint32_t from, uint32_t to, uint64_t poller_id = 1);
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
  key->set_type(0);

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
}
