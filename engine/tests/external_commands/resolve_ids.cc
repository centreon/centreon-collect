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
#include "com/centreon/engine/configuration/applier/command.hh"
#include "com/centreon/engine/configuration/applier/host.hh"
#include "com/centreon/engine/configuration/applier/service.hh"
#include "com/centreon/engine/host.hh"
#include "com/centreon/engine/service.hh"
#include "helper.hh"

using namespace com::centreon::engine;

/* Broker routes external commands with the host/service ids it resolved; the
 * poller rewrites the names from them before parsing (see
 * processing::resolve_ids). */
class ExternalCommandResolveIds : public ::testing::Test {
 protected:
  std::unique_ptr<configuration::state_helper> _state_hlp;

 public:
  void SetUp() override {
    _state_hlp = init_config_state();
    configuration::error_cnt err;
    configuration::applier::host hst_aply;
    configuration::applier::service svc_aply;
    configuration::applier::command cmd_aply;
    configuration::Command cmd;
    configuration::command_helper cmd_hlp(&cmd);
    cmd.set_command_name("cmd");
    cmd.set_command_line("/usr/bin/echo 1");
    cmd_aply.add_object(cmd);

    configuration::Host hst;
    configuration::host_helper hst_hlp(&hst);
    hst.set_host_name("my host");
    hst.set_address("127.0.0.1");
    hst.set_host_id(26);
    hst.set_check_command("cmd");
    hst_aply.add_object(hst);

    configuration::Service svc;
    configuration::service_helper svc_hlp(&svc);
    svc.set_host_name("my host");
    svc.set_service_description("my service");
    svc.set_service_id(503);
    svc.set_host_id(26);
    svc.set_check_command("cmd");
    svc_aply.add_object(svc);

    _state_hlp->expand(err);
    hst_aply.resolve_object(hst);
    svc_aply.resolve_object(svc);
  }
  void TearDown() override { deinit_config_state(); }
};

TEST_F(ExternalCommandResolveIds, ServiceNamesRewrittenFromIds) {
  EXPECT_EQ(commands::processing::resolve_ids(
                "[12] SCHEDULE_FORCED_SVC_CHECK;;;1790600000", 26, 503),
            "[12] SCHEDULE_FORCED_SVC_CHECK;my host;my service;1790600000");
  /* Stale names are replaced, the remaining arguments kept verbatim. */
  EXPECT_EQ(
      commands::processing::resolve_ids(
          "[12] PROCESS_SERVICE_CHECK_RESULT;old;stale;2;out|a=1;b=2", 26, 503),
      "[12] PROCESS_SERVICE_CHECK_RESULT;my host;my service;2;out|a=1;b=2");
}

TEST_F(ExternalCommandResolveIds, HostNameRewrittenFromId) {
  EXPECT_EQ(commands::processing::resolve_ids(
                "[12] SCHEDULE_FORCED_HOST_CHECK;;1790600000", 26, 0),
            "[12] SCHEDULE_FORCED_HOST_CHECK;my host;1790600000");
  EXPECT_EQ(
      commands::processing::resolve_ids("[12] ENABLE_HOST_CHECK;x", 26, 0),
      "[12] ENABLE_HOST_CHECK;my host");
}

TEST_F(ExternalCommandResolveIds, LeftUntouchedWhenNotApplicable) {
  /* No id: the line is what PHP wrote. */
  EXPECT_EQ(commands::processing::resolve_ids(
                "[12] SCHEDULE_FORCED_HOST_CHECK;my host;1", 0, 0),
            "[12] SCHEDULE_FORCED_HOST_CHECK;my host;1");
  /* Not a host/service command. */
  EXPECT_EQ(
      commands::processing::resolve_ids("[12] ENABLE_NOTIFICATIONS", 26, 0),
      "[12] ENABLE_NOTIFICATIONS");
  /* Unknown ids: left to the parser's own error handling. */
  EXPECT_EQ(commands::processing::resolve_ids(
                "[12] SCHEDULE_FORCED_HOST_CHECK;my host;1", 999, 0),
            "[12] SCHEDULE_FORCED_HOST_CHECK;my host;1");
  EXPECT_EQ(commands::processing::resolve_ids(
                "[12] SCHEDULE_FORCED_SVC_CHECK;my host;x;1", 26, 999),
            "[12] SCHEDULE_FORCED_SVC_CHECK;my host;x;1");
}

/* End to end through the parser: a line with empty names and the ids reaches
 * the handler and acts on the right object. */
TEST_F(ExternalCommandResolveIds, ResolvedLineExecutes) {
  auto svc = service::services_by_id.find({26, 503})->second;
  ASSERT_TRUE(svc->active_checks_enabled());
  std::string line =
      commands::processing::resolve_ids("[12] DISABLE_SVC_CHECK;;", 26, 503);
  ASSERT_TRUE(commands::processing::execute(line));
  EXPECT_FALSE(svc->active_checks_enabled());
}
