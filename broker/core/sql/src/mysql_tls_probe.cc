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

#include <mysql.h>

#include "com/centreon/common/pool.hh"

using namespace com::centreon::broker::database;

namespace {

/**
 * @brief Read the capabilities of a Handshake v10 payload: protocol version
 * (1), server version (NUL-terminated), thread id (4), scramble (8), filler
 * (1), capabilities (2).
 *
 * @return Whether the CLIENT_SSL capability is set, or std::nullopt for
 * anything else, as an error packet.
 */
std::optional<bool> parse_greeting(const unsigned char* payload, size_t size) {
  if (size < 1 || payload[0] != 10)
    return std::nullopt;
  const unsigned char* version_end =
      static_cast<const unsigned char*>(memchr(payload + 1, 0, size - 1));
  if (!version_end)
    return std::nullopt;
  size_t caps_offset = version_end - payload + 1 + 4 + 8 + 1;
  if (caps_offset + 2 > size)
    return std::nullopt;
  uint16_t caps = payload[caps_offset] | payload[caps_offset + 1] << 8;
  return (caps & CLIENT_SSL) != 0;
}

/**
 * @brief Reads the greeting of a database server on the shared pool. Its
 * handlers run on one strand, so that cancel() can close the socket while an
 * operation is pending.
 */
class tls_probe : public std::enable_shared_from_this<tls_probe> {
  asio::strand<asio::io_context::executor_type> _strand;
  asio::ip::tcp::resolver _resolver;
  asio::ip::tcp::socket _socket;
  const std::string _host;
  const uint16_t _port;
  std::shared_ptr<spdlog::logger> _logger;
  std::array<unsigned char, 4> _header;
  std::array<unsigned char, 256> _payload;
  std::promise<std::optional<bool>> _result;
  bool _done = false;

  void _resolve();
  void _connect(const boost::system::error_code& err,
                const asio::ip::tcp::resolver::results_type& endpoints);
  void _read_header(const boost::system::error_code& err);
  void _read_payload(const boost::system::error_code& err);
  void _parse(const boost::system::error_code& err, size_t size);
  void _fail(const std::string& reason);
  void _finish(std::optional<bool> result);

