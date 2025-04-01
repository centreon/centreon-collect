/**
 * Copyright 2017-2019,2023-2024 Centreon (https://www.centreon.com/)
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
#include "com/centreon/engine/commands/command.hh"
#include "com/centreon/engine/configuration/applier/command.hh"
#include "com/centreon/engine/configuration/applier/connector.hh"
#include "com/centreon/engine/configuration/applier/contact.hh"
#include "com/centreon/engine/configuration/applier/contactgroup.hh"
#include "com/centreon/engine/configuration/applier/timeperiod.hh"
#include "com/centreon/engine/globals.hh"
#include "common/engine_conf/message_helper.hh"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;
using namespace com::centreon::engine::configuration::applier;

class ApplierPbContact : public ::testing::Test {
 protected:
  std::unique_ptr<configuration::state_helper> _state_hlp;

 public:
  void SetUp() override { _state_hlp = init_config_state(); }

  void TearDown() override { deinit_config_state(); }

  configuration::Contact valid_pb_contact_config() const {
    // Add command.
    {
      configuration::Command cmd;
      configuration::command_helper cmd_hlp(&cmd);
      cmd.set_command_name("cmd");
      cmd.set_command_line("true");
      configuration::applier::command aplyr;
      aplyr.add_object(cmd);
    }
    // Add timeperiod.
    {
      configuration::Timeperiod tperiod;
      configuration::timeperiod_helper tp_help(&tperiod);
      tperiod.set_timeperiod_name("24x7");
      tperiod.set_alias("24x7");
      configuration::Timerange* tr = tperiod.mutable_timeranges()->add_monday();
      // monday: 00:00-24:00
      tr->set_range_start(0);
      tr->set_range_end(24 * 3600);
      configuration::applier::timeperiod aplyr;
      aplyr.add_object(tperiod);
    }
    // Valid contact configuration
    // (will generate 0 warnings or 0 errors).
    configuration::Contact ctct;
    ctct.set_contact_name("admin");
    ctct.set_host_notification_period("24x7");
    ctct.set_service_notification_period("24x7");
    fill_string_group(ctct.mutable_host_notification_commands(), "cmd");
    fill_string_group(ctct.mutable_service_notification_commands(), "cmd");
    return ctct;
  }
};

// Given a contact applier
// And a configuration contact
// When we modify the contact configuration with an unexisting contact
// Then an exception is thrown.
TEST_F(ApplierPbContact, PbModifyUnexistingContactFromConfig) {
  configuration::applier::contact aply;
  configuration::Contact ctct;
  configuration::contact_helper hlp(&ctct);
  ctct.set_contact_name("test");
  fill_string_group(ctct.mutable_contactgroups(), "test_group");
  fill_string_group(ctct.mutable_host_notification_commands(), "cmd1,cmd2");
  configuration::Contact* cfg = pb_indexed_config.mut_state().add_contacts();
  cfg->CopyFrom(ctct);
  ASSERT_THROW(aply.modify_object(cfg, ctct), std::exception);
}

// Given contactgroup / contact appliers
// And a configuration contactgroup and a configuration contact
// that are already in configuration
// When we remove the contact configuration applier
// Then it is really removed from the configuration applier.
TEST_F(ApplierPbContact, PbRemoveContactFromConfig) {
  configuration::applier::contact aply;
  configuration::applier::contactgroup aply_grp;

  configuration::Contactgroup grp;
  grp.set_contactgroup_name("test_group");

  configuration::Contact ctct;
  configuration::contact_helper c_helper(&ctct);
  ctct.set_contact_name("test");
  ctct.add_address("coucou");
  ctct.add_address("foo");
  ctct.add_address("bar");
  fill_string_group(ctct.mutable_contactgroups(), "test_group");
  fill_string_group(ctct.mutable_host_notification_commands(), "cmd1");
  fill_string_group(ctct.mutable_host_notification_commands(), "cmd2");
  fill_string_group(ctct.mutable_service_notification_commands(), "svc1");
  fill_string_group(ctct.mutable_service_notification_commands(), "svc2");
  configuration::CustomVariable* cv = ctct.add_customvariables();
  cv->set_name("superVar");
  cv->set_value("superValue");
  aply_grp.add_object(grp);
  aply.add_object(ctct);
  configuration::error_cnt err;
  _state_hlp->expand(err);
  engine::contact* my_contact = engine::contact::contacts.begin()->second.get();
  ASSERT_EQ(my_contact->get_addresses().size(), 3u);
  aply.remove_object("test");
  ASSERT_TRUE(engine::contact::contacts.empty());
}

TEST_F(ApplierPbContact, PbModifyContactFromConfig) {
  configuration::applier::contact aply;
  configuration::applier::contactgroup aply_grp;
  configuration::Contactgroup grp;
  configuration::contactgroup_helper grp_hlp(&grp);
  grp.set_contactgroup_name("test_group");
  configuration::Contact ctct;
  configuration::contact_helper ctct_hlp(&ctct);
  ctct.set_contact_name("test");
  fill_string_group(ctct.mutable_contactgroups(), "test_group");
  fill_string_group(ctct.mutable_host_notification_commands(), "cmd1,cmd2");
  fill_string_group(ctct.mutable_service_notification_commands(), "svc1,svc2");
  ASSERT_TRUE(ctct_hlp.insert_customvariable("_superVar", "SuperValue"));
  ASSERT_TRUE(ctct.customvariables().size() == 1);

  configuration::applier::command cmd_aply;
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
  cmd_aply.add_object(cmd);

  aply_grp.add_object(grp);
  aply.add_object(ctct);
  configuration::error_cnt err;
  _state_hlp->expand(err);
  ctct_hlp.hook("host_notification_commands", "cmd");
  ctct_hlp.hook("service_notification_commands", "svc1,svc2");
  ASSERT_TRUE(ctct_hlp.insert_customvariable("_superVar", "Super"));
  ASSERT_TRUE(ctct_hlp.insert_customvariable("_superVar1", "Super1"));
  ctct.set_alias("newAlias");
  ASSERT_EQ(ctct.customvariables().size(), 2u);
  ctct_hlp.hook("service_notification_options", "n");
  aply.modify_object(pb_indexed_config.mut_contacts().begin()->second.get(),
                     ctct);
  contact_map::const_iterator ct_it{engine::contact::contacts.find("test")};
  ASSERT_TRUE(ct_it != engine::contact::contacts.end());
  ASSERT_EQ(ct_it->second->get_custom_variables().size(), 2u);
  ASSERT_EQ(ct_it->second->get_custom_variables()["superVar"].value(),
            std::string_view("Super"));
  ASSERT_EQ(ct_it->second->get_custom_variables()["superVar1"].value(),
            std::string_view("Super1"));
  ASSERT_EQ(ct_it->second->get_alias(), std::string_view("newAlias"));
  ASSERT_FALSE(ct_it->second->notify_on(notifier::service_notification,
                                        notifier::unknown));

  auto found = pb_indexed_config.mut_commands().contains("cmd");
  pb_indexed_config.mut_commands().erase("cmd");
  ASSERT_TRUE(found)
      << "Command 'cmd' not found among the configuration commands";

  cmd.set_command_name("cmd");
  cmd.set_command_line("bar");
  configuration::applier::command aplyr;
  aplyr.add_object(cmd);
  ctct_hlp.hook("host_notification_commands", "cmd");
  auto* old_ct = pb_indexed_config.mut_contacts().at("test").get();
  aply.modify_object(old_ct, ctct);
  {
    command_map::iterator found{commands::command::commands.find("cmd")};
    ASSERT_TRUE(found != commands::command::commands.end());
    ASSERT_TRUE(found->second);
    ASSERT_TRUE(found->second->get_command_line() == "bar");
  }
}

// Given contactgroup / contact appliers
// And a configuration contactgroup and a configuration contact
// that are already in configuration
// When we resolve the contact configuration
// Then the contact contactgroups is cleared, nothing more if the
// contact check is OK. Here, since notification commands are empty,
// an exception is thrown.
TEST_F(ApplierPbContact, PbResolveContactFromConfig) {
  configuration::error_cnt err;
  configuration::applier::contact aply;
  configuration::applier::contactgroup aply_grp;
  configuration::Contactgroup grp;
  configuration::contactgroup_helper cg_hlp(&grp);
  grp.set_contactgroup_name("test_group");

  configuration::Contact ctct;
  configuration::contact_helper ct_hlp(&ctct);
  ctct.set_contact_name("test");
  fill_string_group(ctct.mutable_contactgroups(), "test_group");
  fill_string_group(ctct.mutable_host_notification_commands(), "cmd1");
  fill_string_group(ctct.mutable_host_notification_commands(), "cmd2");
  aply_grp.add_object(grp);
  aply.add_object(ctct);
  _state_hlp->expand(err);
  ASSERT_THROW(aply.resolve_object(ctct, err), std::exception);
}

// Given a contact
// And an applier
// When the contact is resolved by the applier
// Then an exception is thrown
// And 2 warnings and 2 errors are returned:
//  * error 1 => no service notification command
//  * error 2 => no host notification command
//  * warning 1 => no service notification period
//  * warning 2 => no host notification period
TEST_F(ApplierPbContact, PbResolveContactNoNotification) {
  configuration::error_cnt err;
  configuration::applier::contact aply;
  configuration::Contact ctct;
  configuration::contact_helper ctct_hlp(&ctct);
  ctct.set_contact_name("test");
  aply.add_object(ctct);
  _state_hlp->expand(err);
  ASSERT_THROW(aply.resolve_object(ctct, err), std::exception);
  ASSERT_EQ(err.config_warnings, 2);
  ASSERT_EQ(err.config_errors, 2);
}

// Given a valid contact
//   - valid host notification period
//   - valid service notification period
//   - valid host notification command
//   - valid service notification command
// And an applier
// When resolve_object() is called
// Then no exception is thrown
// And no errors are returned
// And links are properly resolved
TEST_F(ApplierPbContact, PbResolveValidContact) {
  configuration::error_cnt err;
  configuration::applier::contact aply;
  configuration::Contact ctct(valid_pb_contact_config());
  aply.add_object(ctct);
  _state_hlp->expand(err);
  ASSERT_NO_THROW(aply.resolve_object(ctct, err));
  ASSERT_EQ(err.config_warnings, 0);
  ASSERT_EQ(err.config_errors, 0);
}

// Given a valid contact
// And an applier
// When adding a non-existing service notification period to the contact
// Then the resolve method throws
// And returns 1 error
TEST_F(ApplierPbContact, PbResolveNonExistingServiceNotificationTimeperiod) {
  configuration::error_cnt err;
  configuration::applier::contact aply;
  configuration::Contact ctct(valid_pb_contact_config());
  ctct.set_service_notification_period("non_existing_period");
  aply.add_object(ctct);
  _state_hlp->expand(err);
  ASSERT_THROW(aply.resolve_object(ctct, err), std::exception);
  ASSERT_EQ(err.config_warnings, 0);
  ASSERT_EQ(err.config_errors, 1);
}

// Given a valid contact
// And an applier
// When adding a non-existing host notification period to the contact
// Then the resolve method throws
// And returns 1 error
TEST_F(ApplierPbContact, PbResolveNonExistingHostNotificationTimeperiod) {
  configuration::error_cnt err;
  configuration::applier::contact aply;
  configuration::Contact ctct(valid_pb_contact_config());
  ctct.set_host_notification_period("non_existing_period");
  aply.add_object(ctct);
  _state_hlp->expand(err);
  ASSERT_THROW(aply.resolve_object(ctct, err), std::exception);
  ASSERT_EQ(err.config_warnings, 0);
  ASSERT_EQ(err.config_errors, 1);
}

// Given a valid contact
// And an applier
// When adding a non-existing service command to the contact
// Then the resolve method throws
// And returns 1 error
TEST_F(ApplierPbContact, PbResolveNonExistingServiceCommand) {
  configuration::error_cnt err;
  configuration::applier::contact aply;
  configuration::Contact ctct(valid_pb_contact_config());
  fill_string_group(ctct.mutable_service_notification_commands(),
                    "non_existing_command");
  aply.add_object(ctct);
  _state_hlp->expand(err);
  ASSERT_THROW(aply.resolve_object(ctct, err), std::exception);
  ASSERT_EQ(err.config_warnings, 0);
  ASSERT_EQ(err.config_errors, 1);
}

// Given a valid contact
// And an applier
// When adding a non-existing host command to the contact
// Then the resolve method throws
// And returns 1 error
TEST_F(ApplierPbContact, PbResolveNonExistingHostCommand) {
  configuration::error_cnt err;
  configuration::applier::contact aply;
  configuration::Contact ctct(valid_pb_contact_config());
  fill_string_group(ctct.mutable_host_notification_commands(),
                    "non_existing_command");
  aply.add_object(ctct);
  _state_hlp->expand(err);
  ASSERT_THROW(aply.resolve_object(ctct, err), std::exception);
  ASSERT_EQ(err.config_warnings, 0);
  ASSERT_EQ(err.config_errors, 1);
}

// Given a valid contact
// And the contact is notified on host recovery
// But not on down or unreachable host
// When resolve_object() is called
// Then a warning is returned
TEST_F(ApplierPbContact, PbContactWithOnlyHostRecoveryNotification) {
  configuration::error_cnt err;
  configuration::applier::contact aply;
  configuration::Contact ctct(valid_pb_contact_config());
  uint16_t options;
  fill_host_notification_options(&options, "r");
  ctct.set_host_notification_options(options);
  fill_service_notification_options(&options, "n");
  ctct.set_service_notification_options(options);
  ctct.set_host_notifications_enabled("1");
  ctct.set_service_notifications_enabled("1");
  aply.add_object(ctct);
  _state_hlp->expand(err);
  aply.resolve_object(ctct, err);
  ASSERT_EQ(err.config_warnings, 1);
  ASSERT_EQ(err.config_errors, 0);
}

// Given a valid contact
// And the contact is notified on service recovery
// But not on critical, warning or unknown service
// When resolve_object() is called
// Then a warning is returned
TEST_F(ApplierPbContact, PbContactWithOnlyServiceRecoveryNotification) {
  configuration::error_cnt err;
  configuration::applier::contact aply;
  configuration::Contact ctct(valid_pb_contact_config());
  uint16_t options;
  fill_host_notification_options(&options, "n");
  ctct.set_host_notification_options(options);
  fill_service_notification_options(&options, "r");
  ctct.set_service_notification_options(options);
  ctct.set_host_notifications_enabled(true);
  ctct.set_service_notifications_enabled(true);
  aply.add_object(ctct);
  _state_hlp->expand(err);
  aply.resolve_object(ctct, err);
  ASSERT_EQ(err.config_warnings, 1);
  ASSERT_EQ(err.config_errors, 0);
}
