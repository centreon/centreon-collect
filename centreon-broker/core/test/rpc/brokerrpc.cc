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

#include "com/centreon/broker/brokerrpc.hh"
#include "com/centreon/broker/pool.hh"
#include "com/centreon/broker/stats/center.hh"

#include <gtest/gtest.h>

#include <google/protobuf/util/time_util.h>

#include <fmt/format.h>
#include <cstdio>
#include <fstream>
#include <iostream>

#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/broker/version.hh"

using namespace com::centreon;
using namespace com::centreon::broker;

class BrokerRpc : public ::testing::Test {
 public:
  void SetUp() override {
    pool::pool::load(0);
    stats::center::load();
  }

  void TearDown() override {
    stats::center::unload();
    pool::pool::unload();
  }

  std::list<std::string> execute(const std::string& command) {
    std::list<std::string> retval;
    char path[1024];
    std::ostringstream oss;
    oss << "test/rpc_client " << command;

    FILE* fp = popen(oss.str().c_str(), "r");
    while (fgets(path, sizeof(path), fp) != nullptr) {
      retval.emplace_back(path);
    }
    pclose(fp);
    return retval;
  }
};

TEST_F(BrokerRpc, StartStop) {
  brokerrpc brpc("0.0.0.0", 40000, "test");
  ASSERT_NO_THROW(brpc.shutdown());
}

TEST_F(BrokerRpc, GetVersion) {
  brokerrpc brpc("0.0.0.0", 40000, "test");
  auto output = execute("GetVersion");
#if CENTREON_BROKER_PATCH == 0
  ASSERT_EQ(output.size(), 2u);
  ASSERT_EQ(output.front(),
            fmt::format("GetVersion: major: {}\n", version::major));
  ASSERT_EQ(output.back(), fmt::format("minor: {}\n", version::minor));
#else
  ASSERT_EQ(output.size(), 3u);
  ASSERT_EQ(output.front(),
            fmt::format("GetVersion: major: {}\n", version::major));
  ASSERT_EQ(output.back(), fmt::format("patch: {}\n", version::patch));
#endif
  brpc.shutdown();
}

TEST_F(BrokerRpc, GetSqlConnectionStatsValue) {
  brokerrpc brpc("0.0.0.0", 40000, "test");
  SqlConnectionStats* _stats;
  std::vector<std::string> vectests = {
      "waiting_tasks: 3, is_connected: false, down_since: 2567\n",
      "waiting_tasks: 10, is_connected: true, up_since: 1234\n",
      "waiting_tasks: 0, is_connected: false, down_since: 115\n",
      "waiting_tasks: 15, is_connected: true, up_since: 356\n"};

  _stats = stats::center::instance().register_mysql_connection();
  stats::center::instance().update(&SqlConnectionStats::set_waiting_tasks, _stats, 3);
  stats::center::instance().update(&SqlConnectionStats::set_is_connected, _stats, false);
  stats::center::instance().update(&SqlConnectionStats::set_down_since, _stats, 2567lu);

  _stats = stats::center::instance().register_mysql_connection();
  stats::center::instance().update(&SqlConnectionStats::set_waiting_tasks, _stats, 10);
  stats::center::instance().update(&SqlConnectionStats::set_is_connected, _stats, true);
  stats::center::instance().update(&SqlConnectionStats::set_up_since, _stats, 1234lu);

  _stats = stats::center::instance().register_mysql_connection();
  stats::center::instance().update(&SqlConnectionStats::set_waiting_tasks, _stats, 0);
  stats::center::instance().update(&SqlConnectionStats::set_is_connected, _stats, false);
  stats::center::instance().update(&SqlConnectionStats::set_down_since, _stats, 115lu);

  _stats = stats::center::instance().register_mysql_connection();
  stats::center::instance().update(&SqlConnectionStats::set_waiting_tasks, _stats, 15);
  stats::center::instance().update(&SqlConnectionStats::set_is_connected, _stats, true);
  stats::center::instance().update(&SqlConnectionStats::set_up_since, _stats, 356lu);

  auto output = execute("GetSqlConnectionStatsValue 4");

  std::vector<std::string> results(output.size());
  std::copy(output.begin(), output.end(), results.begin());

  ASSERT_EQ(vectests, results);
  brpc.shutdown();
}

