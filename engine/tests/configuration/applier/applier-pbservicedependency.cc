/*
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
#include <com/centreon/engine/configuration/applier/command.hh>
#include <com/centreon/engine/configuration/applier/host.hh>
#include <com/centreon/engine/configuration/applier/service.hh>
#include <com/centreon/engine/configuration/applier/servicedependency.hh>
#include <com/centreon/engine/host.hh>
#include <com/centreon/engine/service.hh>
#include <com/centreon/engine/servicedependency.hh>
#include "common/configuration/command_helper.hh"
#include "common/configuration/host_helper.hh"
#include "common/configuration/service_helper.hh"
#include "common/configuration/servicedependency_helper.hh"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;

class ApplierServiceDependency : public ::testing::Test {
 public:
  void SetUp() override { init_config_state(LEGACY); }

  void TearDown() override { deinit_config_state(); }
};

TEST_F(ApplierServiceDependency, AddDependencyWithoutExpansion) {
  configuration::applier::host hst_aply;
  configuration::host hst;
  ASSERT_TRUE(hst.parse("host_name", "test_host1"));
  ASSERT_TRUE(hst.parse("address", "127.0.0.1"));
  ASSERT_TRUE(hst.parse("_HOST_ID", "12"));
  hst_aply.add_object(hst);

  ASSERT_TRUE(hst.parse("host_name", "test_host2"));
  ASSERT_TRUE(hst.parse("address", "127.0.0.2"));
  ASSERT_TRUE(hst.parse("_HOST_ID", "13"));
  hst_aply.add_object(hst);
  ASSERT_EQ(host::hosts.size(), 2u);

  configuration::applier::command cmd_aply;
  configuration::command cmd;
  cmd.parse("command_name", "cmd");
  cmd.parse("command_line", "echo 1");
  cmd_aply.add_object(cmd);

  configuration::applier::service svc_aply;
  configuration::service svc;
  ASSERT_TRUE(svc.parse("host", "test_host1"));
  ASSERT_TRUE(svc.parse("service_description", "test_svc1"));
  ASSERT_TRUE(svc.parse("service_id", "12"));
  ASSERT_TRUE(svc.parse("check_command", "cmd"));
  svc.set_host_id(12);
  svc_aply.add_object(svc);

  ASSERT_TRUE(svc.parse("host", "test_host2"));
  ASSERT_TRUE(svc.parse("service_description", "test_svc2"));
  ASSERT_TRUE(svc.parse("service_id", "13"));
  svc.set_host_id(13);
  svc_aply.add_object(svc);
  ASSERT_EQ(service::services.size(), 2u);

  configuration::applier::servicedependency sd_apply;
  configuration::servicedependency sd;
  ASSERT_TRUE(sd.parse("host", "test_host1"));
  ASSERT_TRUE(sd.parse("dependent_host", "test_host2"));
  ASSERT_TRUE(sd.parse("service_description", "test_svc1"));
  ASSERT_TRUE(sd.parse("dependent_service_description", "test_svc2"));
  /* the dependency type is missing because configuration is not
   * expanded. */
  ASSERT_THROW(sd_apply.add_object(sd), std::exception);
}

TEST_F(ApplierServiceDependency, PbAddDependencyWithoutExpansion) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host1");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst_aply.add_object(hst);

  hst.set_host_name("test_host2");
  hst.set_address("127.0.0.2");
  hst.set_host_id(13);
  hst_aply.add_object(hst);
  ASSERT_EQ(host::hosts.size(), 2u);

  configuration::applier::command cmd_aply;
  configuration::Command cmd;
  configuration::command_helper cmd_hlp(&cmd);
  cmd.set_command_name("cmd");
  cmd.set_command_line("echo 1");
  cmd_aply.add_object(cmd);

  configuration::applier::service svc_aply;
  configuration::Service svc;
  configuration::service_helper svc_hlp(&svc);
  svc.set_host_name("test_host1");
  svc.set_service_description("test_svc1");
  svc.set_service_id(12);
  svc.set_check_command("cmd");
  svc.set_host_id(12);
  svc_aply.add_object(svc);

  svc.set_host_name("test_host2");
  svc.set_service_description("test_svc2");
  svc.set_service_id(13);
  svc.set_host_id(13);
  svc_aply.add_object(svc);
  ASSERT_EQ(service::services.size(), 2u);

  configuration::applier::servicedependency sd_apply;
  configuration::Servicedependency sd;
  configuration::servicedependency_helper sd_hlp(&sd);
  sd_hlp.hook("hosts", "test_host1");
  sd_hlp.hook("dependent_hosts", "test_host2");
  sd_hlp.hook("service_description", "test_svc1");
  sd_hlp.hook("dependent_service_description", "test_svc2");
  /* the dependency type is missing because configuration is not
   * expanded. */
  ASSERT_THROW(sd_apply.add_object(sd), std::exception);
}

