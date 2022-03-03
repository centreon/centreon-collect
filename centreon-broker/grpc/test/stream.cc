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
 */

#include <condition_variable>
#include <mutex>
#include <set>

#include <absl/strings/string_view.h>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include <grpcpp/create_channel.h>
#include "../src/grpc_stream.pb.h"

using system_clock = std::chrono::system_clock;
using time_point = system_clock::time_point;
using duration = system_clock::duration;
using unique_lock = std::unique_lock<std::mutex>;

#include "com/centreon/broker/grpc/acceptor.hh"
#include "com/centreon/broker/grpc/stream.hh"
#include "com/centreon/broker/io/raw.hh"
#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/broker/pool.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "tcp_relais.hh"

using namespace com::centreon::broker;
using namespace com::centreon::exceptions;

using event_ptr =
    std::shared_ptr<com::centreon::broker::stream::centreon_event>;

const static std::string test_hostport("127.0.0.1:4444");

struct test_param {
  uint32_t data_type;
  uint32_t src;
  uint32_t dest;
  std::string buffer;
};

test_param tests_feed[] = {{100, 1200, 1300, "bonjour1"},
                           {200, 2200, 2300, "bonjour2"},
                           {300, 3200, 3300, "bonjour3"}};

std::shared_ptr<io::raw> create_event(const test_param& param) {
  std::shared_ptr<io::raw> ret = std::make_shared<io::raw>();
  ret->_buffer.assign(param.buffer.begin(), param.buffer.end());
  return ret;
}

class grpc_test_server : public ::testing::TestWithParam<test_param> {
 protected:
  static std::unique_ptr<com::centreon::broker::grpc::acceptor> s;

 public:
  static void SetUpTestSuite() {
    // log_v2::grpc()->set_level(spdlog::level::trace);
    s = std::make_unique<com::centreon::broker::grpc::acceptor>(4444);
  }
  static void TearDownTestSuite() { s.reset(); };

 protected:
  void SetUp() override {}

  void TearDown() override {}
};

std::unique_ptr<com::centreon::broker::grpc::acceptor> grpc_test_server::s;

#define COMPARE_EVENT(read_ret, received, param)             \
  ASSERT_TRUE(read_ret);                                     \
  {                                                          \
    std::shared_ptr<io::raw> raw_receive =                   \
        std::dynamic_pointer_cast<io::raw>(received);        \
    ASSERT_TRUE(raw_receive.get() != nullptr)                \
        << " failed to cast to io::raw ";                    \
    EXPECT_EQ(std::string(raw_receive->get_buffer().begin(), \
                          raw_receive->get_buffer().end()),  \
              param.buffer)                                  \
        << " buffer incorrect ";                             \
  }

