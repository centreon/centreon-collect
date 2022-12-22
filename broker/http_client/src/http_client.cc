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
               connection_creator conn_creator)
    : _io_context(io_context),
      _logger(logger),
      _conf(conf),
      _retry_timer(*io_context),
      _retry_timer_active(false),
      _retry_interval(_min_retry_interval),
      _halt(false) {
  SPDLOG_LOGGER_INFO(_logger, "client::client {}", *_conf);
  _not_connected_conns.reserve(conf->get_max_connections());
  _keep_alive_conns.reserve(conf->get_max_connections());
  _busy_conns.reserve(conf->get_max_connections());
  for (unsigned cpt = 0; cpt < conf->get_max_connections(); ++cpt) {
    _not_connected_conns.insert(conn_creator(io_context, logger, conf));
  }
}

client::pointer client::load(
    const std::shared_ptr<asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    const http_config::pointer& conf,
    connection_creator conn_creator) {
  return pointer(new client(io_context, logger, conf, conn_creator));
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
  connection_base::pointer conn;
  cb_request::pointer req(std::make_shared<cb_request>(callback, request));

  // shudown keepalive passed connections
  for (conn_iter = _keep_alive_conns.begin();
       conn_iter != _keep_alive_conns.end();) {
    conn = *conn_iter;
    if (conn->get_keep_alive_end() <= now) {  // http keepalive ended
      SPDLOG_LOGGER_DEBUG(_logger, "end of keepalive for {:p}",
                          static_cast<void*>(conn.get()));
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
        [](const connection_base::pointer& left,
           const connection_base::pointer& right) -> bool {
          return left->get_keep_alive_end() < right->get_keep_alive_end();
        });
    conn = *conn_iter;
    _keep_alive_conns.erase(conn_iter);
    _busy_conns.insert(conn);
    SPDLOG_LOGGER_DEBUG(_logger, "reuse of {:p}",
                        static_cast<void*>(conn.get()));
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
    connection_base::pointer conn = *_not_connected_conns.begin();
    _not_connected_conns.erase(_not_connected_conns.begin());
    _busy_conns.insert(conn);
    SPDLOG_LOGGER_DEBUG(_logger, "connection of {:p}",
                        static_cast<void*>(conn.get()));
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
void client::send_first_queue_request(connection_base::pointer conn) {
  cb_request::pointer first(std::move(_queue.front()));
  _queue.pop_front();
  SPDLOG_LOGGER_DEBUG(_logger, "send on {:p}", static_cast<void*>(conn.get()));

  conn->send(first->request, [me = shared_from_this(), conn, first](
                                 const boost::beast::error_code& error,
                                 const std::string& detail,
                                 const response_ptr& response) mutable {
    me->on_sent(error, detail, first, response, conn);
  });
}

void client::on_connect(const boost::beast::error_code& error,
                        const std::string& detail,
                        connection_base::pointer conn) {
  if (error) {
    SPDLOG_LOGGER_ERROR(_logger, "{:p} fail to connect {}: {}",
                        static_cast<void*>(conn.get()), error.message(),
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
  SPDLOG_LOGGER_DEBUG(_logger, "{:p} connected",
                      static_cast<void*>(conn.get()));

  send_first_queue_request(conn);
}

void client::on_sent(const boost::beast::error_code& error,
                     const std::string& detail,
                     const cb_request::pointer& request,
                     const response_ptr& response,
                     connection_base::pointer conn) {
  cb_request::pointer to_call;
  if (error) {
    SPDLOG_LOGGER_ERROR(_logger, "{:p} fail to send request",
                        static_cast<void*>(conn.get()));
    conn->shutdown();
    {
      lock_guard l(_protect);
      if (_halt) {
        to_call = request;
      } else {
        _busy_conns.erase(conn);
        _not_connected_conns.insert(conn);
        if (request->retry_counter++ < _conf->get_max_send_retry()) {
          SPDLOG_LOGGER_ERROR(_logger,
                              "{:p} fail to send request => push front",
                              static_cast<void*>(conn.get()));
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
    SPDLOG_LOGGER_DEBUG(_logger, "response received on {:p}",
                        static_cast<void*>(conn.get()));
    {
      lock_guard l(_protect);
      if (!_halt) {
        if (conn->get_state() == connection_base::e_idle &&
            conn->get_keep_alive_end() >
                system_clock::now()) {  // connection available for next
                                        // requests?
          if (_queue.empty()) {
            SPDLOG_LOGGER_DEBUG(_logger, "nothing to send {:p} wait",
                                static_cast<void*>(conn.get()));
            _keep_alive_conns.insert(conn);
            _busy_conns.erase(conn);
          } else {
            SPDLOG_LOGGER_DEBUG(_logger, "recycle of {:p}",
                                static_cast<void*>(conn.get()));
            send_first_queue_request(conn);
          }
        } else {
          SPDLOG_LOGGER_DEBUG(_logger, "no keepalive {:p} shutdown",
                              static_cast<void*>(conn.get()));
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
  for (connection_base::pointer& conn : _keep_alive_conns) {
    conn->shutdown();
  }
  for (connection_base::pointer& conn : _busy_conns) {
    conn->shutdown();
  }
  _not_connected_conns.clear();
  _keep_alive_conns.clear();
  _busy_conns.clear();
  _halt = true;
}
