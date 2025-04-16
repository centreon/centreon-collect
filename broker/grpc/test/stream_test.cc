/**
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

#include "grpc_stream.grpc.pb.h"

#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/io/protocols.hh"
#include "common/log_v2/log_v2.hh"
#include "grpc_test_include.hh"
#include "tcp_relais.hh"

using namespace com::centreon::broker;
using namespace com::centreon::exceptions;
using com::centreon::common::log_v2::log_v2;

com::centreon::broker::grpc::grpc_config::pointer conf(
    std::make_shared<com::centreon::broker::grpc::grpc_config>("127.0.0.1:4444",
                                                               false,
                                                               "",
                                                               "",
                                                               "",
                                                               "my_aut",
                                                               "",
                                                               false,
                                                               30,
                                                               false));

static constexpr unsigned relay_listen_port = 5123u;
static constexpr unsigned server_listen_port = 5124u;

com::centreon::broker::grpc::grpc_config::pointer conf_relay_in(
    std::make_shared<com::centreon::broker::grpc::grpc_config>(
        "127.0.0.1:5123"));

com::centreon::broker::grpc::grpc_config::pointer conf_relay_out(
    std::make_shared<com::centreon::broker::grpc::grpc_config>(
        "127.0.0.1:5124"));

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
  static std::shared_ptr<spdlog::logger> _logger;

 public:
  static void SetUpTestSuite() {
    _logger = log_v2::instance().get(log_v2::GRPC);
    //_logger->set_level(spdlog::level::trace);
    s = std::make_unique<com::centreon::broker::grpc::acceptor>(conf);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    io::protocols::load();
    io::events::load();
  }
  static void TearDownTestSuite() {
    s.reset();
    std::this_thread::sleep_for(std::chrono::seconds(1));
  };

 protected:
  void SetUp() override {}

  void TearDown() override {}
};

std::unique_ptr<com::centreon::broker::grpc::acceptor> grpc_test_server::s;
std::shared_ptr<spdlog::logger> grpc_test_server::_logger;

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
  com::centreon::broker::grpc::connector conn(conf);
  std::shared_ptr<io::stream> client = conn.open();
  std::shared_ptr<io::stream> accepted = s->open();
  ASSERT_NE(accepted.get(), nullptr);

  for (unsigned test_ind = 0; test_ind < 100; ++test_ind) {
    test_param param = GetParam();
    param.data_type += test_ind;
    param.src += test_ind;
    param.dest += test_ind;
    param.buffer += "_";
    param.buffer += std::to_string(test_ind);
    _logger->debug("{} write param.data_type={}", __PRETTY_FUNCTION__,
                   param.data_type);
    client->write(create_event(param));

    std::shared_ptr<io::data> receive;
    bool read_ret = accepted->read(receive, time(nullptr) + 2);
    _logger->debug("{} read_ret={} param.data_type={}", __PRETTY_FUNCTION__,
                   read_ret, param.data_type);
    COMPARE_EVENT(read_ret, receive, param);
  }
  client->stop();
  accepted->stop();
}

INSTANTIATE_TEST_SUITE_P(grpc_test_server,
                         grpc_test_server,
                         ::testing::ValuesIn(tests_feed));

TEST_P(grpc_test_server, ServerToClientSendReceive) {
  com::centreon::broker::grpc::connector conn(conf);
  std::shared_ptr<io::stream> client = conn.open();
  std::shared_ptr<io::stream> accepted = s->open();
  ASSERT_NE(accepted.get(), nullptr);

  for (unsigned test_ind = 0; test_ind < 100; ++test_ind) {
    test_param param = GetParam();
    param.data_type += test_ind;
    param.src += test_ind;
    param.dest += test_ind;
    param.buffer += "_";
    param.buffer += std::to_string(test_ind);
    _logger->debug("{} write param.data_type={}", __PRETTY_FUNCTION__,
                   param.data_type);
    accepted->write(create_event(param));

    std::shared_ptr<io::data> receive;
    bool read_ret = client->read(receive, time(nullptr) + 2);
    _logger->debug("{} read_ret={} param.data_type={}", __PRETTY_FUNCTION__,
                   read_ret, param.data_type);
    COMPARE_EVENT(read_ret, receive, param);
  }
  client->stop();
  accepted->stop();
}

class grpc_comm_failure : public ::testing::TestWithParam<test_param> {
 protected:
  static std::unique_ptr<com::centreon::broker::grpc::acceptor> s;
  static std::unique_ptr<test_util::tcp_relais> relay;
  static std::shared_ptr<spdlog::logger> _logger;

 public:
  static void SetUpTestSuite() {
    srand(time(nullptr));
    _logger = log_v2::instance().get(log_v2::GRPC);
    //_logger->set_level(spdlog::level::trace);
    s = std::make_unique<com::centreon::broker::grpc::acceptor>(conf_relay_out);
    relay = std::make_unique<test_util::tcp_relais>(
        "127.0.0.1", relay_listen_port, "127.0.0.1", server_listen_port);
  }
  static void TearDownTestSuite() {
    s.reset();
    relay.reset();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  };

 protected:
  void SetUp() override {}

  void TearDown() override {}
};

std::unique_ptr<com::centreon::broker::grpc::acceptor> grpc_comm_failure::s;
std::unique_ptr<test_util::tcp_relais> grpc_comm_failure::relay;
std::shared_ptr<spdlog::logger> grpc_comm_failure::_logger;

INSTANTIATE_TEST_SUITE_P(grpc_comm_failure,
                         grpc_comm_failure,
                         ::testing::ValuesIn(tests_feed));

TEST_P(grpc_comm_failure, ClientToServerFailureBeforeWrite) {
  com::centreon::broker::grpc::connector conn(conf_relay_in);
  std::shared_ptr<io::stream> client = conn.open();
  std::shared_ptr<io::stream> accepted = s->open();
  ASSERT_NE(accepted.get(), nullptr);

  test_param param = GetParam();
  _logger->debug("{} write param.data_type={}", __PRETTY_FUNCTION__,
                 param.data_type);
  client->write(create_event(GetParam()));
  std::shared_ptr<io::data> receive;
  bool read_ret = accepted->read(receive, time(nullptr) + 2);
  _logger->debug("{} read_ret={} param.data_type={}", __PRETTY_FUNCTION__,
                 read_ret, param.data_type);
  COMPARE_EVENT(read_ret, receive, param);

  relay->shutdown_relays();

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_THROW(client->write(create_event(GetParam())), msg_fmt);
  client->stop();
  accepted->stop();
}

TEST_P(grpc_comm_failure, ClientToServerFailureAfterWrite) {
  com::centreon::broker::grpc::connector conn(conf_relay_in);
  std::shared_ptr<io::stream> client = conn.open();
  std::shared_ptr<io::stream> accepted = s->open();
  ASSERT_NE(accepted.get(), nullptr);

  test_param param = GetParam();
  _logger->debug("{} write param.data_type={}", __PRETTY_FUNCTION__,
                 param.data_type);
  client->write(create_event(param));
  std::shared_ptr<io::data> receive;
  bool read_ret = accepted->read(receive, time(nullptr) + 2);
  _logger->debug("{} read_ret={} param.data_type={}", __PRETTY_FUNCTION__,
                 read_ret, param.data_type);
  COMPARE_EVENT(read_ret, receive, param);

  unsigned offset = rand();
  param.data_type += offset;
  param.src += offset;
  param.dest += offset;
  param.buffer += "_";
  param.buffer += std::to_string(offset);
  client->write(create_event(param));
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  relay->shutdown_relays();
  std::this_thread::sleep_for(std::chrono::milliseconds(5));

  read_ret = accepted->read(receive, time(nullptr) + 2);
  COMPARE_EVENT(read_ret, receive, param);
  client->stop();
  accepted->stop();
}

TEST_P(grpc_comm_failure, ServerToClientFailureBeforeWrite) {
  relay->shutdown_relays();
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  com::centreon::broker::grpc::connector conn(conf_relay_in);
  std::shared_ptr<io::stream> client = conn.open();
  std::shared_ptr<io::stream> accepted = s->open();
  ASSERT_NE(accepted.get(), nullptr);

  test_param param = GetParam();
  _logger->debug("{} write param.data_type={}", __PRETTY_FUNCTION__,
                 param.data_type);
  accepted->write(create_event(param));
  std::shared_ptr<io::data> receive;
  bool read_ret = client->read(receive, time(nullptr) + 2);
  _logger->debug("{} read_ret={} param.data_type={}", __PRETTY_FUNCTION__,
                 read_ret, param.data_type);
  COMPARE_EVENT(read_ret, receive, param);

  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  relay->shutdown_relays();
  std::this_thread::sleep_for(std::chrono::milliseconds(5));

  ASSERT_THROW(client->read(receive, time(nullptr) + 2), msg_fmt);
  unsigned offset = rand();
  param.data_type += offset;
  param.src += offset;
  param.dest += offset;
  param.buffer += "_";
  param.buffer += std::to_string(offset);

  EXPECT_THROW(accepted->write(create_event(param)), msg_fmt);

  std::shared_ptr<io::stream> client2 = conn.open();
  std::shared_ptr<io::stream> accepted2 = s->open();
  ASSERT_NE(accepted2.get(), nullptr);
  accepted2->write(create_event(param));
  read_ret = client2->read(receive, time(nullptr) + 2);
  SPDLOG_LOGGER_DEBUG(_logger, "{} read_ret={} param.data_type={}",
                      __PRETTY_FUNCTION__, read_ret, param.data_type);
  COMPARE_EVENT(read_ret, receive, param);
  client->stop();
  client2->stop();
  accepted->stop();
  accepted2->stop();
}

TEST_P(grpc_comm_failure, ServerToClientFailureAfterWrite) {
  relay->shutdown_relays();
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  com::centreon::broker::grpc::connector conn(conf_relay_in);
  std::shared_ptr<io::stream> client = conn.open();
  std::shared_ptr<io::stream> accepted = s->open();
  ASSERT_NE(accepted.get(), nullptr);

  test_param param = GetParam();
  _logger->debug("{} write param.data_type={}", __PRETTY_FUNCTION__,
                 param.data_type);
  accepted->write(create_event(param));
  std::shared_ptr<io::data> receive;
  bool read_ret = client->read(receive, time(nullptr) + 2);
  _logger->debug("{} read_ret={} param.data_type={}", __PRETTY_FUNCTION__,
                 read_ret, param.data_type);
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
  ASSERT_THROW(client->read(receive, time(nullptr) + 2), msg_fmt);

  std::shared_ptr<io::stream> client2 = conn.open();
  std::shared_ptr<io::stream> accepted2 = s->open();
  ASSERT_NE(accepted2.get(), nullptr);
  accepted2->write(create_event(param));
  read_ret = client2->read(receive, time(nullptr) + 2);
  SPDLOG_LOGGER_DEBUG(_logger, "{} read_ret={} param.data_type={}",
                      __PRETTY_FUNCTION__, read_ret, param.data_type);
  COMPARE_EVENT(read_ret, receive, param);
  client->stop();
  client2->stop();
  accepted->stop();
  accepted2->stop();
}

/*************************************
 * this part use keys generated with create_key.sh
 *
 *************************************/