TEST_P(grpc_test_server, ClientToServerSendReceive) {
  com::centreon::broker::grpc::stream client(test_hostport);
  std::unique_ptr<io::stream> accepted =
      s->open(std::chrono::milliseconds(100));

  unsigned nb_written = 0;
  for (unsigned test_ind = 0; test_ind < 100; ++test_ind) {
    test_param param = GetParam();
    param.data_type += test_ind;
    param.src += test_ind;
    param.dest += test_ind;
    param.buffer += "_";
    param.buffer += std::to_string(test_ind);
    log_v2::grpc()->debug("{} write param.data_type={}", __PRETTY_FUNCTION__,
                          param.data_type);
    nb_written += client.write(create_event(param));

    std::shared_ptr<io::data> receive;
    bool read_ret = accepted->read(receive, time(nullptr) + 2);
    log_v2::grpc()->debug("{} read_ret={} param.data_type={}",
                          __PRETTY_FUNCTION__, read_ret, param.data_type);
    COMPARE_EVENT(read_ret, receive, param);
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  nb_written += client.flush();
  ASSERT_EQ(nb_written, 100);
}

INSTANTIATE_TEST_SUITE_P(GRPCTestServer,
                         grpc_test_server,
                         ::testing::ValuesIn(tests_feed));

TEST_P(grpc_test_server, ServerToClientSendReceive) {
  unsigned nb_written = 0;
  com::centreon::broker::grpc::stream client(test_hostport);
  std::unique_ptr<io::stream> accepted =
      s->open(std::chrono::milliseconds(100));
  for (unsigned test_ind = 0; test_ind < 100; ++test_ind) {
    test_param param = GetParam();
    param.data_type += test_ind;
    param.src += test_ind;
    param.dest += test_ind;
    param.buffer += "_";
    param.buffer += std::to_string(test_ind);
    log_v2::grpc()->debug("{} write param.data_type={}", __PRETTY_FUNCTION__,
                          param.data_type);
    nb_written += accepted->write(create_event(param));

    std::shared_ptr<io::data> receive;
    bool read_ret = client.read(receive, time(nullptr) + 2);
    log_v2::grpc()->debug("{} read_ret={} param.data_type={}",
                          __PRETTY_FUNCTION__, read_ret, param.data_type);
    COMPARE_EVENT(read_ret, receive, param);
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  nb_written += accepted->flush();
  ASSERT_EQ(nb_written, 100);
}

class grpc_comm_failure : public ::testing::TestWithParam<test_param> {
 protected:
  static std::unique_ptr<com::centreon::broker::grpc::acceptor> s;
  static std::unique_ptr<test_util::tcp_relais> relay;

 public:
  static constexpr unsigned relay_listen_port = 5123u;
  static constexpr unsigned server_listen_port = 5124u;

  static void SetUpTestSuite() {
    srand(time(nullptr));
    // log_v2::grpc()->set_level(spdlog::level::trace);
    s = std::make_unique<com::centreon::broker::grpc::acceptor>(
        server_listen_port);
    relay = std::make_unique<test_util::tcp_relais>(
        "127.0.0.1", relay_listen_port, "localhost", server_listen_port);
  }
  static void TearDownTestSuite() {
    s.reset();
    relay.reset();
  };

 protected:
  void SetUp() override {}

  void TearDown() override {}
};

constexpr unsigned grpc_comm_failure::relay_listen_port;
constexpr unsigned grpc_comm_failure::server_listen_port;

std::unique_ptr<com::centreon::broker::grpc::acceptor> grpc_comm_failure::s;
std::unique_ptr<test_util::tcp_relais> grpc_comm_failure::relay;

INSTANTIATE_TEST_SUITE_P(GRPCTestCommFailure,
                         grpc_comm_failure,
                         ::testing::ValuesIn(tests_feed));

TEST_P(grpc_comm_failure, ClientToServerFailureBeforeWrite) {
  com::centreon::broker::grpc::stream client("127.0.0.1:" +
                                             std::to_string(relay_listen_port));
  std::unique_ptr<io::stream> accepted =
      s->open(std::chrono::milliseconds(100));

  test_param param = GetParam();
  log_v2::grpc()->debug("{} write param.data_type={}", __PRETTY_FUNCTION__,
                        param.data_type);
  client.write(create_event(GetParam()));
  std::shared_ptr<io::data> receive;
  bool read_ret = accepted->read(receive, time(nullptr) + 2);
  log_v2::grpc()->debug("{} read_ret={} param.data_type={}",
                        __PRETTY_FUNCTION__, read_ret, param.data_type);
  COMPARE_EVENT(read_ret, receive, param);

  relay->shutdown_relays();

  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  ASSERT_THROW(client.write(create_event(GetParam())), msg_fmt);
}

TEST_P(grpc_comm_failure, ClientToServerFailureAfterWrite) {
  com::centreon::broker::grpc::stream client("127.0.0.1:" +
                                             std::to_string(relay_listen_port));
  std::unique_ptr<io::stream> accepted =
      s->open(std::chrono::milliseconds(100));

  test_param param = GetParam();
  log_v2::grpc()->debug("{} write param.data_type={}", __PRETTY_FUNCTION__,
                        param.data_type);
  client.write(create_event(param));
  std::shared_ptr<io::data> receive;
  bool read_ret = accepted->read(receive, time(nullptr) + 2);
  log_v2::grpc()->debug("{} read_ret={} param.data_type={}",
                        __PRETTY_FUNCTION__, read_ret, param.data_type);
  COMPARE_EVENT(read_ret, receive, param);

  unsigned offset = rand();
  param.data_type += offset;
  param.src += offset;
  param.dest += offset;
  param.buffer += "_";
  param.buffer += std::to_string(offset);
  client.write(create_event(param));
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  relay->shutdown_relays();
  std::this_thread::sleep_for(std::chrono::milliseconds(5));

  read_ret = accepted->read(receive, time(nullptr) + 2);
  COMPARE_EVENT(read_ret, receive, param);
}

TEST_P(grpc_comm_failure, ServerToClientFailureBeforeWrite) {
  relay->shutdown_relays();
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  com::centreon::broker::grpc::stream client("127.0.0.1:" +
                                             std::to_string(relay_listen_port));
  std::unique_ptr<io::stream> accepted =
      s->open(std::chrono::milliseconds(100));

  test_param param = GetParam();
  log_v2::grpc()->debug("{} write param.data_type={}", __PRETTY_FUNCTION__,
                        param.data_type);
  accepted->write(create_event(param));
  std::shared_ptr<io::data> receive;
  bool read_ret = client.read(receive, time(nullptr) + 2);
  log_v2::grpc()->debug("{} read_ret={} param.data_type={}",
                        __PRETTY_FUNCTION__, read_ret, param.data_type);
  COMPARE_EVENT(read_ret, receive, param);

  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  relay->shutdown_relays();
  std::this_thread::sleep_for(std::chrono::milliseconds(5));

  ASSERT_THROW(client.read(receive, time(nullptr) + 2), msg_fmt);
  unsigned offset = rand();
  param.data_type += offset;
  param.src += offset;
  param.dest += offset;
  param.buffer += "_";
  param.buffer += std::to_string(offset);

  EXPECT_THROW(accepted->write(create_event(param)), msg_fmt);

  com::centreon::broker::grpc::stream client2(
      "127.0.0.1:" + std::to_string(relay_listen_port));
  accepted = s->open(std::chrono::milliseconds(100));
  accepted->write(create_event(param));
  read_ret = client2.read(receive, time(nullptr) + 2);
  log_v2::grpc()->debug("{} read_ret={} param.data_type={}",
                        __PRETTY_FUNCTION__, read_ret, param.data_type);
  COMPARE_EVENT(read_ret, receive, param);
}

TEST_P(grpc_comm_failure, ServerToClientFailureAfterWrite) {
  relay->shutdown_relays();
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  com::centreon::broker::grpc::stream client("127.0.0.1:" +
                                             std::to_string(relay_listen_port));
  std::unique_ptr<io::stream> accepted =
      s->open(std::chrono::milliseconds(100));

  test_param param = GetParam();
  log_v2::grpc()->debug("{} write param.data_type={}", __PRETTY_FUNCTION__,
                        param.data_type);
  accepted->write(create_event(param));
  std::shared_ptr<io::data> receive;
  bool read_ret = client.read(receive, time(nullptr) + 2);
  log_v2::grpc()->debug("{} read_ret={} param.data_type={}",
                        __PRETTY_FUNCTION__, read_ret, param.data_type);
  COMPARE_EVENT(read_ret, receive, param);

  unsigned offset = rand();
  param.data_type += offset;
  param.src += offset;
  param.dest += offset;
  param.buffer += "_";
  param.buffer += std::to_string(offset);

  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  relay->shutdown_relays();
  std::this_thread::sleep_for(std::chrono::milliseconds(5));

  EXPECT_THROW(accepted->write(create_event(param)), msg_fmt);
  ASSERT_THROW(client.read(receive, time(nullptr) + 2), msg_fmt);

  com::centreon::broker::grpc::stream client2(
      "127.0.0.1:" + std::to_string(relay_listen_port));
  accepted = s->open(std::chrono::milliseconds(100));
  accepted->write(create_event(param));
  read_ret = client2.read(receive, time(nullptr) + 2);
  log_v2::grpc()->debug("{} read_ret={} param.data_type={}",
                        __PRETTY_FUNCTION__, read_ret, param.data_type);
  COMPARE_EVENT(read_ret, receive, param);
}

TEST_P(grpc_comm_failure, ServerToClientFailureNoLostEvent) {
  relay->shutdown_relays();
  std::this_thread::sleep_for(std::chrono::seconds(1));
  std::unique_ptr<com::centreon::broker::grpc::stream> client(
      std::make_unique<com::centreon::broker::grpc::stream>(
          "127.0.0.1:" + std::to_string(relay_listen_port)));

  std::shared_ptr<io::data> receive;
  client->read(receive, std::chrono::milliseconds(100));
  std::unique_ptr<io::stream> accepted =
      s->open(std::chrono::milliseconds(100));
  unsigned volatile nb_written = 0;

  test_param param = GetParam();

  std::multiset<unsigned> emit, received;

  auto write_callback = [&emit, &nb_written](const event_ptr& written) {
    emit.insert(atoi(written->buffer().c_str()));
    ++nb_written;
    log_v2::grpc()->debug("{} nb_written={}", __PRETTY_FUNCTION__, nb_written);
  };

  if (accepted) {
    static_cast<com::centreon::broker::grpc::stream*>(accepted.get())
        ->set_write_callback(write_callback);
  }

  auto read = [&received, &client](unsigned milli_second_delay) {
    std::shared_ptr<io::data> receive;
    while (true) {
      try {
        bool read_ret = client->read(
            receive, std::chrono::milliseconds(milli_second_delay));
        log_v2::grpc()->debug("{} read_ret={}", __PRETTY_FUNCTION__, read_ret);
        if (read_ret) {
          std::string s(
              std::static_pointer_cast<io::raw>(receive)->get_buffer().begin(),
              std::static_pointer_cast<io::raw>(receive)->get_buffer().end());
          received.insert(atoi(s.c_str()));
        } else {
          break;
        }
      } catch (const std::exception&) {
        client = std::make_unique<com::centreon::broker::grpc::stream>(
            "127.0.0.1:" + std::to_string(relay_listen_port));
      }
    }
  };

  auto create_param = [this, &param, &nb_written]() {
    param = GetParam();
    param.data_type = nb_written;
    param.src += nb_written;
    param.dest += nb_written;
    param.buffer = std::to_string(nb_written);
  };

  auto write = [this, &accepted, &nb_written, create_param, &param, &emit,
                &write_callback]() {
    try {
      if (!accepted) {
        accepted = s->open(std::chrono::milliseconds(100));
      }
      if (accepted) {
        static_cast<com::centreon::broker::grpc::stream*>(accepted.get())
            ->set_write_callback(write_callback);
        create_param();
        accepted->write(create_event(param));
        log_v2::grpc()->debug("{} write param.data_type={}",
                              __PRETTY_FUNCTION__, param.data_type);
      }
    } catch (const std::exception&) {
      accepted->stop();
      accepted = s->open(std::chrono::milliseconds(100));
      if (accepted) {
        static_cast<com::centreon::broker::grpc::stream*>(accepted.get())
            ->set_write_callback(write_callback);
        try {
          create_param();
          accepted->write(create_event(param));
          log_v2::grpc()->debug("{} write param.data_type={}",
                                __PRETTY_FUNCTION__, param.data_type);

        } catch (const std::exception&) {
          accepted->stop();
          accepted.reset();
        }
      }
    }
  };

  for (nb_written = 0; nb_written < 25;) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    write();
    std::this_thread::sleep_for(std::chrono::milliseconds(1 + rand() % 100));
    relay->shutdown_relays();
    std::this_thread::sleep_for(std::chrono::milliseconds(1 + rand() % 100));
    read(10);
  }
  read(100);
  EXPECT_EQ(emit.size(), received.size());
  ASSERT_EQ(emit, received);
}

TEST_P(grpc_comm_failure, ClientToServerFailureNoLostEvent) {
  relay->shutdown_relays();
  std::this_thread::sleep_for(std::chrono::seconds(1));
  std::unique_ptr<com::centreon::broker::grpc::stream> client(
      std::make_unique<com::centreon::broker::grpc::stream>(
          "127.0.0.1:" + std::to_string(relay_listen_port)));

  std::unique_ptr<io::stream> accepted =
      s->open(std::chrono::milliseconds(100));

  if (accepted) {
    std::shared_ptr<io::data> receive;
    static_cast<com::centreon::broker::grpc::stream*>(accepted.get())
        ->read(receive, std::chrono::milliseconds(100));
  }
  unsigned volatile nb_written = 0;

  test_param param = GetParam();

  std::multiset<unsigned> emit, received;

  auto write_callback = [&emit, &nb_written](const event_ptr& written) {
    emit.insert(atoi(written->buffer().c_str()));
    ++nb_written;
    log_v2::grpc()->debug("{} nb_written={}", __PRETTY_FUNCTION__, nb_written);
  };

  client->set_write_callback(write_callback);

  auto read = [&received, &accepted](unsigned milli_second_delay) {
    std::shared_ptr<io::data> receive;
    while (true) {
      if (!accepted) {
        accepted = s->open(std::chrono::milliseconds(100));
      }
      if (!accepted) {
        break;
      }
      try {
        bool read_ret =
            static_cast<com::centreon::broker::grpc::stream*>(accepted.get())
                ->read(receive, std::chrono::milliseconds(milli_second_delay));
        log_v2::grpc()->debug("{} read_ret={}", __PRETTY_FUNCTION__, read_ret);
        if (read_ret) {
          std::string s(
              std::static_pointer_cast<io::raw>(receive)->get_buffer().begin(),
              std::static_pointer_cast<io::raw>(receive)->get_buffer().end());
          received.insert(atoi(s.c_str()));
        } else {
          break;
        }
      } catch (const std::exception&) {
        accepted = s->open(std::chrono::milliseconds(100));
      }
    }
  };

  auto create_param = [this, &param, &nb_written]() {
    param = GetParam();
    param.data_type = nb_written;
    param.src += nb_written;
    param.dest += nb_written;
    param.buffer = std::to_string(nb_written);
  };

  auto write = [this, &client, &nb_written, create_param, &param,
                &write_callback]() {
    client->flush();
    try {
      if (!client) {
        client = std::make_unique<com::centreon::broker::grpc::stream>(
            "127.0.0.1:" + std::to_string(relay_listen_port));
        client->set_write_callback(write_callback);
      }
      create_param();
      client->write(create_event(param));
      log_v2::grpc()->debug("{} write param.data_type={}", __PRETTY_FUNCTION__,
                            param.data_type);
    } catch (const std::exception&) {
      client->stop();
      client = std::make_unique<com::centreon::broker::grpc::stream>(
          "127.0.0.1:" + std::to_string(relay_listen_port));
      client->set_write_callback(write_callback);
      try {
        create_param();
        client->write(create_event(param));
        log_v2::grpc()->debug("{} write param.data_type={}",
                              __PRETTY_FUNCTION__, param.data_type);
      } catch (const std::exception&) {
        client->stop();
        client.reset();
      }
    }
  };

  for (nb_written = 0; nb_written < 25;) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    write();
    std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 100));
    relay->shutdown_relays();
    std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 100));
    read(10);
  }
  read(100);
  EXPECT_EQ(emit.size(), received.size());
  ASSERT_EQ(emit, received);
}
