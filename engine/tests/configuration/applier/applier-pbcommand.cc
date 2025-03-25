/*
 * Copyright 2017-2019,2023 Centreon (https://www.centreon.com/)
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
#include <com/centreon/engine/macros.hh>

#include "com/centreon/engine/commands/connector.hh"
#include "com/centreon/engine/configuration/applier/command.hh"
#include "com/centreon/engine/configuration/applier/connector.hh"
#include "com/centreon/engine/configuration/applier/contact.hh"
#include "com/centreon/engine/configuration/applier/host.hh"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;
using namespace com::centreon::engine::configuration::applier;

class ApplierPbCommand : public ::testing::Test {
 public:
  void SetUp() override { init_config_state(); }

  void TearDown() override { deinit_config_state(); }
};

// Given a command applier
// And a configuration command just with a name
// Then the applier add_object adds the command in the configuration set
// but not in the commands map (the command is unusable).
TEST_F(ApplierPbCommand, PbUnusableCommandFromConfig) {
  configuration::applier::command aply;
  configuration::Command cmd;
  configuration::command_helper cmd_hlp(&cmd);
  cmd.set_command_name("cmd");
  ASSERT_THROW(aply.add_object(cmd), std::exception);
  ASSERT_EQ(pb_indexed_config.state().commands().size(), 1u);
  ASSERT_EQ(commands::command::commands.size(), 0u);
}

// Given a command applier
// And a configuration command with a name and a command line
// Then the applier add_object adds the command into the configuration set
// and the commands map (accessible from commands::set::instance()).
TEST_F(ApplierPbCommand, PbNewCommandFromConfig) {
  configuration::applier::command aply;
  configuration::Command cmd;
  configuration::command_helper cmd_hlp(&cmd);
  cmd.set_command_name("cmd");
  cmd.set_command_line("echo 1");
  aply.add_object(cmd);
  ASSERT_EQ(pb_indexed_config.state().commands().size(), 1u);
  command_map::iterator found{commands::command::commands.find("cmd")};
  ASSERT_FALSE(found == commands::command::commands.end());
  ASSERT_FALSE(!found->second);
  ASSERT_EQ(found->second->get_name(), "cmd");
  ASSERT_EQ(found->second->get_command_line(), "echo 1");
}
// Given a command applier
// And a configuration command with a name, a command line and a connector
// Then the applier add_object adds the command into the configuration set
// but not in the commands map (the connector is not defined).
TEST_F(ApplierPbCommand, PbNewCommandWithEmptyConnectorFromConfig) {
  configuration::applier::command aply;
  configuration::Command cmd;
  configuration::command_helper cmd_hlp(&cmd);
  cmd.set_command_name("cmd");
  cmd.set_command_line("echo 1");
  cmd.set_connector("perl");
  ASSERT_THROW(aply.add_object(cmd), std::exception);
  ASSERT_EQ(pb_indexed_config.state().commands().size(), 1u);
  command_map::iterator found{commands::command::commands.find("cmd")};
  ASSERT_TRUE(found == commands::command::commands.end());
}

// Given a command applier
// And a configuration command with a name, a command line and a connector
// And the connector is well defined.
// Then the applier add_object adds the command into the configuration set
// but not in the commands map (the connector is not defined).
TEST_F(ApplierPbCommand, PbNewCommandWithConnectorFromConfig) {
  configuration::error_cnt err;
  configuration::applier::command aply;
  configuration::applier::connector cnn_aply;
  configuration::Command cmd;
  configuration::command_helper cmd_hlp(&cmd);
  cmd.set_command_name("cmd");
  cmd.set_command_line("echo 1");
  cmd.set_connector("perl");
  configuration::Connector cnn;
  configuration::connector_helper cnn_hlp(&cnn);
  cnn.set_connector_name("perl");

  cnn_aply.add_object(cnn);
  aply.add_object(cmd);

  ASSERT_EQ(pb_indexed_config.state().commands().size(), 1u);
  command_map::iterator found = commands::command::commands.find("cmd");
  ASSERT_EQ(found->second->get_name(), "cmd");
  ASSERT_EQ(found->second->get_command_line(), "echo 1");
  ASSERT_NO_THROW(aply.resolve_object(cmd, err));
}

// Given some command/connector appliers
// And a configuration command
// And a connector with the same name.
// Then the applier add_object adds the command into the configuration set
// but not in the commands map (the connector is not defined).
TEST_F(ApplierPbCommand, PbNewCommandAndConnectorWithSameName) {
  configuration::error_cnt err;
  configuration::applier::command aply;
  configuration::applier::connector cnn_aply;
  configuration::Command cmd;
  configuration::command_helper cmd_hlp(&cmd);
  cmd.set_command_name("cmd");
  cmd.set_command_line("echo 1");
  configuration::Connector cnn;
  configuration::connector_helper cnn_hlp(&cnn);
  cnn.set_connector_name("cmd");
  cnn.set_connector_line("echo 2");

  cnn_aply.add_object(cnn);
  aply.add_object(cmd);

  ASSERT_EQ(pb_indexed_config.state().commands().size(), 1u);
  command_map::iterator found{commands::command::commands.find("cmd")};
  ASSERT_FALSE(found == commands::command::commands.end());
  ASSERT_FALSE(!found->second);

  ASSERT_EQ(found->second->get_name(), "cmd");
  ASSERT_EQ(found->second->get_command_line(), "echo 1");

  aply.resolve_object(cmd, err);
  connector_map::iterator found_con{
      commands::connector::connectors.find("cmd")};
  ASSERT_TRUE(found_con != commands::connector::connectors.end());
  ASSERT_TRUE(found_con->second);

  found = commands::command::commands.find("cmd");
  ASSERT_TRUE(found != commands::command::commands.end());
}

// Given some command and connector appliers already applied with
// all objects created.
// When the command is changed from the configuration,
// Then the modify_object() method updated correctly the command.
TEST_F(ApplierPbCommand, PbModifyCommandWithConnector) {
  configuration::applier::command aply;
  configuration::applier::connector cnn_aply;
  configuration::Command cmd;
  configuration::command_helper cmd_hlp(&cmd);
  cmd.set_command_name("cmd");
  cmd.set_command_line("echo 1");
  cmd.set_connector("perl");
  configuration::Connector cnn;
  configuration::connector_helper cnn_hlp(&cnn);
  cnn.set_connector_name("perl");

  cnn_aply.add_object(cnn);
  aply.add_object(cmd);

  configuration::Command* to_modify =
      &pb_indexed_config.mut_state().mutable_commands()->at(0);
  cmd.set_command_line("date");
  aply.modify_object(to_modify, cmd);
  command_map::iterator found{commands::command::commands.find("cmd")};
  ASSERT_EQ(found->second->get_name(), "cmd");
  ASSERT_EQ(found->second->get_command_line(), "date");
}

// Given simple command (without connector) applier already applied with
// all objects created.
// When the command is removed from the configuration,
// Then the command is totally removed.
TEST_F(ApplierPbCommand, PbRemoveCommand) {
  configuration::applier::command aply;
  configuration::Command cmd;
  configuration::command_helper cmd_hlp(&cmd);
  cmd.set_command_name("cmd");
  cmd.set_command_line("echo 1");

  aply.add_object(cmd);

  aply.remove_object<std::string>({0, "cmd"});
  command_map::iterator found{commands::command::commands.find("cmd")};
  ASSERT_EQ(found, commands::command::commands.end());
  ASSERT_TRUE(pb_indexed_config.state().commands().size() == 0);
}

// Given some command and connector appliers already applied with
// all objects created.
// When the command is removed from the configuration,
// Then the command is totally removed.
TEST_F(ApplierPbCommand, PbRemoveCommandWithConnector) {
  configuration::applier::command aply;
  configuration::applier::connector cnn_aply;
  configuration::Command cmd;
  configuration::command_helper cmd_hlp(&cmd);
  cmd.set_command_name("cmd");
  cmd.set_command_line("echo 1");
  cmd.set_connector("perl");
  configuration::Connector cnn;
  configuration::connector_helper cnn_hlp(&cnn);
  cnn.set_connector_name("perl");

  cnn_aply.add_object(cnn);
  aply.add_object(cmd);

  aply.remove_object<std::string>({0, "cmd"});
  command_map::iterator found{commands::command::commands.find("cmd")};
  ASSERT_EQ(found, commands::command::commands.end());
  ASSERT_TRUE(pb_indexed_config.state().commands().size() == 0);
}

// Given simple command (without connector) applier already applied with
// all objects created.
// When the command is removed from the configuration,
// Then the command is totally removed.
TEST_F(ApplierPbCommand, PbComplexCommand) {
  configuration::error_cnt err;
  configuration::applier::command cmd_aply;
  configuration::applier::host hst_aply;

  configuration::Command cmd;
  configuration::command_helper cmd_hlp(&cmd);
  cmd.set_command_name("base_centreon_ping");
  cmd.set_command_line(
      "$USER1$/check_icmp -H $HOSTADDRESS$ -n $_HOSTPACKETNUMBER$ -w "
      "$_HOSTWARNING$ -c $_HOSTCRITICAL$");
  cmd_aply.add_object(cmd);

  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("hst_test");
  hst.set_address("127.0.0.1");
  hst.set_host_id(1);
  configuration::CustomVariable* cv = hst.add_customvariables();
  cv->set_name("PACKETNUMBER");
  cv->set_value("42");
  cv = hst.add_customvariables();
  cv->set_name("WARNING");
  cv->set_value("200,20%");
  cv = hst.add_customvariables();
  cv->set_name("CRITICAL");
  cv->set_value("400,50%");
  hst.set_check_command("base_centreon_ping");
  hst_aply.add_object(hst);

  command_map::iterator cmd_found{
      commands::command::commands.find("base_centreon_ping")};
  ASSERT_NE(cmd_found, commands::command::commands.end());
  ASSERT_TRUE(pb_indexed_config.state().commands().size() == 1);

  host_map::iterator hst_found{engine::host::hosts.find("hst_test")};
  ASSERT_NE(hst_found, engine::host::hosts.end());
  ASSERT_TRUE(pb_indexed_config.hosts().size() == 1);

  hst_aply.expand_objects(pb_indexed_config);
  hst_aply.resolve_object(hst, err);
  ASSERT_TRUE(hst_found->second->custom_variables.size() == 3);
  nagios_macros* macros(get_global_macros());
  grab_host_macros_r(macros, hst_found->second.get());
  std::string processed_cmd(
      hst_found->second->get_check_command_ptr()->process_cmd(macros));
  ASSERT_EQ(processed_cmd,
            "/check_icmp -H 127.0.0.1 -n 42 -w 200,20% -c 400,50%");
}

// Given simple command (without connector) applier already applied with
// all objects created.
// When the command is removed from the configuration,
// Then the command is totally removed.
TEST_F(ApplierPbCommand, PbComplexCommandWithContact) {
  configuration::error_cnt err;
  configuration::applier::command cmd_aply;
  configuration::applier::host hst_aply;
  configuration::applier::contact cnt_aply;

  configuration::Command cmd;
  configuration::command_helper cmd_hlp(&cmd);
  cmd.set_command_name("base_centreon_ping");
  cmd.set_command_line(
      "$USER1$/check_icmp -H $HOSTADDRESS$ -n $_HOSTPACKETNUMBER$ -w "
      "$_HOSTWARNING$ -c $_HOSTCRITICAL$ $CONTACTNAME$");
  cmd_aply.add_object(cmd);

  configuration::Contact cnt;
  configuration::contact_helper cnt_hlp(&cnt);
  cnt.set_contact_name("user");
  cnt.set_email("contact@centreon.com");
  cnt.set_pager("0473729383");
  cnt.set_host_notification_period("24x7");
  cnt.set_service_notification_period("24x7");
  cnt_aply.add_object(cnt);

  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("hst_test");
  hst.set_address("127.0.0.1");
  hst.set_host_id(1);
  auto* cv = hst.add_customvariables();
  cv->set_name("PACKETNUMBER");
  cv->set_value("42");
  cv = hst.add_customvariables();
  cv->set_name("WARNING");
  cv->set_value("200,20%");
  cv = hst.add_customvariables();
  cv->set_name("CRITICAL");
  cv->set_value("400,50%");
  hst.set_check_command("base_centreon_ping");
  fill_string_group(hst.mutable_contacts(), "user");
  hst_aply.add_object(hst);

  command_map::iterator cmd_found =
      commands::command::commands.find("base_centreon_ping");
  ASSERT_NE(cmd_found, commands::command::commands.end());
  ASSERT_TRUE(pb_indexed_config.state().commands().size() == 1);

  host_map::iterator hst_found = engine::host::hosts.find("hst_test");
  ASSERT_NE(hst_found, engine::host::hosts.end());
  ASSERT_TRUE(pb_indexed_config.hosts().size() == 1);

  hst_aply.expand_objects(pb_indexed_config);
  hst_aply.resolve_object(hst, err);
  ASSERT_TRUE(hst_found->second->custom_variables.size() == 3);
  nagios_macros* macros(get_global_macros());
  grab_host_macros_r(macros, hst_found->second.get());
  std::string processed_cmd(
      hst_found->second->get_check_command_ptr()->process_cmd(macros));
  ASSERT_EQ(processed_cmd,
            "/check_icmp -H 127.0.0.1 -n 42 -w 200,20% -c 400,50% user");
}