static std::string read_file(const std::string& path) {
  std::ifstream file(path);
  std::stringstream ss;
  ss << file.rdbuf();
  file.close();
  return ss.str();
}

com::centreon::broker::grpc::grpc_config::pointer conf_crypted_server1234(
    std::make_shared<com::centreon::broker::grpc::grpc_config>(
        "localhost:4446",
        true,
        read_file("tests/grpc_test_keys/server_1234.crt"),
        read_file("tests/grpc_test_keys/server_1234.key"),
        read_file("tests/grpc_test_keys/ca_1234.crt"),
        "my_auth",
        "",
        false,
        30,
        false));

com::centreon::broker::grpc::grpc_config::pointer conf_crypted_client1234(
    std::make_shared<com::centreon::broker::grpc::grpc_config>(
        "localhost:4446",
        true,
        read_file("tests/grpc_test_keys/client_1234.crt"),
        read_file("tests/grpc_test_keys/client_1234.key"),
        read_file("tests/grpc_test_keys/ca_1234.crt"),
        "my_auth",
        "",
        false,
        30,
        false));

class grpc_test_server_crypted : public ::testing::TestWithParam<test_param> {
 protected:
  static std::unique_ptr<com::centreon::broker::grpc::acceptor> s;
  static std::shared_ptr<spdlog::logger> _logger;

