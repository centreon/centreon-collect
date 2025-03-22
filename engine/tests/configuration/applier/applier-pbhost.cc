/**
 * Copyright 2023 Centreon (https://www.centreon.com/)
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

#include "../../timeperiod/utils.hh"
#include "com/centreon/engine/configuration/applier/command.hh"
#include "com/centreon/engine/configuration/applier/host.hh"
#include "com/centreon/engine/configuration/applier/service.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/host.hh"
#include "com/centreon/engine/timezone_manager.hh"
#include "common/engine_conf/command_helper.hh"
#include "common/engine_conf/host_helper.hh"
#include "common/engine_conf/service_helper.hh"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;
using namespace com::centreon::engine::configuration::applier;

class ApplierPbHost : public ::testing::Test {
 public:
  void SetUp() override { init_config_state(); }

  void TearDown() override { deinit_config_state(); }
};

// Given host configuration without host_id
// Then the applier add_object throws an exception.
TEST_F(ApplierPbHost, PbNewHostWithoutHostId) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  ASSERT_THROW(hst_aply.add_object(hst), std::exception);
}

// Given a host configuration
// When we change the host name in the configuration
// Then the applier modify_object changes the host name without changing
// the host id.
TEST_F(ApplierPbHost, HostRenamed) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst_aply.add_object(hst);
  host_map const& hm(engine::host::hosts);
  ASSERT_EQ(hm.size(), 1u);
  std::shared_ptr<com::centreon::engine::host> h1(hm.begin()->second);
  ASSERT_TRUE(h1->name() == "test_host");

  hst.set_host_name("test_host1");
  hst_aply.modify_object(&pb_indexed_config.state().mutable_hosts()->at(0),
                         hst);
  ASSERT_EQ(hm.size(), 1u);
  h1 = hm.begin()->second;
  ASSERT_TRUE(h1->name() == "test_host1");
  ASSERT_EQ(get_host_id(h1->name()), 12u);
}

TEST_F(ApplierPbHost, PbHostRemoved) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst_aply.add_object(hst);
  host_map const& hm(engine::host::hosts);
  ASSERT_EQ(hm.size(), 1u);
  std::shared_ptr<com::centreon::engine::host> h1(hm.begin()->second);
  ASSERT_TRUE(h1->name() == "test_host");

  hst_aply.remove_object(0);

  ASSERT_EQ(hm.size(), 0u);
  hst.set_host_name("test_host1");
  hst_aply.add_object(hst);
  h1 = hm.begin()->second;
  ASSERT_EQ(hm.size(), 1u);
  ASSERT_TRUE(h1->name() == "test_host1");
  ASSERT_EQ(get_host_id(h1->name()), 12u);
}

TEST_F(ApplierPbHost, PbHostParentChildUnreachable) {
  configuration::error_cnt err;
  configuration::applier::host hst_aply;
  configuration::applier::command cmd_aply;
  configuration::Host hst_child;
  configuration::host_helper hst_child_hlp(&hst_child);
  configuration::Host hst_parent;
  configuration::host_helper hst_parent_hlp(&hst_parent);

  configuration::Command cmd;
  configuration::command_helper cmd_hlp(&cmd);
  cmd.set_command_name("base_centreon_ping");
  cmd.set_command_line(
      "$USER1$/check_icmp -H $HOSTADDRESS$ -n $_HOSTPACKETNUMBER$ -w "
      "$_HOSTWARNING$ -c $_HOSTCRITICAL$");
  cmd_aply.add_object(cmd);

  hst_child.set_host_name("child_host");
  hst_child.set_address("127.0.0.1");
  hst_child_hlp.hook("parents", "parent_host");
  hst_child.set_host_id(1);
  hst_child_hlp.hook("_PACKETNUMBER", "42");
  hst_child_hlp.hook("_WARNING", "200,20%");
  hst_child_hlp.hook("_CRITICAL", "400,50%");
  hst_child.set_check_command("base_centreon_ping");
  hst_aply.add_object(hst_child);

  hst_parent.set_host_name("parent_host");
  hst_parent.set_address("127.0.0.1");
  hst_parent.set_host_id(2);
  hst_parent_hlp.hook("_PACKETNUMBER", "42");
  hst_parent_hlp.hook("_WARNING", "200,20%");
  hst_parent_hlp.hook("_CRITICAL", "400,50%");
  hst_parent.set_check_command("base_centreon_ping");
  hst_aply.add_object(hst_parent);

  ASSERT_EQ(engine::host::hosts.size(), 2u);

  hst_aply.expand_objects(pb_indexed_config.state());
  hst_aply.resolve_object(hst_child, err);
  hst_aply.resolve_object(hst_parent, err);

  host_map::iterator child = engine::host::hosts.find("child_host");
  host_map::iterator parent = engine::host::hosts.find("parent_host");

  ASSERT_EQ(parent->second->child_hosts.size(), 1u);
  ASSERT_EQ(child->second->parent_hosts.size(), 1u);

  engine::host::host_state result;
  parent->second->run_sync_check_3x(&result, 0, 0, 0);
  ASSERT_EQ(parent->second->get_current_state(), engine::host::state_down);
  child->second->run_sync_check_3x(&result, 0, 0, 0);
  ASSERT_EQ(child->second->get_current_state(),
            engine::host::state_unreachable);
}
