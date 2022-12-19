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

#include "http_client.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::http_client;

using lock_guard = std::lock_guard<std::mutex>;

static constexpr duration _min_retry_interval(std::chrono::milliseconds(100));

client::client(const std::shared_ptr<asio::io_context>& io_context,
               const std::shared_ptr<spdlog::logger>& logger,
               const http_config::pointer& conf,
               unsigned max_connections)
    : _io_context(io_context),
      _logger(logger),
      _conf(conf),
      _retry_timer(*io_context),
      _retry_timer_active(false),
      _retry_interval(_min_retry_interval),
      _halt(false) {
  SPDLOG_LOGGER_INFO(_logger, "client::client {}", *_conf);
  _not_connected_conns.reserve(max_connections);
  _keep_alive_conns.reserve(max_connections);
  _busy_conns.reserve(max_connections);
  for (; max_connections > 0; --max_connections) {
    _not_connected_conns.insert(connection::load(io_context, logger, conf));
  }
}

client::pointer client::load(
    const std::shared_ptr<asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    const http_config::pointer& conf,
    unsigned max_connections) {
  return pointer(new client(io_context, logger, conf, max_connections));
}

/**
 * @brief
 *
 * @param request
 * @param callback
 * @return true if connection available
 * @return false if enqueue
 */
bool client::send(const request_ptr& request, send_callback_type&& callback) {
  lock_guard l(_protect);
  time_point now = system_clock::now();

  connection_cont::iterator conn_iter;
  connection::pointer conn;
  cb_request::pointer req(std::make_shared<cb_request>(callback, request));

  // shudown keepalive passed connections
  for (conn_iter = _keep_alive_conns.begin();
       conn_iter != _keep_alive_conns.end();) {
    conn = *conn_iter;
    if (conn->get_keep_alive_end() <= now) {  // http keepalive ended
      conn->shutdown();
      conn_iter = _keep_alive_conns.erase(conn_iter);
    } else {
      ++conn_iter;
    }
  }

  if (!_keep_alive_conns.empty()) {
    // search the oldest keep alive
    conn_iter = std::min_element(
        _keep_alive_conns.begin(), _keep_alive_conns.end(),
        [](const connection::pointer& left,
           const connection::pointer& right) -> bool {
          return left->get_keep_alive_end() < right->get_keep_alive_end();
        });
    conn = *conn_iter;
    _keep_alive_conns.erase(conn_iter);
    _busy_conns.insert(conn);
    conn->send(request, [me = shared_from_this(), conn, req](
                            const boost::beast::error_code& error,
                            const std::string& detail,
                            const response_ptr& response) mutable {
      me->on_sent(error, detail, req, response, conn);
    });
    return true;
  }

  _queue.push_back(req);

  // no idle keepalive connection => connect
  connect();
  return false;
}

/**
 * @brief connect a non connected connection
 *  lock _protect before use it
 * @return true if a connect is launched
 * @return false no idle connection
 */
bool client::connect() {
  if (!_not_connected_conns.empty()) {
    connection::pointer conn = *_not_connected_conns.begin();
    _not_connected_conns.erase(_not_connected_conns.begin());
    _busy_conns.insert(conn);
    conn->connect(
        [me = shared_from_this(), conn](const boost::beast::error_code& error,
                                        const std::string& detail) mutable {
          me->on_connect(error, detail, conn);
        });
    return true;
  }
  return false;
}

/**
 * @brief pop first queue element and try to send it
 * beware, this method don't lock _protect
 *
 */
void client::send_first_queue_request(connection::pointer conn) {
  cb_request::pointer first(std::move(_queue.front()));
  _queue.pop_front();
  conn->send(first->request, [me = shared_from_this(), conn, first](
                                 const boost::beast::error_code& error,
                                 const std::string& detail,
                                 const response_ptr& response) mutable {
    me->on_sent(error, detail, first, response, conn);
  });
}

void client::on_connect(const boost::beast::error_code& error,
                        const std::string& detail,
                        connection::pointer conn) {
  if (error) {
    SPDLOG_LOGGER_ERROR(_logger, "fail to connect {}: {}", error.message(),
                        detail);
    conn->shutdown();
    lock_guard l(_protect);
    if (!_halt) {
      _busy_conns.erase(conn);
      _not_connected_conns.insert(conn);
      start_retry_connect_timer();
    }
    return;
  }
  lock_guard l(_protect);
  if (_halt) {
    return;
  }
  _retry_interval = _min_retry_interval;
  if (_queue.empty()) {  // nothing to send => shutdown
    _busy_conns.erase(conn);
    _not_connected_conns.insert(conn);
    conn->shutdown();
    return;
  }
  send_first_queue_request(conn);
}

void client::on_sent(const boost::beast::error_code& error,
                     const std::string& detail,
                     const cb_request::pointer& request,
                     const response_ptr& response,
                     connection::pointer conn) {
  cb_request::pointer to_call;
  if (error) {
    SPDLOG_LOGGER_ERROR(_logger, "fail to send request => push front");
    conn->shutdown();
    {
      lock_guard l(_protect);
      if (_halt) {
        to_call = request;
      } else {
        _busy_conns.erase(conn);
        _not_connected_conns.insert(conn);
        if (request->retry_counter++ < _conf->get_max_send_retry()) {
          SPDLOG_LOGGER_ERROR(_logger, "fail to send request => push front");
          _queue.push_front(request);
        } else {
          to_call = request;
        }
        start_retry_connect_timer();
      }
    }
    if (to_call) {
      SPDLOG_LOGGER_ERROR(_logger,
                          "too many send request error => callback with error");
      to_call->callback(error, detail, response);
    }
  } else {
    {
      lock_guard l(_protect);
      if (!_halt) {
        if (conn->get_keep_alive_end() >
            system_clock::now()) {  // connection available for next requests?
          if (_queue.empty()) {
            _keep_alive_conns.insert(conn);
            _busy_conns.erase(conn);
          } else {
            send_first_queue_request(conn);
          }
        } else {
          conn->shutdown();
          _not_connected_conns.insert(conn);
          _busy_conns.erase(conn);
        }
      }
    }
    request->callback(error, detail, response);
  }
}

/**
 * @brief start retry timer
 * delay is multipled by two each times
 * lock _protect before use it
 *
 */
void client::start_retry_connect_timer() {
  if (!_retry_timer_active & !_halt) {
    _retry_timer_active = true;
    _retry_timer.expires_after(_retry_interval);
    _retry_interval *= 2;
    if (_retry_interval > _conf->get_max_retry_interval()) {
      _retry_interval = _conf->get_max_retry_interval();
    }
    _retry_timer.async_wait(
        [me = shared_from_this()](const boost::system::error_code& err) {
          me->retry_connect_timer_handler(err);
        });
  }
}

void client::retry_connect_timer_handler(const boost::system::error_code& err) {
  if (err) {
    return;
  }
  lock_guard l(_protect);
  if (!_queue.empty()) {
    connect();
  }
}

void client::shutdown() {
  SPDLOG_LOGGER_INFO(_logger, "client::shutdown {}", *_conf);
  lock_guard l(_protect);
  _retry_timer.cancel();
  for (connection::pointer& conn : _keep_alive_conns) {
    conn->shutdown();
  }
  for (connection::pointer& conn : _busy_conns) {
    conn->shutdown();
  }
  _not_connected_conns.clear();
  _keep_alive_conns.clear();
  _busy_conns.clear();
  _halt = true;
}