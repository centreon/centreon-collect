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

#include "test_server.hh"

#include <gtest/gtest.h>

static uint32_t const timeout_ms = 5;
static uint32_t const buff_size = 1024;

test_server::test_server()
    : _initialised{false},
      _ctx{nullptr},
      _acceptor{nullptr},
      _connections{},
      _num_connections{0},
      _bind_ok{false} {
  _answer_reply.insert({"PING\n", "PONG\n"});
  _answer_reply.insert(
      {"HEAD /centreon?pretty HTTP/1.1\\r\\nHost: "
       "127.0.0.1:9200\\r\\nAccept: */*\\r\\n\\r\\n",
       "HTTP/1.1 200 OK"});
  _answer_reply.insert(
      {"PUT /centreon/_mapping/metrics?pretty "
       "HTTP/1.1\\r\\nHost: 127.0.0.1:9200\\r\\n"
       "Accept: */*\\r\\nContent-Type: "
       "application/json\\r\\n'",
       "HTTP/1.1 200 OK"});
  _answer_reply.insert(
      {"POST /write?u=centreon&p=pass&db=centreon&precision=s HTTP/1.0",
       "HTTP/1.0 204 No Content\n"});
  _answer_reply.insert(
      {"POST /write?u=centreon&p=fail1&db=centreon&precision=s HTTP/1.0",
       "HTTP/1.1 204 OK\n"});
  _answer_reply.insert(
      {"POST /write?u=centreon&p=fail2&db=centreon&precision=s HTTP/1.0",
       "HTTP/1.1 200\n"});
}

auto wait_for_connections = [](test_server& server, uint32_t nb) -> void {
  for (int i = 0; i < 100 && server.get_num_connections() != nb; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
  }
};

void test_server::init() {
  _ctx.reset(new asio::io_context());
  _acceptor.reset(new asio::ip::tcp::acceptor(*_ctx));
}

void test_server::run() {
  asio::ip::tcp::endpoint ep =
      asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 4242);
  _acceptor->open(ep.protocol());
  _acceptor->set_option(asio::ip::tcp::acceptor::reuse_address(true));
  try {
    _acceptor->bind(ep);
    _bind_ok = true;
  } catch (boost::system::system_error const& se) {
    std::cout << "bind error: " << se.what() << std::endl;
    _bind_ok = false;
    std::unique_lock<std::mutex> lock(_m_init);
    _initialised = true;
    lock.unlock();
    _cond_init.notify_all();
    return;
  }
  _acceptor->listen();
  start_accept();

  std::unique_lock<std::mutex> lock(_m_init);
  _initialised = true;
  _cond_init.notify_all();
  lock.unlock();

  _ctx->run();
}

void test_server::start_accept() {
  std::list<test_server_connection>::iterator con_handle =
      _connections.emplace(_connections.begin(), *_ctx, buff_size);
  auto handler = std::bind(&test_server::handle_accept, this, con_handle,
                           std::placeholders::_1);
  _acceptor->async_accept(con_handle->socket, handler);
}

void test_server::handle_accept(
    std::list<test_server_connection>::iterator con_handle,
    boost::system::error_code const& err) {
  if (!err) {
    ++_num_connections;
    std::cout << "Connection from: "
              << con_handle->socket.remote_endpoint().address().to_string()
              << "\n";
    start_read(con_handle);
  } else {
    _connections.erase(con_handle);
  }
  start_accept();
}

void test_server::start_read(std::list<test_server_connection>::iterator& con) {
  auto handler = std::bind(&test_server::handle_read, this, con,
                           std::placeholders::_1, std::placeholders::_2);
  con->socket.async_read_some(asio::buffer(con->buf, buff_size), handler);
}

