/*
 * Copyright 2018 - 2019 Centreon (https://www.centreon.com/)
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
#include "com/centreon/engine/configuration/applier/servicegroup.hh"
#include "common/engine_conf/message_helper.hh"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;
using namespace com::centreon::engine::configuration::applier;

class ApplierServicegroup : public ::testing::Test {
 protected:
  std::unique_ptr<configuration::state_helper> _state_hlp;

 public:
  void SetUp() override { _state_hlp = init_config_state(); }

  void TearDown() override { deinit_config_state(); }
};

// Given a servicegroup applier
// And a configuration servicegroup
// When we modify the servicegroup configuration with a non existing
// servicegroup
// Then an exception is thrown.
TEST_F(ApplierServicegroup, PbModifyUnexistingServicegroupFromConfig) {
  configuration::applier::servicegroup aply;
  configuration::Servicegroup sg;
  configuration::servicegroup_helper sg_hlp(&sg);
  sg.set_servicegroup_name("test");
  fill_pair_string_group(sg.mutable_members(), "host1,service1");
  configuration::Servicegroup* new_sg =
      pb_indexed_config.mut_state().add_servicegroups();
  new_sg->CopyFrom(sg);
  ASSERT_THROW(aply.modify_object(new_sg, sg), std::exception);
}

// Given a servicegroup applier
// And a configuration servicegroup in configuration
// When we modify the servicegroup configuration
// Then the applier modify_object updates the servicegroup.
TEST_F(ApplierServicegroup, PbModifyServicegroupFromConfig) {
  configuration::applier::servicegroup aply;
  configuration::Servicegroup sg;
  configuration::servicegroup_helper sg_hlp(&sg);
  sg.set_servicegroup_name("test");
  fill_pair_string_group(sg.mutable_members(), "host1,service1");
  aply.add_object(sg);
  auto it = engine::servicegroup::servicegroups.find("test");
  ASSERT_TRUE(it->second->get_alias() == "test");

  sg.set_alias("test_renamed");
  aply.modify_object(pb_indexed_config.mut_state().mutable_servicegroups(0),
                     sg);
  it = engine::servicegroup::servicegroups.find("test");
  ASSERT_TRUE(it->second->get_alias() == "test_renamed");
}

// Given an empty servicegroup
// When the resolve_object() method is called
// Then no warning, nor error are given
TEST_F(ApplierServicegroup, PbResolveEmptyservicegroup) {
  configuration::error_cnt err;
  configuration::applier::servicegroup aplyr;
  configuration::Servicegroup grp;
  configuration::servicegroup_helper grp_hlp(&grp);
  grp.set_servicegroup_name("test");
  aplyr.add_object(grp);
  _state_hlp->expand(err);
  aplyr.resolve_object(grp, err);
  ASSERT_EQ(err.config_warnings, 0);
  ASSERT_EQ(err.config_errors, 0);
}

// Given a servicegroup with a non-existing service
// When the resolve_object() method is called
// Then an exception is thrown
// And the method returns 1 error
TEST_F(ApplierServicegroup, PbResolveInexistentService) {
  configuration::error_cnt err;
  configuration::applier::servicegroup aplyr;
  configuration::Servicegroup grp;
  configuration::servicegroup_helper grp_helper(&grp);
  grp.set_servicegroup_name("test");
  fill_pair_string_group(grp.mutable_members(), "host1,non_existing_service");
  aplyr.add_object(grp);
  _state_hlp->expand(err);
  ASSERT_THROW(aplyr.resolve_object(grp, err), std::exception);
  ASSERT_EQ(err.config_warnings, 0);
  ASSERT_EQ(err.config_errors, 1);
}

// Given a servicegroup with a service and a host
// When the resolve_object() method is called
// Then the service is really added to the service group.
TEST_F(ApplierServicegroup, PbResolveServicegroup) {
  configuration::error_cnt err;
  configuration::applier::host aply_hst;
  configuration::applier::service aply_svc;
  configuration::applier::command aply_cmd;
  configuration::applier::servicegroup aply_grp;
  configuration::Servicegroup grp;
  configuration::servicegroup_helper grp_hlp(&grp);
  grp.set_servicegroup_name("test_group");
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  configuration::Command cmd;
  configuration::command_helper cmd_hlp(&cmd);
  cmd.set_command_name("cmd");
  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  aply_hst.add_object(hst);
  configuration::Service svc;
  configuration::service_helper svc_hlp(&svc);
  svc.set_service_description("test");
  svc.set_host_name("test_host");
  svc.set_service_id(18);
  cmd.set_command_line("echo 1");
  svc.set_check_command("cmd");
  aply_cmd.add_object(cmd);

  // We fake here the expand_object on configuration::service
  svc.set_host_id(12);

  aply_svc.add_object(svc);
  fill_string_group(svc.mutable_servicegroups(), "test_group");
  fill_pair_string_group(grp.mutable_members(), "test_host,test");
  aply_grp.add_object(grp);
  _state_hlp->expand(err);
  ASSERT_NO_THROW(aply_grp.resolve_object(grp, err));
}

// Given a servicegroup with a service already configured
// And a second servicegroup configuration
// When we set the first one as servicegroup member to the second
// Then the parse method returns true and set the first one service
// to the second one.
TEST_F(ApplierServicegroup, PbSetServicegroupMembers) {
  configuration::applier::host aply_hst;
  configuration::applier::service aply_svc;
  configuration::applier::command aply_cmd;
  configuration::applier::servicegroup aply_grp;
  configuration::Servicegroup grp;
  configuration::servicegroup_helper grp_hlp(&grp);
  grp.set_servicegroup_name("test_group");
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  configuration::Command cmd;
  configuration::command_helper cmd_hlp(&cmd);
  cmd.set_command_name("cmd");
  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  aply_hst.add_object(hst);
  configuration::Service svc;
  configuration::service_helper svc_hlp(&svc);
  svc.set_service_description("test");
  svc.set_host_name("test_host");
  svc.set_service_id(18);
  cmd.set_command_line("echo 1");
  svc.set_check_command("cmd");
  aply_cmd.add_object(cmd);

  // We fake here the expand_object on configuration::service
  svc.set_host_id(12);

  configuration::error_cnt err;
  aply_svc.add_object(svc);
  fill_string_group(svc.mutable_servicegroups(), "test_group");
  fill_pair_string_group(grp.mutable_members(), "test_host,test");
  aply_grp.add_object(grp);
  _state_hlp->expand(err);
  aply_grp.resolve_object(grp, err);
  ASSERT_TRUE(grp.members().data().size() == 1);

  configuration::Servicegroup grp1;
  configuration::servicegroup_helper grp1_hlp(&grp1);
  grp1.set_servicegroup_name("big_group");
  fill_string_group(grp1.mutable_servicegroup_members(), "test_group");
  aply_grp.add_object(grp1);
  _state_hlp->expand(err);

  // grp1 must be reload because the expand_objects reload them totally.
  auto found = std::find_if(pb_indexed_config.state().servicegroups().begin(),
                            pb_indexed_config.state().servicegroups().end(),
                            [](const configuration::Servicegroup& sg) {
                              return sg.servicegroup_name() == "big_group";
                            });
  ASSERT_TRUE(found != pb_indexed_config.state().servicegroups().end());
  ASSERT_EQ(found->members().data().size(), 1);
}

// Given a servicegroup applier
// And a configuration servicegroup in configuration
// When we remove the configuration
// Then it is really removed
TEST_F(ApplierServicegroup, PbRemoveServicegroupFromConfig) {
  configuration::applier::host aply_hst;
  configuration::applier::service aply_svc;
  configuration::applier::command aply_cmd;
  configuration::applier::servicegroup aply_grp;
  configuration::Servicegroup grp;
  configuration::servicegroup_helper grp_hlp(&grp);
  grp.set_servicegroup_name("test_group");
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  configuration::Command cmd;
  configuration::command_helper cmd_hlp(&cmd);
  cmd.set_command_name("cmd");
  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  aply_hst.add_object(hst);
  configuration::Service svc;
  configuration::service_helper svc_hlp(&svc);
  svc.set_service_description("test");
  svc.set_host_name("test_host");
  svc.set_service_id(18);
  cmd.set_command_line("echo 1");
  svc.set_check_command("cmd");
  aply_cmd.add_object(cmd);

  // We fake here the expand_object on configuration::service
  svc.set_host_id(12);

  aply_svc.add_object(svc);
  fill_string_group(svc.mutable_servicegroups(), "test_group");
  fill_pair_string_group(grp.mutable_members(), "test_host,test");
  aply_grp.add_object(grp);
  configuration::error_cnt err;
  _state_hlp->expand(err);
  aply_grp.resolve_object(grp, err);
  ASSERT_EQ(grp.members().data().size(), 1);

  configuration::Servicegroup grp1;
  configuration::servicegroup_helper grp1_hlp(&grp1);
  grp1.set_servicegroup_name("big_group");
  fill_string_group(grp1.mutable_servicegroup_members(), "test_group");
  aply_grp.add_object(grp1);
  _state_hlp->expand(err);
  auto found = std::find_if(pb_indexed_config.state().servicegroups().begin(),
                            pb_indexed_config.state().servicegroups().end(),
                            [](const configuration::Servicegroup& sg) {
                              return sg.servicegroup_name() == "big_group";
                            });
  ASSERT_TRUE(found != pb_indexed_config.state().servicegroups().end());
  ASSERT_EQ(found->members().data().size(), 1);

  ASSERT_EQ(engine::servicegroup::servicegroups.size(), 2u);
  aply_grp.remove_object("big_group");
  ASSERT_EQ(engine::servicegroup::servicegroups.size(), 1u);
}

// Given a servicegroup applier
// And a configuration servicegroup in configuration
// When we remove the configuration
// Then it is really removed
TEST_F(ApplierServicegroup, PbRemoveServiceFromGroup) {
  configuration::applier::host aply_hst;
  configuration::applier::service aply_svc;
  configuration::applier::command aply_cmd;
  configuration::applier::servicegroup aply_grp;
  configuration::Servicegroup grp;
  configuration::servicegroup_helper grp_hlp(&grp);
  grp.set_servicegroup_name("test_group");

  configuration::Command cmd;
  configuration::command_helper cmd_hlp(&cmd);
  cmd.set_command_name("cmd");
  cmd.set_command_line("echo 1");
  aply_cmd.add_object(cmd);

  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  aply_hst.add_object(hst);

  configuration::Service svc;
  configuration::service_helper svc_hlp(&svc);
  svc.set_service_description("test");
  svc_hlp.hook("service_description", "test");
  svc.set_host_name("test_host");
  svc.set_service_id(18);
  svc.set_check_command("cmd");
  // We fake here the expand_object on configuration::service
  svc.set_host_id(12);
  aply_svc.add_object(svc);
  svc_hlp.hook("servicegroups", "test_group");

  svc.set_service_description("test2");
  svc.set_host_name("test_host");
  svc.set_service_id(19);
  svc.set_check_command("cmd");
  // We fake here the expand_object on configuration::service
  svc.set_host_id(12);
  aply_svc.add_object(svc);
  svc_hlp.hook("servicegroups", "test_group");

  grp_hlp.hook("members", "test_host,test,test_host,test2");
  aply_grp.add_object(grp);
  configuration::error_cnt err;
  _state_hlp->expand(err);
  aply_grp.resolve_object(grp, err);
  ASSERT_EQ(grp.members().data().size(), 2);

  engine::servicegroup* sg =
      engine::servicegroup::servicegroups["test_group"].get();
  ASSERT_EQ(sg->members.size(), 2u);
  aply_svc.remove_object({12, 18});
  ASSERT_EQ(sg->members.size(), 1u);

  grp_hlp.hook("members", "test_host,test,test_host,test2");
  aply_grp.modify_object(pb_indexed_config.mut_state().mutable_servicegroups(0),
                         grp);

  ASSERT_EQ(engine::servicegroup::servicegroups.size(), 1u);
}
