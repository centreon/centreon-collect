/**
 * Copyright 2011 - 2019 Centreon (https://www.centreon.com/)
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

#include "com/centreon/broker/processing/acceptor.hh"
#include <gtest/gtest.h>
#include "com/centreon/broker/config/applier/init.hh"
#include "com/centreon/broker/io/raw.hh"
#include "com/centreon/broker/multiplexing/muxer_filter.hh"
#include "common/log_v2/log_v2.hh"
#include "temporary_endpoint.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::processing;
using com::centreon::common::log_v2::log_v2;

class ProcessingTest : public ::testing::Test {
 public:
  void SetUp() override {
    try {
      config::applier::init(com::centreon::common::BROKER, 0, "test_broker", 0);
    } catch (std::exception const& e) {
      (void)e;
    }

    log_v2::instance().get(log_v2::CORE)->set_level(spdlog::level::info);
    _endpoint = std::make_shared<temporary_endpoint>();
  }

  void TearDown() override {
    _endpoint.reset();
    config::applier::deinit();
  }

 protected:
  std::shared_ptr<io::endpoint> _endpoint;
};

TEST_F(ProcessingTest, NotStarted) {
  multiplexing::muxer_filter f{};
  std::unique_ptr<acceptor> acc =
      std::make_unique<acceptor>(_endpoint, "temporary_endpoint", f, f);
  ASSERT_NO_THROW(acc->exit());
}

TEST_F(ProcessingTest, StartStop1) {
  multiplexing::muxer_filter f{};
  auto acc = std::make_unique<acceptor>(_endpoint, "temporary_endpoint", f, f);
  acc->start();
  ASSERT_NO_THROW(acc->exit());
}

TEST_F(ProcessingTest, StartStop2) {
  multiplexing::muxer_filter f{};
  auto acc = std::make_unique<acceptor>(_endpoint, "temporary_endpoint", f, f);
  acc->start();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  ASSERT_NO_THROW(acc->exit());
}

TEST_F(ProcessingTest, StartStop3) {
  multiplexing::muxer_filter f{};
  auto acc = std::make_unique<acceptor>(_endpoint, "temporary_endpoint", f, f);
  acc->start();
  std::this_thread::sleep_for(std::chrono::seconds(1));
  ASSERT_NO_THROW(acc->exit());
}

TEST_F(ProcessingTest, StartWithFilterStop) {
  multiplexing::muxer_filter filters({});
  filters.insert(io::raw::static_type());
  multiplexing::muxer_filter f{};
  auto acc =
      std::make_unique<acceptor>(_endpoint, "temporary_endpoint", filters, f);
  time_t now{time(nullptr)};
  acc->set_retry_interval(2);
  acc->start();
  std::this_thread::sleep_for(std::chrono::milliseconds(1500));
  ASSERT_NO_THROW(acc->exit());
  time_t now1{time(nullptr)};
  ASSERT_TRUE(now1 <= now + 3);
}
