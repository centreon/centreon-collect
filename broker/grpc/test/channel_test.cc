/*
 * Copyright 2022 Centreon (https://www.centreon.com/)
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
 *
 */

#include <thread>
#include "common/log_v2/log_v2.hh"
#include "grpc_test_include.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::grpc;
using namespace com::centreon::exceptions;
using com::centreon::common::log_v2::log_v2;

extern std::shared_ptr<asio::io_context> g_io_context;

class grpc_channel_tester : public testing::Test {
 public:
  static void SetUpTestSuite() {
    g_io_context->restart();
    com::centreon::broker::pool::load(g_io_context, 1);
    // log_v2::grpc()->set_level(spdlog::level::trace);
  }
};

static com::centreon::broker::grpc::grpc_config::pointer conf1(
    std::make_shared<com::centreon::broker::grpc::grpc_config>("1.2.3.4:1234"));

class channel_test : public channel {
 public:
  using event_send_cont = std::multimap<system_clock::time_point,
                                        channel::event_with_data::pointer>;
  event_send_cont sent;

  std::queue<event_ptr> to_read;
  using event_read_cont = std::multimap<system_clock::time_point, event_ptr>;
  event_read_cont readen;

  using pointer = std::shared_ptr<channel_test>;
  pointer shared_from_this() {
    return std::static_pointer_cast<channel_test>(channel::shared_from_this());
  }

  channel_test(const grpc_config::pointer& conf)
      : channel("channel_test",
                conf,
                log_v2::instance().create_logger_or_get_id("grpc")) {}

  void start_write(const channel::event_with_data::pointer& to_send) override {
    pool::io_context().post(
        [me = shared_from_this()]() { me->simul_on_write(); });
  }

  virtual void simul_on_write() {
    sent.emplace(system_clock::now(), _write_current);
    on_write_done(true);
  }

  void start_read(event_ptr& to_read,
                  bool first_read [[maybe_unused]]) override {
    pool::io_context().post(
        [me = shared_from_this(), to_read]() { me->simul_on_read(); });
  }

  void shutdown() override {}

  virtual void simul_on_read() {
    if (!to_read.empty()) {
      _read_current = to_read.front();
      to_read.pop();
      readen.emplace(system_clock::now(), _read_current);
      on_read_done(true);
    }
  }

  void start() { channel::start(); }

  static pointer create(const grpc_config::pointer& conf) {
    pointer ret(std::make_shared<channel_test>(conf));
    return ret;
  }
};

TEST_F(grpc_channel_tester, write_all) {
  std::shared_ptr<channel_test> channel = channel_test::create(conf1);
  channel->start();

  for (int ii = 0; ii < 100; ++ii) {
    channel->write(std::make_shared<channel::event_with_data>());
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  ASSERT_EQ((channel->sent.size()), 100);
}

TEST_F(grpc_channel_tester, read_all) {
  std::shared_ptr<channel_test> channel = channel_test::create(conf1);
  for (int ii = 0; ii < 100; ++ii) {
    channel->to_read.push(std::make_shared<grpc_event_type>());
  }
  channel->start();
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  EXPECT_EQ(channel->readen.size(), 100);
  for (int ii = 0; ii < 100; ++ii) {
    auto read_ret = channel->read(std::chrono::milliseconds(10));
    ASSERT_TRUE(read_ret.second) << "ii=" << ii;
  }

  system_clock::time_point read_start = system_clock::now();
  auto read_ret = channel->read(std::chrono::milliseconds(10));
  system_clock::time_point read_end = system_clock::now();

  ASSERT_FALSE(read_ret.second);

  ASSERT_LE(std::chrono::milliseconds(9),
            std::chrono::duration_cast<std::chrono::milliseconds>(read_end -
                                                                  read_start));
  ASSERT_LE(std::chrono::duration_cast<std::chrono::milliseconds>(read_end -
                                                                  read_start),
            std::chrono::milliseconds(200));
}

TEST_F(grpc_channel_tester, throw_read_after_failure) {
  class channel_test_failure : public channel_test {
   public:
    int failure_ind;
    channel_test_failure(const grpc_config::pointer& conf)
        : channel_test(conf) {}

    void simul_on_read() override {
      if (!to_read.empty()) {
        _read_current = to_read.front();
        to_read.pop();
        readen.emplace(system_clock::now(), _read_current);
        on_read_done(--failure_ind >= 0);
      }
    }
  };

  std::shared_ptr<channel_test_failure> channel(
      std::make_shared<channel_test_failure>(conf1));
  for (int ii = 0; ii < 100; ++ii) {
    channel->to_read.push(std::make_shared<grpc_event_type>());
  }
  int failure_ind = channel->failure_ind = rand() % 80 + 5;

  channel->start();
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  EXPECT_EQ(channel->readen.size(), failure_ind + 1);
  for (int ii = 0; ii < failure_ind; ++ii) {
    auto read_ret = channel->read(std::chrono::milliseconds(10));
    ASSERT_TRUE(read_ret.second) << "ii=" << ii;
  }

  ASSERT_THROW(channel->read(std::chrono::milliseconds(10)), msg_fmt);
}

TEST_F(grpc_channel_tester, throw_write_after_failure) {
  class channel_test_failure : public channel_test {
   public:
    int failure_ind;
    channel_test_failure(const grpc_config::pointer& conf)
        : channel_test(conf) {}

    void simul_on_write() override {
      sent.emplace(system_clock::now(), _write_current);
      on_write_done(--failure_ind >= 0);
    }
  };
  std::shared_ptr<channel_test_failure> channel(
      std::make_shared<channel_test_failure>(conf1));

  channel->start();

  int failure_ind = channel->failure_ind = rand() % 100 + 10;

  for (int32_t ii = 0; ii <= failure_ind; ++ii) {
    channel->write(std::make_shared<channel::event_with_data>());
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  ASSERT_THROW(channel->write(std::make_shared<channel::event_with_data>()),
               msg_fmt);
  ASSERT_THROW(channel->read(std::chrono::milliseconds(1)), msg_fmt);
}
