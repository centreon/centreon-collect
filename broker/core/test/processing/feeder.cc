/**
 * Copyright 2020-2021 Centreon (https://www.centreon.com/)
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

#include "com/centreon/broker/processing/feeder.hh"
#include <gtest/gtest.h>
#include "com/centreon/broker/config/applier/state.hh"
#include "com/centreon/broker/file/disk_accessor.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/io/protocols.hh"
#include "com/centreon/broker/multiplexing/engine.hh"
#include "com/centreon/broker/multiplexing/muxer_filter.hh"
#include "com/centreon/broker/pool.hh"
#include "com/centreon/broker/stats/center.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::processing;

extern std::shared_ptr<asio::io_context> g_io_context;

class TestStream : public io::stream {
 public:
  TestStream() : io::stream("TestStream") {}
  bool read(std::shared_ptr<io::data>&, time_t) override { return true; }

  int32_t write(std::shared_ptr<io::data> const&) override { return 1; }
  int32_t stop() override { return 0; }
};

class TestFeeder : public ::testing::Test {
 protected:
  std::shared_ptr<feeder> _feeder;

 public:
  void SetUp() override {
    g_io_context->restart();
    pool::load(g_io_context, 0);
    stats::center::load();
    config::applier::state::load();
    file::disk_accessor::load(10000);
    multiplexing::engine::load();
    io::protocols::load();
    io::events::load();

    std::shared_ptr<io::stream> client(new TestStream);
    multiplexing::muxer_filter read_filters;
    multiplexing::muxer_filter write_filters;
    _feeder =
        feeder::create("test-feeder", multiplexing::engine::instance_ptr(),
                       client, read_filters, write_filters);
  }

  void TearDown() override {
    _feeder->stop();
    _feeder.reset();
    multiplexing::engine::unload();
    config::applier::state::unload();
    io::events::unload();
    io::protocols::unload();
    stats::center::unload();
    file::disk_accessor::unload();
    pool::unload();
  }
};

TEST_F(TestFeeder, ImmediateStartExit) {
  ASSERT_NO_THROW(_feeder->stop());
}

TEST_F(TestFeeder, isFinished) {
  // It began
  ASSERT_FALSE(_feeder->is_finished());
  nlohmann::json tree;
  _feeder->stats(tree);
  ASSERT_EQ(tree["state"].get<std::string>(), "connected");
}