void test_server::handle_read(
    std::list<test_server_connection>::iterator con_handle,
    boost::system::error_code const& err,
    size_t bytes_transfered) {
  if (bytes_transfered > 0) {
    boost::system::error_code err;

    bool key_found{false};
    con_handle->buf[bytes_transfered] = 0;
    std::string const& s{con_handle->buf};
    for (auto it(_answer_reply.begin()), end(_answer_reply.end()); it != end;
         ++it) {
      if (s.find(it->first) != std::string::npos) {
        asio::write(con_handle->socket, asio::buffer(it->second),
                    asio::transfer_all(), err);
        key_found = true;
        break;
      }
    }

    if (!key_found) {
      if (s.find("SERV_DELAY\n") != std::string::npos) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1050));
        asio::write(con_handle->socket, asio::buffer(std::string{"PONG\n"}),
                    asio::transfer_all(), err);
      } else {
        // unknow command mirror the command
        std::string const& s{con_handle->buf};
        asio::write(con_handle->socket, asio::buffer(s), asio::transfer_all(),
                    err);
      }
    }
  }

  if (!err) {
    start_read(con_handle);
  } else {
    if (err.value() != ENOENT)
      std::cout << "read error: " << err.message() << err.value() << std::endl;
    --_num_connections;
    _connections.erase(con_handle);
  }
}

void test_server::wait_for_init() {
  std::unique_lock<std::mutex> lock{_m_init};
  _cond_init.wait(lock, [&]() { return _initialised; });
}

bool test_server::add_client(asio::ip::tcp::socket& sock,
                             asio::io_context& io) {
  asio::ip::tcp::resolver resolver{io};
  boost::system::error_code err;
  auto endpoints = resolver.resolve("localhost", std::to_string(4242), err);
  if (err) {
    std::cout << "resolve error: " << err.message() << std::endl;
    return false;
  }
  asio::connect(sock, endpoints, err);
  if (err) {
    std::cout << "connect error: " << err.message() << std::endl;
    return false;
  }
  return true;
}

class AsioTest : public ::testing::Test {
 public:
  void SetUp() override {
    buf = new char[buff_size];

    std::thread t{[&] {
      _server.init();
      _server.run();
    }};

    _thread = std::move(t);

    _server.wait_for_init();
  }
  void TearDown() override {
    _server.stop();
    _thread.join();

    delete[] buf;
  }

  test_server _server;
  std::thread _thread;
  char* buf;
};

TEST_F(AsioTest, Ping) {
  ASSERT_TRUE(_server.get_bind_ok());
  asio::io_context io;
  asio::ip::tcp::socket s1{io}, s2{io};

  ASSERT_EQ(_server.get_num_connections(), 0u);
  ASSERT_TRUE(_server.add_client(s1, io));
  std::this_thread::sleep_for(std::chrono::milliseconds{timeout_ms});
  ASSERT_EQ(_server.get_num_connections(), 1u);
  ASSERT_TRUE(_server.add_client(s2, io));
  wait_for_connections(_server, 2);
  ASSERT_EQ(_server.get_num_connections(), 2u);

  boost::system::error_code err;
  asio::write(s1, asio::buffer(std::string{"PING\n"}), asio::transfer_all(),
              err);
  ASSERT_FALSE(err);
  size_t len = s1.read_some(asio::buffer(buf, buff_size), err);
  buf[len] = '\0';
  ASSERT_FALSE(err);
  ASSERT_TRUE(std::string{buf}.find("PONG") != std::string::npos);
  memset(buf, 0, len);

  asio::write(s2, asio::buffer(std::string{"PING\n"}), asio::transfer_all(),
              err);
  ASSERT_FALSE(err);
  s2.read_some(asio::buffer(buf, buff_size), err);
  buf[len] = '\0';
  ASSERT_TRUE(std::string{buf}.find("PONG") != std::string::npos);

  s1.close();
  std::this_thread::sleep_for(std::chrono::milliseconds{timeout_ms});
  ASSERT_EQ(_server.get_num_connections(), 1u);
  s2.close();
  std::this_thread::sleep_for(std::chrono::milliseconds{timeout_ms});
  wait_for_connections(_server, 0);
  ASSERT_EQ(_server.get_num_connections(), 0u);
}