TEST_F(ApplierServiceDependency, AddDependency) {
  configuration::applier::host hst_aply;
  configuration::host hst;
  ASSERT_TRUE(hst.parse("host_name", "test_host1"));
  ASSERT_TRUE(hst.parse("address", "127.0.0.1"));
  ASSERT_TRUE(hst.parse("_HOST_ID", "12"));
  hst_aply.add_object(hst);

  ASSERT_TRUE(hst.parse("host_name", "test_host2"));
  ASSERT_TRUE(hst.parse("address", "127.0.0.2"));
  ASSERT_TRUE(hst.parse("_HOST_ID", "13"));
  hst_aply.add_object(hst);
  ASSERT_EQ(host::hosts.size(), 2u);

  configuration::applier::command cmd_aply;
  configuration::command cmd;
  cmd.parse("command_name", "cmd");
  cmd.parse("command_line", "echo 1");
  cmd_aply.add_object(cmd);

  configuration::applier::service svc_aply;
  configuration::service svc;
  ASSERT_TRUE(svc.parse("host", "test_host1"));
  ASSERT_TRUE(svc.parse("service_description", "test_svc1"));
  ASSERT_TRUE(svc.parse("service_id", "12"));
  ASSERT_TRUE(svc.parse("check_command", "cmd"));
  svc.set_host_id(12);
  svc_aply.add_object(svc);

  ASSERT_TRUE(svc.parse("host", "test_host2"));
  ASSERT_TRUE(svc.parse("service_description", "test_svc2"));
  ASSERT_TRUE(svc.parse("service_id", "13"));
  svc.set_host_id(13);
  svc_aply.add_object(svc);
  ASSERT_EQ(service::services.size(), 2u);

  configuration::applier::servicedependency sd_apply;
  configuration::servicedependency sd;
  ASSERT_TRUE(sd.parse("host", "test_host1"));
  ASSERT_TRUE(sd.parse("dependent_host", "test_host2"));
  ASSERT_TRUE(sd.parse("service_description", "test_svc1"));
  ASSERT_TRUE(sd.parse("dependent_service_description", "test_svc2"));

  /* Just to simulate the expansion */
  sd.dependency_type(configuration::servicedependency::execution_dependency);

  ASSERT_NO_THROW(sd_apply.add_object(sd));
}

TEST_F(ApplierServiceDependency, PbAddDependency) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host1");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst_aply.add_object(hst);

  hst.set_host_name("test_host2");
  hst.set_address("127.0.0.2");
  hst.set_host_id(13);
  hst_aply.add_object(hst);
  ASSERT_EQ(host::hosts.size(), 2u);

  configuration::applier::command cmd_aply;
  configuration::Command cmd;
  cmd.set_command_name("cmd");
  cmd.set_command_line("echo 1");
  cmd_aply.add_object(cmd);

  configuration::applier::service svc_aply;
  configuration::Service svc;
  configuration::service_helper svc_hlp(&svc);
  svc.set_host_name("test_host1");
  svc.set_service_description("test_svc1");
  svc.set_service_id(12);
  svc.set_check_command("cmd");
  svc.set_host_id(12);
  svc_aply.add_object(svc);

  svc.set_host_name("test_host2");
  svc.set_service_description("test_svc2");
  svc.set_service_id(13);
  svc.set_host_id(13);
  svc_aply.add_object(svc);
  ASSERT_EQ(service::services.size(), 2u);

  configuration::applier::servicedependency sd_apply;
  configuration::Servicedependency sd;
  configuration::servicedependency_helper sd_hlp(&sd);
  sd_hlp.hook("hosts", "test_host1");
  sd_hlp.hook("dependent_hosts", "test_host2");
  sd_hlp.hook("service_description", "test_svc1");
  sd_hlp.hook("dependent_service_description", "test_svc2");
  /* Just to simulate the expansion */
  sd.set_dependency_type(configuration::DependencyKind::execution_dependency);
  ASSERT_NO_THROW(sd_apply.add_object(sd));
}
