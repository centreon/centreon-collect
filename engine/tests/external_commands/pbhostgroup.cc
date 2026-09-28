/**
 * Copyright 2026 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */

#include <gtest/gtest.h>

#include "com/centreon/engine/commands/processing.hh"
#include "com/centreon/engine/configuration/applier/host.hh"
#include "com/centreon/engine/configuration/applier/hostgroup.hh"
#include "com/centreon/engine/host.hh"
#include "com/centreon/engine/hostgroup.hh"
#include "helper.hh"

using namespace com::centreon::engine;

class HostgroupExternalCommand : public ::testing::Test {
 protected:
  std::unique_ptr<configuration::state_helper> _state_hlp;

 public:
  void SetUp() override { _state_hlp = init_config_state(); }
  void TearDown() override { deinit_config_state(); }
};

/* A hostgroup command must reach every member of the group. The loop bound of
 * _redirector_hostgroup was members.begin() for three years, so these
 * commands were silent no-ops: this test pins the fix. */
TEST_F(HostgroupExternalCommand, ToggleReachesEveryMember) {
  configuration::error_cnt err;
  configuration::applier::hostgroup hg_aply;
  configuration::applier::host hst_aply;
  configuration::Hostgroup hg;
  configuration::hostgroup_helper hg_hlp(&hg);

  std::vector<configuration::Host> hosts(3);
  for (uint32_t i = 0; i < hosts.size(); ++i) {
    configuration::host_helper hlp(&hosts[i]);
    hosts[i].set_host_name(fmt::format("h{}", i + 1));
    hosts[i].set_host_id(i + 1);
    hosts[i].set_address("127.0.0.1");
    hst_aply.add_object(hosts[i]);
  }
  hg.set_hostgroup_name("hg");
  hg_hlp.hook("members", "h1,h2,h3");
  ASSERT_NO_THROW(hg_aply.add_object(hg));
  ASSERT_NO_THROW(_state_hlp->expand(err));
  for (auto& h : hosts)
    ASSERT_NO_THROW(hst_aply.resolve_object(h));
  ASSERT_NO_THROW(hg_aply.resolve_object(hg));
  ASSERT_EQ(hostgroup::hostgroups["hg"]->members.size(), 3u);

  for (const auto& [name, hst] : host::hosts)
    ASSERT_TRUE(hst->active_checks_enabled());

  ASSERT_TRUE(
      commands::processing::execute("[1] DISABLE_HOSTGROUP_HOST_CHECKS;hg"));
  for (const auto& [name, hst] : host::hosts)
    EXPECT_FALSE(hst->active_checks_enabled()) << name;

  ASSERT_TRUE(
      commands::processing::execute("[1] ENABLE_HOSTGROUP_HOST_CHECKS;hg"));
  for (const auto& [name, hst] : host::hosts)
    EXPECT_TRUE(hst->active_checks_enabled()) << name;
}
