/*
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
#include "com/centreon/engine/configuration/applier/command.hh"
#include "com/centreon/engine/configuration/applier/host.hh"
#include "com/centreon/engine/configuration/applier/hostgroup.hh"
#include "com/centreon/engine/macros/grab_host.hh"
#include "com/centreon/engine/timezone_manager.hh"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;
using namespace com::centreon::engine::configuration::applier;

class ApplierHostGroup : public ::testing::Test {
 protected:
  std::unique_ptr<configuration::state_helper> _state_hlp;

 public:
  void SetUp() override { _state_hlp = init_config_state(); }

  void TearDown() override { deinit_config_state(); }
};

// Given host configuration without host_id
// Then the applier add_object throws an exception.
TEST_F(ApplierHostGroup, PbNewHostGroup) {
  configuration::error_cnt err;
  configuration::applier::hostgroup hg_aply;
  configuration::applier::host hst_aply;
  configuration::Hostgroup hg;
  configuration::hostgroup_helper hg_hlp(&hg);
  configuration::Host hst_a;
  configuration::host_helper hst_a_hlp(&hst_a);
  configuration::Host hst_b;
  configuration::host_helper hst_b_hlp(&hst_b);
  configuration::Host hst_c;
  configuration::host_helper hst_c_hlp(&hst_c);

  hst_a.set_host_name("a");
  hst_a.set_host_id(1);
  hst_a.set_address("127.0.0.1");

  hst_b.set_host_name("b");
  hst_b.set_host_id(2);
  hst_b.set_address("127.0.0.1");

  hst_c.set_host_name("c");
  hst_c.set_host_id(3);
  hst_c.set_address("127.0.0.1");
  hst_aply.add_object(hst_a);
  hst_aply.add_object(hst_b);
  hst_aply.add_object(hst_c);

  hg.set_hostgroup_name("temphg");
  hg_hlp.hook("members", "a,b,c");
  ASSERT_NO_THROW(hg_aply.add_object(hg));

  ASSERT_NO_THROW(_state_hlp->expand(err));

  ASSERT_NO_THROW(hst_aply.resolve_object(hst_a, err));
  ASSERT_NO_THROW(hst_aply.resolve_object(hst_b, err));
  ASSERT_NO_THROW(hst_aply.resolve_object(hst_c, err));
  ASSERT_NO_THROW(hg_aply.resolve_object(hg, err));

  ASSERT_EQ(engine::hostgroup::hostgroups.size(), 1u);
  ASSERT_EQ(engine::hostgroup::hostgroups.begin()->second->members.size(), 3u);
}

// Given a host configuration
// When we change the host name in the configuration
// Then the applier modify_object changes the host name without changing
// the host id.
TEST_F(ApplierHostGroup, PbHostRenamed) {
  configuration::error_cnt err;
  configuration::applier::hostgroup hg_aply;
  configuration::applier::host hst_aply;
  configuration::Hostgroup hg;
  configuration::hostgroup_helper hg_hlp(&hg);
  configuration::Host hst_a;
  configuration::host_helper hst_a_hlp(&hst_a);
  configuration::Host hst_c;
  configuration::host_helper hst_c_hlp(&hst_c);

  hst_a.set_host_name("a");
  hst_a.set_host_id(1);
  hst_a.set_address("127.0.0.1");

  hst_c.set_host_name("c");
  hst_c.set_host_id(2);
  hst_c.set_address("127.0.0.1");

  hst_aply.add_object(hst_a);
  hst_aply.add_object(hst_c);

  hg.set_hostgroup_name("temphg");
  hg_hlp.hook("members", "a,c");
  ASSERT_NO_THROW(hg_aply.add_object(hg));

  ASSERT_NO_THROW(_state_hlp->expand(err));

  ASSERT_NO_THROW(hst_aply.resolve_object(hst_a, err));
  ASSERT_NO_THROW(hst_aply.resolve_object(hst_c, err));
  ASSERT_NO_THROW(hg_aply.resolve_object(hg, err));

  hg.mutable_members()->clear_data();
  hg_hlp.hook("members", "c");
  hg_aply.modify_object(
      &pb_indexed_config.mut_state().mutable_hostgroups()->at(0), hg);

  ASSERT_NO_THROW(_state_hlp->expand(err));

  ASSERT_EQ(engine::hostgroup::hostgroups.size(), 1u);
  ASSERT_EQ(engine::hostgroup::hostgroups.begin()->second->members.size(), 1u);
  ASSERT_EQ(engine::hostgroup::hostgroups.begin()->second->get_group_name(),
            "temphg");
}

TEST_F(ApplierHostGroup, PbHostRemoved) {
  configuration::error_cnt err;
  configuration::applier::hostgroup hg_aply;
  configuration::applier::host hst_aply;
  configuration::Hostgroup hg;
  configuration::hostgroup_helper hg_hlp(&hg);
  configuration::Host hst_a;
  configuration::host_helper hst_a_hlp(&hst_a);
  configuration::Host hst_c;
  configuration::host_helper hst_c_hlp(&hst_c);

  hst_a.set_host_name("a");
  hst_a.set_host_id(1);
  hst_a.set_address("127.0.0.1");

  hst_c.set_host_name("c");
  hst_c.set_host_id(2);
  hst_c.set_address("127.0.0.1");

  hst_aply.add_object(hst_a);
  hst_aply.add_object(hst_c);

  hg.set_hostgroup_name("temphg");
  hg_hlp.hook("members", "a,c");
  ASSERT_NO_THROW(hg_aply.add_object(hg));

  ASSERT_NO_THROW(_state_hlp->expand(err));

  ASSERT_NO_THROW(hst_aply.resolve_object(hst_a, err));
  ASSERT_NO_THROW(hst_aply.resolve_object(hst_c, err));
  ASSERT_NO_THROW(hg_aply.resolve_object(hg, err));

  engine::hostgroup* hg_obj{engine::hostgroup::hostgroups["temphg"].get()};
  ASSERT_EQ(hg_obj->members.size(), 2u);
  ASSERT_NO_THROW(hst_aply.remove_object(1));
  ASSERT_EQ(hg_obj->members.size(), 1u);

  hg.mutable_members()->clear_data();
  hg_hlp.hook("members", "c");
  ASSERT_NO_THROW(hg_aply.modify_object(
      &pb_indexed_config.mut_state().mutable_hostgroups()->at(0), hg));

  hg_aply.remove_object("temphg");
  ASSERT_TRUE(pb_indexed_config.state().hostgroups().empty());
  ASSERT_TRUE(engine::hostgroup::hostgroups.empty());
}
