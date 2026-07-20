/**
 * Copyright 2005 - 2024 Centreon (https://www.centreon.com/)
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
#include "../timeperiod/utils.hh"
#include "com/centreon/engine/commands/command.hh"
#include "com/centreon/engine/commands/commands.hh"
#include "com/centreon/engine/configuration/applier/command.hh"
#include "com/centreon/engine/configuration/applier/contact.hh"
#include "com/centreon/engine/configuration/applier/host.hh"
#include "com/centreon/engine/macros.hh"
#include "com/centreon/engine/macros/grab_host.hh"
#include "common/engine_conf/command_helper.hh"
#include "common/engine_conf/contact_helper.hh"
#include "common/engine_conf/host_helper.hh"
#include "common/engine_conf/message_helper.hh"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;

class PbCustomVar : public ::testing::Test {
 protected:
  std::unique_ptr<configuration::state_helper> _state_hlp;

 public:
  void SetUp() override { _state_hlp = init_config_state(); }

  void TearDown() override { deinit_config_state(); }
};

// Given simple command (without connector) applier already applied with
// all objects created.
// When the command is removed from the configuration,
// Then the command is totally removed.
TEST_F(PbCustomVar, UpdateHostCustomVar) {
  configuration::applier::command cmd_aply;
  configuration::applier::host hst_aply;
  configuration::applier::contact cnt_aply;

  configuration::Command cmd;
  configuration::command_helper cmd_hlp(&cmd);
  cmd.set_command_name("base_centreon_ping");
  cmd.set_command_line(
      "$USER1$/check_icmp -H $HOSTADDRESS$ -n $_HOSTPACKETNUMBER$ -w "
      "$_HOSTWARNING$ -c $_HOSTCRITICAL$ $CONTACTNAME$");

  configuration::Contact cnt;
  configuration::contact_helper cnt_hlp(&cnt);
  cnt.set_contact_name("user");
  cnt.set_email("contact@centreon.com");
  cnt.set_pager("0473729383");
  cnt.set_host_notification_period("24x7");
  cnt.set_service_notification_period("24x7");

  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("hst_test");
  hst.set_address("127.0.0.1");
  hst.set_host_id(1);
  hst_hlp.insert_customvariable("_PACKETNUMBER", "42");
  hst_hlp.insert_customvariable("_WARNING", "200,20%");
  hst_hlp.insert_customvariable("_CRITICAL", "400,50%");
  hst.set_check_command("base_centreon_ping");
  hst.mutable_contacts()->add_data("user");
  configuration::error_cnt err;
  _state_hlp->expand(err);
  cmd_aply.add_object(cmd);
  cnt_aply.add_object(cnt);
  hst_aply.add_object(hst);

  command_map::iterator cmd_found{
      commands::command::commands.find("base_centreon_ping")};
  ASSERT_NE(cmd_found, commands::command::commands.end());
  ASSERT_TRUE(pb_indexed_config.commands().size() == 1);

  host_map::iterator hst_found{engine::host::hosts.find("hst_test")};
  ASSERT_NE(hst_found, engine::host::hosts.end());
  ASSERT_TRUE(pb_indexed_config.hosts().size() == 1);

  hst_aply.resolve_object(hst, err);
  ASSERT_EQ(hst_found->second->custom_variables.size(), 3);
  nagios_macros* macros(get_global_macros());
  grab_host_macros_r(macros, hst_found->second.get());
  std::string processed_cmd(
      hst_found->second->get_check_command_ptr()->process_cmd(macros));
  ASSERT_EQ(processed_cmd,
            "/check_icmp -H 127.0.0.1 -n 42 -w 200,20% -c 400,50% user");

  char* msg = strdupa("hst_test;PACKETNUMBER;44");
  cmd_change_object_custom_var(CMD_CHANGE_CUSTOM_HOST_VAR, msg);
  grab_host_macros_r(macros, hst_found->second.get());
  std::string processed_cmd2(
      hst_found->second->get_check_command_ptr()->process_cmd(macros));
  ASSERT_EQ(processed_cmd2,
            "/check_icmp -H 127.0.0.1 -n 44 -w 200,20% -c 400,50% user");
}