 public:
  static void SetUpTestSuite() {
    _logger = log_v2::instance().get(log_v2::GRPC);
    //_logger->set_level(spdlog::level::trace);
    s = std::make_unique<com::centreon::broker::grpc::acceptor>(
        conf_crypted_server1234);
  }
  static void TearDownTestSuite() { s.reset(); };

 protected:
  void SetUp() override {}

  void TearDown() override {}
};

std::unique_ptr<com::centreon::broker::grpc::acceptor>
    grpc_test_server_crypted::s;
std::shared_ptr<spdlog::logger> grpc_test_server_crypted::_logger;

INSTANTIATE_TEST_SUITE_P(grpc_test_server_crypted,
                         grpc_test_server_crypted,
                         ::testing::ValuesIn(tests_feed));

TEST_P(grpc_test_server_crypted, ServerToClientWithKeySendReceive) {
  com::centreon::broker::grpc::connector conn(conf_crypted_client1234);
  std::shared_ptr<io::stream> client = conn.open();
  std::shared_ptr<io::stream> accepted = s->open();
  ASSERT_NE(accepted.get(), nullptr);

  for (unsigned test_ind = 0; test_ind < 100; ++test_ind) {
    test_param param = GetParam();
    param.data_type += test_ind;
    param.src += test_ind;
    param.dest += test_ind;
    param.buffer += "_";
    param.buffer += std::to_string(test_ind);
    SPDLOG_LOGGER_DEBUG(_logger, "{} write param.data_type={}",
                        __PRETTY_FUNCTION__, param.data_type);
    accepted->write(create_event(param));

    std::shared_ptr<io::data> receive;
    bool read_ret = client->read(receive, time(nullptr) + 2);
    SPDLOG_LOGGER_DEBUG(_logger, "{} read_ret={} param.data_type={}",
                        __PRETTY_FUNCTION__, read_ret, param.data_type);
    COMPARE_EVENT(read_ret, receive, param);
  }
  client->stop();
  accepted->stop();
}

com::centreon::broker::grpc::grpc_config::pointer
    conf_crypted_client1234_bad_auth(
        std::make_shared<com::centreon::broker::grpc::grpc_config>(
            "localhost:4446",
            true,
            read_file("tests/grpc_test_keys/client_1234.crt"),
            read_file("tests/grpc_test_keys/client_1234.key"),
            read_file("tests/grpc_test_keys/ca_1234.crt"),
            "my_auth_pasbon",
            "",
            false,
            30,
            false));

TEST_P(grpc_test_server_crypted, ServerToClientWithKeyAndBadAuthorization) {
  com::centreon::broker::grpc::connector conn(conf_crypted_client1234_bad_auth);
  std::shared_ptr<io::stream> client = conn.open();
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  ASSERT_FALSE(s->is_ready());
  client->stop();
}