 public:
  tls_probe(asio::io_context& io_context,
            const std::string& host,
            uint16_t port,
            const std::shared_ptr<spdlog::logger>& logger);
  std::future<std::optional<bool>> start();
  void cancel();
};

tls_probe::tls_probe(asio::io_context& io_context,
                     const std::string& host,
                     uint16_t port,
                     const std::shared_ptr<spdlog::logger>& logger)
    : _strand{asio::make_strand(io_context)},
      _resolver{_strand},
      _socket{_strand},
      _host{host},
      _port{port},
      _logger{logger} {}

/**
 * @brief Start the probe on the strand.
 *
 * @return The future result of the probe, see server_advertises_tls().
 */
std::future<std::optional<bool>> tls_probe::start() {
  std::future<std::optional<bool>> result = _result.get_future();
  asio::post(_strand, [me = shared_from_this()] { me->_resolve(); });
  return result;
}

/**
 * @brief Stop the probe, its pending handler is called with an error and
 * does nothing.
 */
void tls_probe::cancel() {
  asio::post(_strand, [me = shared_from_this()] {
    me->_resolver.cancel();
    me->_finish(std::nullopt);
  });
}

void tls_probe::_resolve() {
  try {
    _resolver.async_resolve(
        _host, std::to_string(_port),
        [me = shared_from_this()](
            const boost::system::error_code& err,
            const asio::ip::tcp::resolver::results_type& endpoints) {
          me->_connect(err, endpoints);
        });
  } catch (const std::exception& e) {
    _fail(e.what());
  }
}

void tls_probe::_connect(
    const boost::system::error_code& err,
    const asio::ip::tcp::resolver::results_type& endpoints) {
  if (err) {
    _fail(err.message());
    return;
  }
  try {
    asio::async_connect(
        _socket, endpoints,
        [me = shared_from_this()](const boost::system::error_code& err,
                                  const asio::ip::tcp::endpoint&) {
          me->_read_header(err);
        });
  } catch (const std::exception& e) {
    _fail(e.what());
  }
}

void tls_probe::_read_header(const boost::system::error_code& err) {
  if (err) {
    _fail(err.message());
    return;
  }
  try {
    asio::async_read(
        _socket, asio::buffer(_header),
        [me = shared_from_this()](const boost::system::error_code& err,
                                  size_t) { me->_read_payload(err); });
  } catch (const std::exception& e) {
    _fail(e.what());
  }
}

void tls_probe::_read_payload(const boost::system::error_code& err) {
  if (err) {
    _fail(err.message());
    return;
  }
  /* Only the beginning of the greeting is needed. */
  size_t len = std::min<size_t>(_header[0] | _header[1] << 8 | _header[2] << 16,
                                _payload.size());
  try {
    asio::async_read(
        _socket, asio::buffer(_payload.data(), len),
        [me = shared_from_this()](const boost::system::error_code& err,
                                  size_t size) { me->_parse(err, size); });
  } catch (const std::exception& e) {
    _fail(e.what());
  }
}

void tls_probe::_parse(const boost::system::error_code& err, size_t size) {
  if (err) {
    _fail(err.message());
    return;
  }
  std::optional<bool> result = parse_greeting(_payload.data(), size);
  if (result)
    _finish(result);
  else
    _fail("not a handshake v10 greeting");
}

void tls_probe::_fail(const std::string& reason) {
  if (_done)
    return;
  SPDLOG_LOGGER_DEBUG(_logger,
                      "mysql_connection: cannot read the greeting of {}:{}: {}",
                      _host, _port, reason);
  _finish(std::nullopt);
}

/**
 * @brief Shut down and close the socket and give the result, only once: after
 * cancel(), the pending handler calls it again. The socket may not be
 * connected yet (cancel() during resolution or connection): the error of
 * shutdown() is then expected and ignored.
 */
void tls_probe::_finish(std::optional<bool> result) {
  if (_done)
    return;
  _done = true;
  boost::system::error_code ignored;
  _socket.shutdown(asio::ip::tcp::socket::shutdown_both, ignored);
  _socket.close(ignored);
  _result.set_value(result);
}

}  // namespace

/**
 * @brief Read the greeting packet that a MySQL/MariaDB server sends as soon as
 * it accepts a TCP connection, and tell if it advertises TLS (CLIENT_SSL
 * capability). The connection is closed without sending anything, so no
 * credential leaves the host.
 *
 * The probe runs on the shared pool and the calling thread waits for it at
 * most timeout, whatever blocks it: name resolution, a connection dropped by a
 * firewall, a server that never sends its greeting. Never call it from a pool
 * thread.
 *
 * @param host The database host.
 * @param port The database port.
 * @param timeout The maximum time to wait for the greeting.
 * @param logger The logger of the caller.
 *
 * @return Whether the server supports TLS, or std::nullopt if the greeting
 * could not be read in time (server unreachable, error packet...). In that
 * case, libmariadb reports the same error while connecting, before
 * authenticating.
 */
std::optional<bool> com::centreon::broker::database::server_advertises_tls(
    const std::string& host,
    uint16_t port,
    std::chrono::milliseconds timeout,
    const std::shared_ptr<spdlog::logger>& logger) {
  auto probe = std::make_shared<tls_probe>(
      com::centreon::common::pool::io_context(), host, port, logger);
  std::future<std::optional<bool>> result = probe->start();
  if (result.wait_for(timeout) == std::future_status::ready)
    return result.get();

  probe->cancel();
  SPDLOG_LOGGER_DEBUG(logger,
                      "mysql_connection: no greeting from {}:{} within {} ms",
                      host, port, timeout.count());
  return std::nullopt;
}
