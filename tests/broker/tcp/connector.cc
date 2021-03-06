/*
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

#include "com/centreon/broker/tcp/connector.hh"
#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include "../test_server.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "com/centreon/broker/io/raw.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;

constexpr static char test_addr[] = "127.0.0.1";
constexpr static uint16_t test_port(4242);

class TcpConnector : public testing::Test {
 public:
  void SetUp() override {
    _server.init();
    _thread = std::thread(&test_server::run, &_server);

    _server.wait_for_init();
  }
  void TearDown() override {
    _server.stop();
    _thread.join();
  }

  test_server _server;
  std::thread _thread;
};

TEST_F(TcpConnector, InvalidHost) {
  tcp::connector connector;

  connector.connect_to("htrekf';kfdsa'", test_port);
  ASSERT_THROW(connector.open(), msg_fmt);
}

TEST_F(TcpConnector, NoConnection) {
  tcp::connector connector;

  connector.connect_to(test_addr, 2);
  ASSERT_THROW(connector.open(), msg_fmt);
}

TEST_F(TcpConnector, Timeout) {
  tcp::connector connector;

  connector.connect_to(test_addr, test_port);
  connector.set_read_timeout(1);
  std::shared_ptr<io::stream> io{connector.open()};

  std::shared_ptr<io::data> data{new io::raw()};

  ASSERT_FALSE(io->read(data, time(NULL) + 1));
}

TEST_F(TcpConnector, Simple) {
  tcp::connector connector;

  connector.connect_to(test_addr, test_port);
  connector.set_read_timeout(-1);
  std::shared_ptr<io::stream> io{connector.open()};

  std::shared_ptr<io::raw> data{new io::raw()};
  std::shared_ptr<io::data> data2{new io::raw()};

  data->append("PING\n");
  io->write(data);

  ASSERT_TRUE(io->peer() == "tcp://127.0.0.1:4242");
  ASSERT_TRUE(io->read(data2, ::time(nullptr) + 5));

  std::vector<char> vec{std::static_pointer_cast<io::raw>(data2)->get_buffer()};
  std::string str{vec.begin(), vec.end()};
  ASSERT_TRUE(str == "PONG\n");
}

TEST_F(TcpConnector, ReadAfterTimeout) {
  tcp::connector connector;

  connector.connect_to(test_addr, test_port);
  connector.set_read_timeout(-1);
  std::shared_ptr<io::stream> io{connector.open()};

  std::shared_ptr<io::raw> data{new io::raw()};
  std::shared_ptr<io::data> data2{new io::raw()};

  data->append("SERV_DELAY\n");
  io->write(data);

  ASSERT_FALSE(io->read(data2, ::time(nullptr) + 1));
  while (!io->read(data2, ::time(nullptr) + 1))
    ;

  std::vector<char> vec{std::static_pointer_cast<io::raw>(data2)->get_buffer()};
  std::string str{vec.begin(), vec.end()};
  ASSERT_TRUE(str == "PONG\n");
}

TEST_F(TcpConnector, MultipleSimple) {
  tcp::connector connector;

  connector.connect_to(test_addr, test_port);
  connector.set_read_timeout(-1);
  std::shared_ptr<io::stream> io{connector.open()};

  std::shared_ptr<io::raw> data{new io::raw()};
  std::shared_ptr<io::data> data2{new io::raw()};

  data->append("PING\n");
  io->write(data);

  ASSERT_TRUE(io->read(data2, ::time(nullptr) + 5));
  {
    std::vector<char> vec{
        std::static_pointer_cast<io::raw>(data2)->get_buffer()};
    std::string str{vec.begin(), vec.end()};
    ASSERT_TRUE(str == "PONG\n");
  }

  io->write(data);
  ASSERT_TRUE(io->read(data2, ::time(nullptr) + 5));
  {
    std::vector<char> vec{
        std::static_pointer_cast<io::raw>(data2)->get_buffer()};
    std::string str{vec.begin(), vec.end()};
    ASSERT_TRUE(str == "PONG\n");
  }

  io->write(data);
  ASSERT_TRUE(io->read(data2, ::time(nullptr) + 5));
  {
    std::vector<char> vec{
        std::static_pointer_cast<io::raw>(data2)->get_buffer()};
    std::string str{vec.begin(), vec.end()};
    ASSERT_TRUE(str == "PONG\n");
  }
}
