/**
 * Copyright 2005 - 2019 Centreon (https://www.centreon.com/)
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
#include "com/centreon/engine/checks/checker.hh"
#include "com/centreon/engine/commands/commands.hh"
#include "com/centreon/engine/configuration/applier/command.hh"
#include "com/centreon/engine/configuration/applier/host.hh"
#include "com/centreon/engine/configuration/applier/service.hh"
#include "com/centreon/engine/events/loop.hh"
#include "com/centreon/process_manager.hh"
#include "common/engine_conf/service_helper.hh"
#include "common/tests/timeperiods/utils.hh"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::common::timeperiods;

class ServiceExternalCommand : public ::testing::Test {
 protected:
  std::unique_ptr<configuration::state_helper> _state_hlp;

 public:
  void SetUp() override { _state_hlp = init_config_state(); }

  void TearDown() override {
    deinit_config_state();
    events::loop::instance().clear();
  }
};

TEST_F(ServiceExternalCommand, AddServiceDowntime) {
  configuration::error_cnt err;
  configuration::applier::host hst_aply;
  configuration::applier::service svc_aply;
  configuration::applier::command cmd_aply;
  configuration::Service svc;
  configuration::service_helper svc_hlp(&svc);
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  configuration::Command cmd;
  configuration::command_helper cmd_hlp(&cmd);
  cmd.set_command_name("cmd");

  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(1);

  svc.set_host_name("test_host");
  svc.set_service_description("test_description");
  svc.set_service_id(3);

  cmd.set_command_line("/usr/bin/echo 1");
  cmd_aply.add_object(cmd);

  hst.set_check_command("cmd");
  svc.set_check_command("cmd");

  hst_aply.add_object(hst);

  // We fake here the expand_object on configuration::service
  svc.set_host_id(1);

  svc_aply.add_object(svc);

  _state_hlp->expand(err);

  hst_aply.resolve_object(hst);
  svc_aply.resolve_object(svc);

  set_time(20000);
  time_t now = time(nullptr);

  std::string str{"test_host;test_description;1;|"};

  testing::internal::CaptureStdout();
  cmd_process_service_check_result(CMD_PROCESS_SERVICE_CHECK_RESULT, now,
                                   const_cast<char*>(str.c_str()));
  checks::checker::instance().reap();

  std::string const& out{testing::internal::GetCapturedStdout()};

  ASSERT_NE(out.find("PASSIVE SERVICE CHECK"), std::string::npos);
}

TEST_F(ServiceExternalCommand, AddServiceDowntimeByHostIpAddress) {
  configuration::applier::host hst_aply;
  configuration::applier::service svc_aply;
  configuration::applier::command cmd_aply;
  configuration::Service svc;
  configuration::service_helper svc_hlp(&svc);
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  configuration::Command cmd;
  configuration::command_helper cmd_hlp(&cmd);
  cmd.set_command_name("cmd");

  hst.set_host_name("test_host");
  hst.set_address("127.0.0.3");
  hst.set_host_id(1);

  svc.set_host_name("test_host");
  svc.set_service_description("test_description");
  svc.set_service_id(3);

  cmd.set_command_line("/usr/bin/echo 1");
  cmd_aply.add_object(cmd);

  hst.set_check_command("cmd");
  svc.set_check_command("cmd");

  hst_aply.add_object(hst);

  // We fake here the expand_object on configuration::service
  svc.set_host_id(1);

  svc_aply.add_object(svc);

  configuration::error_cnt err;
  _state_hlp->expand(err);

  hst_aply.resolve_object(hst);
  svc_aply.resolve_object(svc);

  set_time(20000);
  time_t now = time(nullptr);

  std::string str{"127.0.0.3;test_description;1;|"};

  testing::internal::CaptureStdout();
  cmd_process_service_check_result(CMD_PROCESS_SERVICE_CHECK_RESULT, now,
                                   const_cast<char*>(str.c_str()));
  checks::checker::instance().reap();

  std::string const& out{testing::internal::GetCapturedStdout()};

  ASSERT_NE(out.find("PASSIVE SERVICE CHECK"), std::string::npos);
}
