/**
 * Copyright 2024 Centreon (https://www.centreon.com/)
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

#include "http_server.hh"

using namespace com::centreon::common::http;

server::server(const std::shared_ptr<asio::io_context>& io_context,
               const std::shared_ptr<spdlog::logger>& logger,
               const http_config::pointer& conf,
               connection_creator&& conn_creator)
    : _io_context(io_context),
      _logger(logger),
      _conf(conf),
      _conn_creator(conn_creator),
      _acceptor(*io_context) {
  SPDLOG_LOGGER_INFO(logger, "create http(s) server listening on {}",
                     conf->get_endpoint());

  try {
    _acceptor.open(_conf->get_endpoint().protocol());
    _acceptor.set_option(asio::ip::tcp::socket::reuse_address(true));

    // Bind to the server address
    _acceptor.bind(_conf->get_endpoint());
    // Start listening for connections
    _acceptor.listen(asio::ip::tcp::socket::max_listen_connections);
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(logger, "fail to listen on {}: {}",
                        _conf->get_endpoint(), e.what());
    throw;
  }
}

server::~server() {
  shutdown();
}

server::pointer server::load(
    const std::shared_ptr<asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    const http_config::pointer& conf,
    connection_creator&& conn_creator) {
  std::shared_ptr<server> ret = std::make_shared<server>(
      io_context, logger, conf, std::move(conn_creator));

  ret->start_accept();

  SPDLOG_LOGGER_INFO(logger, "server listen on {}", conf->get_endpoint());

  return ret;
}

void server::start_accept() {
  connection_base::pointer future_conn = _conn_creator();
  std::lock_guard l(_acceptor_m);
  _acceptor.async_accept(future_conn->get_socket(), future_conn->get_peer(),
                         [future_conn, me = shared_from_this()](
                             const boost::beast::error_code& err) {
                           me->on_accept(err, future_conn);
                         });
}

void server::on_accept(const boost::beast::error_code& err,
                            const connection_base::pointer& conn) {
  if (err) {
    if (err != boost::asio::error::operation_aborted) {
      SPDLOG_LOGGER_ERROR(_logger, "fail accept connection on {}: {}",
                          _conf->get_endpoint(), err.message());
    }
    return;
  }
  SPDLOG_LOGGER_DEBUG(_logger, "connection accepted from {} to {}",
                      conn->get_peer(), _conf->get_endpoint());
  conn->on_accept();

  start_accept();
}

void server::shutdown() {
  std::lock_guard l(_acceptor_m);
  boost::system::error_code ec;
  _acceptor.close(ec);
  if (ec) {
    SPDLOG_LOGGER_ERROR(_logger, "error at server shutdown on {}: {}",
                        _conf->get_endpoint(), ec.message());
  } else {
    SPDLOG_LOGGER_INFO(_logger, "server shutdown {}", _conf->get_endpoint());
  }
}
