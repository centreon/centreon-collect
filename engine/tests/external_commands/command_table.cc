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
#include "common/external_commands/command_table.hh"

using namespace com::centreon::engine::commands;
namespace ec = com::centreon::common::external_commands;

/* The routing table Broker uses (common/external_commands) must list exactly
 * the commands Engine's parser knows: a command missing on the Broker side
 * would be refused as unknown by ExecuteExternalCommand, a command missing on
 * the Engine side would be routed to a poller that ignores it. */
TEST(ExternalCommandTable, MatchesEngineProcessingTable) {
  std::vector<std::string_view> names = processing::command_names();
  ASSERT_FALSE(names.empty());
  for (std::string_view name : names) {
    auto info = ec::lookup(name);
    EXPECT_TRUE(info.has_value()) << "missing in common table: " << name;
  }
  /* Spot checks on the classification Broker routes with. */
  EXPECT_EQ(ec::lookup("SCHEDULE_FORCED_SVC_CHECK")->kind, ec::target::service);
  EXPECT_EQ(ec::lookup("PROCESS_HOST_CHECK_RESULT")->kind, ec::target::host);
  EXPECT_EQ(ec::lookup("ENABLE_HOSTGROUP_HOST_CHECKS")->kind,
            ec::target::hostgroup);
  EXPECT_EQ(ec::lookup("ENABLE_NOTIFICATIONS")->kind, ec::target::global);
  EXPECT_EQ(ec::lookup("RESTART_PROGRAM")->kind, ec::target::process);
  EXPECT_EQ(ec::lookup("ACKNOWLEDGE_SVC_PROBLEM")->broker_rpc,
            "AcknowledgeServiceProblem");
  EXPECT_TRUE(ec::lookup("SCHEDULE_SVC_CHECK")->broker_rpc.empty());
  EXPECT_FALSE(ec::lookup("NOT_A_COMMAND").has_value());
}

/* Thread-safety flag: only the passive check results may be executed on the
 * receiving thread, which is what the Broker downward channel relies on. */
TEST(ExternalCommandTable, OnlyCheckResultsAreThreadSafe) {
  EXPECT_TRUE(processing::is_thread_safe(
      "[1] PROCESS_SERVICE_CHECK_RESULT;host;svc;0;ok"));
  EXPECT_TRUE(
      processing::is_thread_safe("[1] PROCESS_HOST_CHECK_RESULT;host;0;ok"));
  EXPECT_FALSE(
      processing::is_thread_safe("[1] SCHEDULE_FORCED_SVC_CHECK;host;svc;1"));
  EXPECT_FALSE(processing::is_thread_safe("[1] ENABLE_NOTIFICATIONS"));
}