TEST_F(BrokerRpc, GetAllSqlConnectionsStatsValues) {
  brokerrpc brpc("0.0.0.0", 40000, "test");
  SqlConnectionStats* _stats;

  std::vector<std::string> vectests = {
    "waiting_tasks: 3, is_connected: false, down_since: 2567\n",
    "waiting_tasks: 10, is_connected: true, up_since: 1234\n",
    "waiting_tasks: 0, is_connected: false, down_since: 115\n",
    "waiting_tasks: 15, is_connected: true, up_since: 356\n"};

  _stats = stats::center::instance().register_mysql_connection();
  stats::center::instance().update(&SqlConnectionStats::set_waiting_tasks, _stats, 3);
  stats::center::instance().update(&SqlConnectionStats::set_is_connected, _stats, false);
  stats::center::instance().update(&SqlConnectionStats::set_down_since, _stats, 2567lu);

  _stats = stats::center::instance().register_mysql_connection();
  stats::center::instance().update(&SqlConnectionStats::set_waiting_tasks, _stats, 10);
  stats::center::instance().update(&SqlConnectionStats::set_is_connected, _stats, true);
  stats::center::instance().update(&SqlConnectionStats::set_up_since, _stats, 1234lu);

  _stats = stats::center::instance().register_mysql_connection();
  stats::center::instance().update(&SqlConnectionStats::set_waiting_tasks, _stats, 0);
  stats::center::instance().update(&SqlConnectionStats::set_is_connected, _stats, false);
  stats::center::instance().update(&SqlConnectionStats::set_down_since, _stats, 115lu);

  _stats = stats::center::instance().register_mysql_connection();
  stats::center::instance().update(&SqlConnectionStats::set_waiting_tasks, _stats, 15);
  stats::center::instance().update(&SqlConnectionStats::set_is_connected, _stats, true);
  stats::center::instance().update(&SqlConnectionStats::set_up_since, _stats, 356lu);

  auto size = execute("GetSqlConnectionSize");
  ASSERT_EQ(size.front(), "connection array size: 4\n");

  auto output = execute("GetAllSqlConnectionsStatsValues");
  std::vector<std::string> results(output.size());
  std::copy(output.begin(), output.end(), results.begin());

  ASSERT_EQ(vectests, results);

  brpc.shutdown();
}

TEST_F(BrokerRpc, GetSqlConnectionSize) {
  brokerrpc brpc("0.0.0.0", 40000, "test");
  SqlConnectionStats* _stats;

  _stats = stats::center::instance().register_mysql_connection();
  stats::center::instance().update(&SqlConnectionStats::set_waiting_tasks, _stats, 3);

  _stats = stats::center::instance().register_mysql_connection();
  stats::center::instance().update(&SqlConnectionStats::set_waiting_tasks, _stats, 5);

  auto output = execute("GetSqlConnectionSize");
  ASSERT_EQ(output.front(), "connection array size: 2\n");

  brpc.shutdown();
}

TEST_F(BrokerRpc, GetConflictManagerStats) {
  brokerrpc brpc("0.0.0.0", 40000, "test");
  ConflictManagerStats* _stats;

  _stats = stats::center::instance().register_conflict_manager();
  stats::center::instance().update(&ConflictManagerStats::set_events_handled, _stats, 3);
  stats::center::instance().update(&ConflictManagerStats::set_loop_timeout, _stats, 30u);

  auto output = execute("GetConflictManagerStats");

  std::cout << output.front();
  brpc.shutdown();
}
