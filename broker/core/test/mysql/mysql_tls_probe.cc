/**
 * Copyright 2026 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */
#include "com/centreon/broker/sql/mysql_tls_probe.hh"

#include <gtest/gtest.h>
#include <mysql.h>

#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker::database;
using log_v2 = com::centreon::common::log_v2::log_v2;

namespace {

/**
 * @brief Build a Handshake v10 greeting packet with the given capabilities.
 */
std::vector<unsigned char> greeting(uint16_t capabilities) {
  std::vector<unsigned char> payload{10};
  const std::string version{"10.11.14-MariaDB"};
  payload.insert(payload.end(), version.begin(), version.end());
  payload.push_back(0);
  // thread id (4), scramble (8), filler (1)
  payload.insert(payload.end(), 4 + 8 + 1, 0);
  payload.push_back(capabilities & 0xff);
  payload.push_back(capabilities >> 8);

  std::vector<unsigned char> packet{static_cast<unsigned char>(payload.size()),
                                    0, 0, 0};
  packet.insert(packet.end(), payload.begin(), payload.end());
  return packet;
}

/**
 * @brief Local server that sends one packet to the first client and keeps the
 * connection open until it is destroyed.
 */
class greeting_server {
  asio::io_context _io_context;
  asio::ip::tcp::acceptor _acceptor;
  asio::ip::tcp::socket _socket;
  const std::vector<unsigned char> _packet;
  /* Read before the thread starts: the acceptor then belongs to it. */
  const uint16_t _port;
  std::thread _thread;

 public:
  explicit greeting_server(std::vector<unsigned char> packet)
      : _acceptor{_io_context, {asio::ip::address_v4::loopback(), 0}},
        _socket{_io_context},
        _packet{std::move(packet)},
        _port{_acceptor.local_endpoint().port()} {
    _acceptor.async_accept(
        _socket, [this](const boost::system::error_code& err) {
          if (!err)
            asio::async_write(_socket, asio::buffer(_packet),
                              [](const boost::system::error_code&, size_t) {});
        });
    _thread =
        std::thread([this] { _io_context.run_for(std::chrono::seconds(30)); });
  }

  ~greeting_server() {
    _io_context.stop();
    _thread.join();
  }

  uint16_t port() const { return _port; }
};

constexpr std::chrono::seconds probe_timeout{10};

}  // namespace

TEST(mysql_tls_probe_test, server_with_tls) {
  greeting_server server{greeting(CLIENT_PROTOCOL_41 | CLIENT_SSL)};
  std::optional<bool> result =
      server_advertises_tls("127.0.0.1", server.port(), probe_timeout,
                            log_v2::instance().get(log_v2::SQL));
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(*result);
}

TEST(mysql_tls_probe_test, server_without_tls) {
  greeting_server server{greeting(CLIENT_PROTOCOL_41)};
  std::optional<bool> result =
      server_advertises_tls("127.0.0.1", server.port(), probe_timeout,
                            log_v2::instance().get(log_v2::SQL));
  ASSERT_TRUE(result.has_value());
  ASSERT_FALSE(*result);
}

TEST(mysql_tls_probe_test, error_packet_is_left_to_libmariadb) {
  // ER_HOST_IS_BLOCKED, as sent instead of a greeting.
  std::vector<unsigned char> packet{8,    0,   0,   0,   0xff, 0x69,
                                    0x04, 'b', 'l', 'o', 'c',  'k'};
  greeting_server server{packet};
  std::optional<bool> result =
      server_advertises_tls("127.0.0.1", server.port(), probe_timeout,
                            log_v2::instance().get(log_v2::SQL));
  ASSERT_FALSE(result.has_value());
}

/* The kernel accepts the connection on a listening socket, but nothing is
 * ever sent, as with a server stuck before its greeting. The probe must give
 * up after its timeout. */
TEST(mysql_tls_probe_test, silent_server_times_out) {
  asio::io_context io_context;
  asio::ip::tcp::acceptor acceptor{io_context,
                                   {asio::ip::address_v4::loopback(), 0}};
  auto start = std::chrono::steady_clock::now();
  std::optional<bool> result = server_advertises_tls(
      "127.0.0.1", acceptor.local_endpoint().port(),
      std::chrono::milliseconds(200), log_v2::instance().get(log_v2::SQL));
  auto elapsed = std::chrono::steady_clock::now() - start;
  ASSERT_FALSE(result.has_value());
  ASSERT_GE(elapsed, std::chrono::milliseconds(200));
  ASSERT_LT(elapsed, std::chrono::seconds(5));
}
