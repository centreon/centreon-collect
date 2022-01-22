/*
 * Copyright 2019 - 2021 Centreon (https://www.centreon.com/)
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

#include <fmt/format.h>
#include <cstdio>
#include <fstream>
#include <iostream>

#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/io/protocols.hh"
#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/broker/version.hh"

using namespace com::centreon;
using namespace com::centreon::broker;

class BrokerRpc : public ::testing::Test {
 public:
  void SetUp() override {
    pool::pool::load(0);
    stats::center::load();
    io::protocols::load();
    io::events::load();
  }

  void TearDown() override {
    io::events::unload();
    io::protocols::unload();
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
  std::vector<std::string> vectests = {"3\n", "10\n", "0\n", "15\n"};

  _stats = stats::center::instance().register_mysql_connection();
  stats::center::instance().update(&SqlConnectionStats::set_waiting_tasks,
                                   _stats, 3);

  _stats = stats::center::instance().register_mysql_connection();
  stats::center::instance().update(&SqlConnectionStats::set_waiting_tasks,
                                   _stats, 10);

  _stats = stats::center::instance().register_mysql_connection();
  stats::center::instance().update(&SqlConnectionStats::set_waiting_tasks,
                                   _stats, 0);

  _stats = stats::center::instance().register_mysql_connection();
  stats::center::instance().update(&SqlConnectionStats::set_waiting_tasks,
                                   _stats, 15);

  auto output = execute("GetSqlConnectionStatsValue 4");

  std::vector<std::string> results(output.size());
  std::copy(output.begin(), output.end(), results.begin());

  ASSERT_EQ(vectests, results);
  brpc.shutdown();
}

TEST_F(BrokerRpc, GetSqlConnectionSize) {
  brokerrpc brpc("0.0.0.0", 40000, "test");
  SqlConnectionStats* _stats;

  _stats = stats::center::instance().register_mysql_connection();
  stats::center::instance().update(&SqlConnectionStats::set_waiting_tasks,
                                   _stats, 3);

  _stats = stats::center::instance().register_mysql_connection();
  stats::center::instance().update(&SqlConnectionStats::set_waiting_tasks,
                                   _stats, 5);

  auto output = execute("GetSqlConnectionSize");
  ASSERT_EQ(output.front(), "connection array size: 2\n");

  brpc.shutdown();
}

TEST_F(BrokerRpc, GetConflictManagerStats) {
  brokerrpc brpc("0.0.0.0", 40000, "test");
  ConflictManagerStats* _stats;

  _stats = stats::center::instance().register_conflict_manager();
  stats::center::instance().update(&ConflictManagerStats::set_events_handled,
                                   _stats, 3);
  stats::center::instance().update(&ConflictManagerStats::set_loop_timeout,
                                   _stats, 30u);

  auto output = execute("GetConflictManagerStats");

  std::cout << output.front();
  brpc.shutdown();
}

TEST_F(BrokerRpc, GetMuxerStats) {
  brokerrpc brpc("0.0.0.0", 40000, "test");
  MuxerStats* _stats;
  std::vector<std::string> vectests{
      "name: mx1, unacknowledged_events: 1789, queue_file_name: qufl1, queue_file_bbdo_unacknowledged_events: 100.1, "
        "queue_file_bbdo_input_ack_limit: 100.2, queue_file_file_max_size: fms1, queue_file_file_expected_terminated_at: feta1, "
        "queue_file_file_percent_processed: fpp1, queue_file_file_read_offset: 100.3, queue_file_file_read_path: 100100, "
        "queue_file_file_write_offset: 100.4, queue_file_file_write_path: 100200\n",
      "name: mx2, unacknowledged_events: 1790, queue_file_name: qufl2, queue_file_bbdo_unacknowledged_events: 200.1, "
        "queue_file_bbdo_input_ack_limit: 200.2, queue_file_file_max_size: fms2, queue_file_file_expected_terminated_at: feta2, "
        "queue_file_file_percent_processed: fpp2, queue_file_file_read_offset: 200.3, queue_file_file_read_path: 200100, "
        "queue_file_file_write_offset: 200.4, queue_file_file_write_path: 200200\n",
      "name: mx3, unacknowledged_events: 1791, queue_file_name: qufl3, queue_file_bbdo_unacknowledged_events: 300.1, "
        "queue_file_bbdo_input_ack_limit: 300.2, queue_file_file_max_size: fms3, queue_file_file_expected_terminated_at: feta3, "
        "queue_file_file_percent_processed: fpp3, queue_file_file_read_offset: 300.3, queue_file_file_read_path: 300100, "
        "queue_file_file_write_offset: 300.4, queue_file_file_write_path: 300200\n"};

  _stats = stats::center::instance().register_muxer("mx1");
  stats::center::instance().update(&MuxerStats::set_unacknowledged_events, _stats, 1789u);
  stats::center::instance().update(_stats->mutable_queue_file()->mutable_name(), std::string("qufl1"));
  stats::center::instance().update(&QueueFileStats::set_bbdo_unacknowledged_events, _stats->mutable_queue_file(), 100.1);
  stats::center::instance().update(&QueueFileStats::set_bbdo_input_ack_limit, _stats->mutable_queue_file(), 100.2);
  stats::center::instance().update(_stats->mutable_queue_file()->mutable_file_max_size(), std::string("fms1"));
  stats::center::instance().update(_stats->mutable_queue_file()->mutable_file_expected_terminated_at(), std::string("feta1"));
  stats::center::instance().update(_stats->mutable_queue_file()->mutable_file_percent_processed(), std::string("fpp1"));
  stats::center::instance().update(&QueueFileStats::set_file_read_offset, _stats->mutable_queue_file(), 100.3);
  stats::center::instance().update(&QueueFileStats::set_file_read_path, _stats->mutable_queue_file(), 100100);
  stats::center::instance().update(&QueueFileStats::set_file_write_offset, _stats->mutable_queue_file(), 100.4);
  stats::center::instance().update(&QueueFileStats::set_file_write_path, _stats->mutable_queue_file(), 100200);

  _stats = stats::center::instance().register_muxer("mx2");
  stats::center::instance().update(&MuxerStats::set_unacknowledged_events, _stats, 1790u);
  stats::center::instance().update(_stats->mutable_queue_file()->mutable_name(), std::string("qufl2"));
  stats::center::instance().update(&QueueFileStats::set_bbdo_unacknowledged_events, _stats->mutable_queue_file(), 200.1);
  stats::center::instance().update(&QueueFileStats::set_bbdo_input_ack_limit, _stats->mutable_queue_file(), 200.2);
  stats::center::instance().update(_stats->mutable_queue_file()->mutable_file_max_size(), std::string("fms2"));
  stats::center::instance().update(_stats->mutable_queue_file()->mutable_file_expected_terminated_at(), std::string("feta2"));
  stats::center::instance().update(_stats->mutable_queue_file()->mutable_file_percent_processed(), std::string("fpp2"));
  stats::center::instance().update(&QueueFileStats::set_file_read_offset, _stats->mutable_queue_file(), 200.3);
  stats::center::instance().update(&QueueFileStats::set_file_read_path, _stats->mutable_queue_file(), 200100);
  stats::center::instance().update(&QueueFileStats::set_file_write_offset, _stats->mutable_queue_file(), 200.4);
  stats::center::instance().update(&QueueFileStats::set_file_write_path, _stats->mutable_queue_file(), 200200);

  _stats = stats::center::instance().register_muxer("mx3");
  stats::center::instance().update(&MuxerStats::set_unacknowledged_events, _stats, 1791u);
  stats::center::instance().update(_stats->mutable_queue_file()->mutable_name(), std::string("qufl3"));
  stats::center::instance().update(&QueueFileStats::set_bbdo_unacknowledged_events, _stats->mutable_queue_file(), 300.1);
  stats::center::instance().update(&QueueFileStats::set_bbdo_input_ack_limit, _stats->mutable_queue_file(), 300.2);
  stats::center::instance().update(_stats->mutable_queue_file()->mutable_file_max_size(), std::string("fms3"));
  stats::center::instance().update(_stats->mutable_queue_file()->mutable_file_expected_terminated_at(), std::string("feta3"));
  stats::center::instance().update(_stats->mutable_queue_file()->mutable_file_percent_processed(), std::string("fpp3"));
  stats::center::instance().update(&QueueFileStats::set_file_read_offset, _stats->mutable_queue_file(), 300.3);
  stats::center::instance().update(&QueueFileStats::set_file_read_path, _stats->mutable_queue_file(), 300100);
  stats::center::instance().update(&QueueFileStats::set_file_write_offset, _stats->mutable_queue_file(), 300.4);
  stats::center::instance().update(&QueueFileStats::set_file_write_path, _stats->mutable_queue_file(), 300200);

  std::list<std::string> output = execute("GetMuxerStats mx1 mx2 mx3");

  std::vector<std::string> results(output.size());
  std::copy(output.begin(), output.end(), results.begin());

  ASSERT_EQ(output.size(), 3u);
  ASSERT_EQ(results, vectests);

  brpc.shutdown();
}