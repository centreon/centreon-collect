/**
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
#include "com/centreon/broker/stats/center.hh"

#include <gtest/gtest.h>

#include <fmt/format.h>

#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/io/protocols.hh"
#include "com/centreon/broker/version.hh"

using namespace com::centreon;
using namespace com::centreon::broker;

class BrokerRpc : public ::testing::Test {
 protected:
  std::shared_ptr<stats::center> _center;

 public:
  void SetUp() override {
    _center = std::make_shared<stats::center>();
    io::protocols::load();
    io::events::load();
  }

  void TearDown() override {
    io::events::unload();
    io::protocols::unload();
  }

  std::list<std::string> execute(const std::string& command) {
    std::list<std::string> retval;
    char path[1024];
    std::string client{fmt::format("tests/rpc_client {}", command)};

    FILE* fp = popen(client.c_str(), "r");
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

TEST_F(BrokerRpc, GetMuxerStats) {
  brokerrpc brpc("0.0.0.0", 40000, "test");
  std::vector<std::string> vectests{
      "name: mx1, queue_file: qufl_, "
      "unacknowledged_events: "
      "1789\n",
      "name: mx2, queue_file: _qufl, "
      "unacknowledged_events: "
      "1790\n"};

  _center->update_muxer("mx1", "qufl_", 18u, 1789u);

  _center->update_muxer("mx2", "_qufl", 18u, 1790u);

  std::list<std::string> output = execute("GetMuxerStats mx1 mx2");

  std::vector<std::string> results(output.size());
  std::copy(output.begin(), output.end(), results.begin());

  ASSERT_EQ(output.size(), 2u);
  ASSERT_EQ(results, vectests);

  brpc.shutdown();
}
