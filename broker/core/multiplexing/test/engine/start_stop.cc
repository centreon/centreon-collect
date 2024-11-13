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
#include <gtest/gtest.h>

#include "com/centreon/broker/config/applier/init.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/io/raw.hh"
#include "com/centreon/broker/multiplexing/engine.hh"
#include "com/centreon/broker/multiplexing/muxer.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;

const std::string MSG1("0123456789abcdef");
const std::string MSG2("foo bar baz");
const std::string MSG3("last message with qux");
const std::string MSG4("no this is the last message");

class StartStop : public testing::Test {
 public:
  void SetUp() override {
    config::applier::init(com::centreon::common::BROKER, 0, "test_broker", 0);
  }

  void TearDown() override { config::applier::deinit(); }
};

/**
 *  Check that multiplexing engine works properly.
 *
 *  @return 0 on success.
 */
TEST_F(StartStop, MultiplexingWorks) {
  // Initialization.
  bool error{true};

  try {
    // Subscriber.
    multiplexing::muxer_filter filters{io::raw::static_type()};
    std::shared_ptr<multiplexing::muxer> mux(multiplexing::muxer::create(
        "core_multiplexing_engine_start_stop",
        multiplexing::engine::instance_ptr(), filters, filters, false));

    // Send events through engine.
    std::array<std::string, 2> messages{MSG1, MSG2};
    for (auto& m : messages) {
      auto data{std::make_shared<io::raw>()};
      data->append(m);
      multiplexing::engine::instance_ptr()->publish(data);
    }

    // Should read no events from muxer.
    {
      std::shared_ptr<io::data> data;
      mux->read(data, 0);
      ASSERT_FALSE(data);
    }

    // Start multiplexing engine.
    multiplexing::engine::instance_ptr()->start();

    // Read retained events.
    for (auto& m : messages) {
      std::shared_ptr<io::data> data;
      bool ret;
      do {
        ret = mux->read(data, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      } while (!ret);

      ASSERT_TRUE(data);
      ASSERT_EQ(data->type(), io::raw::static_type());
      std::shared_ptr<io::raw> raw(std::static_pointer_cast<io::raw>(data));
      ASSERT_EQ(strncmp(raw->const_data(), m.c_str(), m.size()), 0);
    }

    // Publish a new event.
    {
      auto data{std::make_shared<io::raw>()};
      data->append(MSG3);
      multiplexing::engine::instance_ptr()->publish(data);
    }

    // Read event.
    {
      std::shared_ptr<io::data> data;
      bool ret;
      do {
        ret = mux->read(data, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      } while (!ret);

      ASSERT_TRUE(data);
      ASSERT_EQ(data->type(), io::raw::static_type());
      auto raw{std::static_pointer_cast<io::raw>(data)};
      ASSERT_EQ(strncmp(raw->const_data(), MSG3.c_str(), MSG3.size()), 0);
    }

    // Stop multiplexing engine.
    multiplexing::engine::instance_ptr()->stop();

    // Publish a new event.
    {
      std::shared_ptr<io::raw> data(new io::raw);
      data->append(MSG4);
      multiplexing::engine::instance_ptr()->publish(
          std::static_pointer_cast<io::data>(data));
    }

    // Read no event.
    {
      std::shared_ptr<io::data> data;
      mux->read(data, 0);
      if (data)
        throw msg_fmt("error at step #6");
    }

    // Success.
    error = false;
  } catch (std::exception const& e) {
    std::cerr << e.what() << "\n";
  } catch (...) {
    std::cerr << "unknown exception\n";
  }

  // Return.
  ASSERT_FALSE(error);
}
