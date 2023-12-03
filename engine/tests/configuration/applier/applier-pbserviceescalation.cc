/*
 * Copyright 2019 Centreon (https://www.centreon.com/)
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
#include <com/centreon/engine/configuration/applier/command.hh>
#include <com/centreon/engine/configuration/applier/host.hh>
#include <com/centreon/engine/configuration/applier/service.hh>
#include <com/centreon/engine/configuration/applier/serviceescalation.hh>
#include <com/centreon/engine/host.hh>
#include <com/centreon/engine/service.hh>
#include <com/centreon/engine/serviceescalation.hh>
#include "common/configuration/command_helper.hh"
#include "common/configuration/host_helper.hh"
#include "common/configuration/service_helper.hh"
#include "common/configuration/serviceescalation_helper.hh"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;

class ApplierServiceEscalation : public ::testing::Test {
 public:
  void SetUp() override { init_config_state(LEGACY); }

  void TearDown() override { deinit_config_state(); }
};

TEST_F(ApplierServiceEscalation, AddEscalation) {
  configuration::applier::host hst_aply;
  configuration::host hst;
  ASSERT_TRUE(hst.parse("host_name", "test_host"));
  ASSERT_TRUE(hst.parse("address", "127.0.0.1"));
  ASSERT_TRUE(hst.parse("_HOST_ID", "12"));
  hst_aply.add_object(hst);
  ASSERT_EQ(host::hosts.size(), 1u);

  configuration::applier::command cmd_aply;
  configuration::command cmd;
  cmd.parse("command_name", "cmd");
  cmd.parse("command_line", "echo 1");
  cmd_aply.add_object(cmd);

  configuration::applier::service svc_aply;
  configuration::service svc;
  ASSERT_TRUE(svc.parse("host", "test_host"));
  ASSERT_TRUE(svc.parse("service_description", "test_svc"));
  ASSERT_TRUE(svc.parse("service_id", "12"));
  ASSERT_TRUE(svc.parse("check_command", "cmd"));
  svc.set_host_id(12);
  svc_aply.add_object(svc);
  ASSERT_EQ(service::services.size(), 1u);

  configuration::applier::serviceescalation se_apply;
  configuration::serviceescalation se;
  ASSERT_TRUE(se.parse("host", "test_host"));
  ASSERT_TRUE(se.parse("service_description", "test_svc"));
  ASSERT_TRUE(se.parse("first_notification", "4"));
  se_apply.add_object(se);
  ASSERT_EQ(serviceescalation::serviceescalations.size(), 1u);
  ASSERT_TRUE(se.parse("first_notification", "8"));
  se_apply.add_object(se);
  ASSERT_EQ(serviceescalation::serviceescalations.size(), 2u);
}

TEST_F(ApplierServiceEscalation, PbAddEscalation) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst_aply.add_object(hst);
  ASSERT_EQ(host::hosts.size(), 1u);

  configuration::applier::command cmd_aply;
  configuration::Command cmd;
  configuration::command_helper cmd_hlp(&cmd);
  cmd.set_command_name("cmd");
  cmd.set_command_line("echo 1");
  cmd_aply.add_object(cmd);

  configuration::applier::service svc_aply;
  configuration::Service svc;
  configuration::service_helper svc_hlp(&svc);
  svc.set_host_name("test_host");
  svc.set_service_description("test_svc");
  svc.set_service_id(12);
  svc.set_check_command("cmd");
  svc.set_host_id(12);
  svc_aply.add_object(svc);
  ASSERT_EQ(service::services.size(), 1u);

  configuration::applier::serviceescalation se_apply;
  configuration::Serviceescalation se;
  configuration::serviceescalation_helper se_hlp(&se);
  se_hlp.hook("hosts", "test_host");
  se_hlp.hook("service_description", "test_svc");
  se.set_first_notification(4);
  se_apply.add_object(se);
  ASSERT_EQ(serviceescalation::serviceescalations.size(), 1u);
  se.set_first_notification(8);
  se_apply.add_object(se);
  ASSERT_EQ(serviceescalation::serviceescalations.size(), 2u);
}

TEST_F(ApplierServiceEscalation, RemoveEscalation) {
  configuration::applier::host hst_aply;
  configuration::host hst;
  ASSERT_TRUE(hst.parse("host_name", "test_host"));
  ASSERT_TRUE(hst.parse("address", "127.0.0.1"));
  ASSERT_TRUE(hst.parse("_HOST_ID", "12"));
  hst_aply.add_object(hst);
  ASSERT_EQ(host::hosts.size(), 1u);

  configuration::applier::command cmd_aply;
  configuration::command cmd;
  cmd.parse("command_name", "cmd");
  cmd.parse("command_line", "echo 1");
  cmd_aply.add_object(cmd);

  configuration::applier::service svc_aply;
  configuration::service svc;
  ASSERT_TRUE(svc.parse("host", "test_host"));
  ASSERT_TRUE(svc.parse("service_description", "test_svc"));
  ASSERT_TRUE(svc.parse("service_id", "12"));
  ASSERT_TRUE(svc.parse("check_command", "cmd"));
  svc.set_host_id(12);
  svc_aply.add_object(svc);
  ASSERT_EQ(service::services.size(), 1u);

  configuration::applier::serviceescalation se_apply;
  configuration::serviceescalation se1, se2;
  ASSERT_TRUE(se1.parse("host_name", "test_host"));
  ASSERT_TRUE(se1.parse("service_description", "test_svc"));
  ASSERT_TRUE(se1.parse("first_notification", "4"));
  se_apply.add_object(se1);
  ASSERT_TRUE(se2.parse("host_name", "test_host"));
  ASSERT_TRUE(se2.parse("service_description", "test_svc"));
  ASSERT_TRUE(se2.parse("first_notification", "8"));
  ASSERT_TRUE(se2.parse("first_notification", "8"));
  se_apply.add_object(se2);
  ASSERT_EQ(serviceescalation::serviceescalations.size(), 2u);

  se_apply.remove_object(se1);
  ASSERT_EQ(serviceescalation::serviceescalations.size(), 1u);
  se_apply.remove_object(se2);
  ASSERT_EQ(serviceescalation::serviceescalations.size(), 0u);
}

TEST_F(ApplierServiceEscalation, PbRemoveEscalation) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst_aply.add_object(hst);
  ASSERT_EQ(host::hosts.size(), 1u);

  configuration::applier::command cmd_aply;
  configuration::Command cmd;
  configuration::command_helper cmd_hlp(&cmd);
  cmd.set_command_name("cmd");
  cmd.set_command_line("echo 1");
  cmd_aply.add_object(cmd);

  configuration::applier::service svc_aply;
  configuration::Service svc;
  configuration::service_helper svc_hlp(&svc);
  svc.set_host_name("test_host");
  svc.set_service_description("test_svc");
  svc.set_service_id(12);
  svc.set_check_command("cmd");
  svc.set_host_id(12);
  svc_aply.add_object(svc);
  ASSERT_EQ(service::services.size(), 1u);

  configuration::applier::serviceescalation se_apply;
  configuration::Serviceescalation se1, se2;
  configuration::serviceescalation_helper se1_hlp(&se1), se2_hlp(&se2);
  se1_hlp.hook("hosts", "test_host");
  se1_hlp.hook("service_description", "test_svc");
  se1.set_first_notification(4);
  se_apply.add_object(se1);
  se2_hlp.hook("hosts", "test_host");
  se2_hlp.hook("service_description", "test_svc");
  se2.set_first_notification(8);
  se_apply.add_object(se2);
  ASSERT_EQ(serviceescalation::serviceescalations.size(), 2u);

  se_apply.remove_object(0);
  ASSERT_EQ(serviceescalation::serviceescalations.size(), 1u);
  se_apply.remove_object(0);
  ASSERT_EQ(serviceescalation::serviceescalations.size(), 0u);
}

TEST_F(ApplierServiceEscalation, RemoveEscalationFromRemovedService) {
  configuration::applier::host hst_aply;
  configuration::host hst;
  ASSERT_TRUE(hst.parse("host_name", "test_host"));
  ASSERT_TRUE(hst.parse("address", "127.0.0.1"));
  ASSERT_TRUE(hst.parse("_HOST_ID", "12"));
  hst_aply.add_object(hst);
  ASSERT_EQ(host::hosts.size(), 1u);

  configuration::applier::command cmd_aply;
  configuration::command cmd;
  cmd.parse("command_name", "cmd");
  cmd.parse("command_line", "echo 1");
  cmd_aply.add_object(cmd);

  configuration::applier::service svc_aply;
  configuration::service svc;
  ASSERT_TRUE(svc.parse("host", "test_host"));
  ASSERT_TRUE(svc.parse("service_description", "test_svc"));
  ASSERT_TRUE(svc.parse("service_id", "12"));
  ASSERT_TRUE(svc.parse("check_command", "cmd"));
  svc.set_host_id(12);
  svc_aply.add_object(svc);
  ASSERT_EQ(service::services.size(), 1u);

  configuration::applier::serviceescalation se_apply;
  configuration::serviceescalation se;
  ASSERT_TRUE(se.parse("host_name", "test_host"));
  ASSERT_TRUE(se.parse("service_description", "test_svc"));
  ASSERT_TRUE(se.parse("first_notification", "4"));
  se_apply.add_object(se);
  ASSERT_EQ(serviceescalation::serviceescalations.size(), 1u);
  ASSERT_TRUE(se.parse("first_notification", "8"));
  se_apply.add_object(se);
  ASSERT_EQ(serviceescalation::serviceescalations.size(), 2u);

  hst_aply.remove_object(hst);
  ASSERT_EQ(host::hosts.size(), 0u);
  svc_aply.remove_object(svc);
  ASSERT_EQ(service::services.size(), 0u);

  se_apply.remove_object(se);
  ASSERT_EQ(serviceescalation::serviceescalations.size(), 1u);
  ASSERT_TRUE(se.parse("first_notification", "4"));
  se_apply.remove_object(se);
  ASSERT_EQ(serviceescalation::serviceescalations.size(), 0u);
}

TEST_F(ApplierServiceEscalation, PbRemoveEscalationFromRemovedService) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst_aply.add_object(hst);
  ASSERT_EQ(host::hosts.size(), 1u);

  configuration::applier::command cmd_aply;
  configuration::Command cmd;
  configuration::command_helper cmd_hlp(&cmd);
  cmd.set_command_name("cmd");
  cmd.set_command_line("echo 1");
  cmd_aply.add_object(cmd);

  configuration::applier::service svc_aply;
  configuration::Service svc;
  configuration::service_helper svc_hlp(&svc);
  svc.set_host_name("test_host");
  svc.set_service_description("test_svc");
  svc.set_service_id(12);
  svc.set_check_command("cmd");
  svc.set_host_id(12);
  svc_aply.add_object(svc);
  ASSERT_EQ(service::services.size(), 1u);

  configuration::applier::serviceescalation se_apply;
  configuration::Serviceescalation se1, se2;
  configuration::serviceescalation_helper se1_hlp(&se1), se2_hlp(&se2);
  se1_hlp.hook("hosts", "test_host");
  se1_hlp.hook("service_description", "test_svc");
  se1.set_first_notification(4);
  se_apply.add_object(se1);
  se2_hlp.hook("hosts", "test_host");
  se2_hlp.hook("service_description", "test_svc");
  se2.set_first_notification(8);
  se_apply.add_object(se2);
  ASSERT_EQ(serviceescalation::serviceescalations.size(), 2u);

  hst_aply.remove_object(0);
  ASSERT_EQ(host::hosts.size(), 0u);
  svc_aply.remove_object(0);
  ASSERT_EQ(service::services.size(), 0u);

  se_apply.remove_object(0);
  ASSERT_EQ(serviceescalation::serviceescalations.size(), 1u);
  se_apply.remove_object(0);
  ASSERT_EQ(serviceescalation::serviceescalations.size(), 0u);
}
