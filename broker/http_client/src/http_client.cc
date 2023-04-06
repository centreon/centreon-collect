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

#include "com/centreon/async/defer.hh"

#include "http_client.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::http_client;

using lock_guard = std::lock_guard<std::mutex>;

/**
 * @brief Construct a new client::client object
 * at the construction, no connection to server is done, connection will be done
 * on demand
 *
 * @param io_context
 * @param logger
 * @param conf
 * @param conn_creator this function is used to construct connections to the
 * server, it can be a http_connection::load, https_connection::load....
 */
client::client(const std::shared_ptr<asio::io_context>& io_context,
               const std::shared_ptr<spdlog::logger>& logger,
               const http_config::pointer& conf,
               connection_creator conn_creator)
    : _io_context(io_context),
      _logger(logger),
      _conf(conf),
      _retry_unit(std::chrono::seconds(1)),
      _halt(false) {
  SPDLOG_LOGGER_INFO(_logger, "client::client {}", *_conf);
  _not_connected_conns.reserve(conf->get_max_connections());
  _keep_alive_conns.reserve(conf->get_max_connections());
  _busy_conns.reserve(conf->get_max_connections());
  // create all connection ready to connect
  for (unsigned cpt = 0; cpt < conf->get_max_connections(); ++cpt) {
    _not_connected_conns.insert(conn_creator(io_context, logger, conf));
  }
}

/**
 * @brief in order to avoid mistakes, client::client is protected
 * The use of this static method is mandatory to create a client object
 *
 * @param io_context
 * @param logger
 * @param conf
 * @param conn_creator
 * @return client::pointer
 */
client::pointer client::load(
    const std::shared_ptr<asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    const http_config::pointer& conf,
    connection_creator conn_creator) {
  return pointer(new client(io_context, logger, conf, conn_creator));
}

/**
 * @brief if an active connection is idle, request is sent on it.
 * Otherwise, it's search not connected available connection, and connected
 * If non connection is available, it pushs request and callback on queue
 *
 * @param request
 * @param callback
 * @return true if connection available
 * @return false if enqueue
 */
bool client::send_or_push(const cb_request::pointer request,
                          bool push_to_front) {
  if (_halt) {
    return false;
  }
  lock_guard l(_protect);
  time_point now = system_clock::now();

  connection_cont::iterator conn_iter;
  connection_base::pointer conn;

  // shutdown keepalive passed connections
  for (conn_iter = _keep_alive_conns.begin();
       conn_iter != _keep_alive_conns.end();) {
    conn = *conn_iter;
    if (conn->get_keep_alive_end() <=
        now) {  // http keepalive ended => close it
      SPDLOG_LOGGER_DEBUG(_logger, "end of keepalive for {:p}",
                          static_cast<void*>(conn.get()));
      conn->shutdown();
      conn_iter = _keep_alive_conns.erase(conn_iter);
    } else {
      ++conn_iter;
    }
  }

  if (!_keep_alive_conns.empty()) {  // now there is only valid idle connections
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
    conn->send(request->request, [me = shared_from_this(), conn, request](
                                     const boost::beast::error_code& error,
                                     const std::string& detail,
                                     const response_ptr& response) mutable {
      me->on_sent(error, detail, request, response, conn);
    });
    return true;
  }

  // no idle keepalive connection => connect
  if (!connect(request)) {  // no idle conn to connect => push to queue
    if (push_to_front) {    // when retry, we push request to front instead of
                            // back
      _queue.push_front(request);
    } else {
      _queue.push_back(request);
    }
  }
  return false;
}

/**
 * @brief connect a non connected connection if available
 *  lock _protect before use it
 * @return true if a connect is launched
 * @return false no idle connection
 */
bool client::connect(const cb_request::pointer& request) {
  if (_halt) {
    return false;
  }
  if (!_not_connected_conns.empty()) {
    for (connection_base::pointer conn : _not_connected_conns) {
      if (conn->get_state() == connection_base::e_not_connected) {
        _not_connected_conns.erase(_not_connected_conns.begin());
        _busy_conns.insert(conn);
        SPDLOG_LOGGER_DEBUG(_logger, "connection of {:p}",
                            static_cast<void*>(conn.get()));
        request->request->_connect = system_clock::now();
        conn->connect([me = shared_from_this(), conn, request](
                          const boost::beast::error_code& error,
                          const std::string& detail) mutable {
          me->on_connect(error, detail, request, conn);
        });
        return true;
      }
    }
  }
  return false;
}

/**
 * @brief pop first queue element and try to send it
 *
 */
void client::send_first_queue_request() {
  cb_request::pointer to_send;
  {
    std::lock_guard<std::mutex> l(_protect);
    if (_queue.empty() || _halt) {
      return;
    }
    to_send = _queue.front();
    _queue.pop_front();
  }
  if (to_send) {
    send_or_push(to_send, false);
  }
}

/**
 * @brief send request on conn
 *
 * @param request
 * @param conn
 */
void client::send(const cb_request::pointer& request,
                  connection_base::pointer conn) {
  if (_logger->level() == spdlog::level::trace) {
    SPDLOG_LOGGER_TRACE(_logger, "send {} on {:p}", *request->request,
                        static_cast<void*>(conn.get()));
  } else {
    SPDLOG_LOGGER_DEBUG(_logger, "send on {:p}",
                        static_cast<void*>(conn.get()));
  }
  conn->send(request->request, [me = shared_from_this(), conn, request](
                                   const boost::beast::error_code& error,
                                   const std::string& detail,
                                   const response_ptr& response) mutable {
    me->on_sent(error, detail, request, response, conn);
  });
}

/**
 * @brief connect handler
 * if error is set conn is shutdown and available for a new connect and we retry
 * to send request if authorized
 *
 * @param error
 * @param detail
 * @param request
 * @param conn
 */
void client::on_connect(const boost::beast::error_code& error,
                        const std::string& detail,
                        const cb_request::pointer& request,
                        connection_base::pointer conn) {
  if (error) {  // error => shutdown and retry
    SPDLOG_LOGGER_ERROR(_logger, "{:p} fail to connect {}: {}",
                        static_cast<void*>(conn.get()), error.message(),
                        detail);
    conn->shutdown();
    {
      lock_guard l(_protect);
      _busy_conns.erase(conn);
      _not_connected_conns.insert(conn);
    }
    retry(error, detail, request, response_ptr());
    return;
  }
  lock_guard l(_protect);
  if (_halt) {
    return;
  }
  SPDLOG_LOGGER_DEBUG(_logger, "{:p} connected",
                      static_cast<void*>(conn.get()));

  send(request, conn);
}

/**
 * @brief this handler is called when we have receive a response to the request
 * if error is set conn is shutdown and available for a new connect and we retry
 * to send request if authorized
 *
 * @param error
 * @param detail
 * @param request
 * @param response
 * @param conn
 */
void client::on_sent(const boost::beast::error_code& error,
                     const std::string& detail,
                     const cb_request::pointer& request,
                     const response_ptr& response,
                     connection_base::pointer conn) {
  cb_request::pointer to_call;
  if (error) {  // error => shutdown and retry
    SPDLOG_LOGGER_ERROR(_logger, "{:p} fail to send request",
                        static_cast<void*>(conn.get()));
    conn->shutdown();
    {
      lock_guard l(_protect);
      _busy_conns.erase(conn);
      _not_connected_conns.insert(conn);
    }
    retry(error, detail, request, response);
    if (to_call) {
      SPDLOG_LOGGER_ERROR(_logger,
                          "too many send request error => callback with error");
      to_call->callback(error, detail, response);
    }

  } else {
    if (_logger->level() == spdlog::level::trace) {
      SPDLOG_LOGGER_TRACE(_logger, "response received for {} on {:p}",
                          *request->request, static_cast<void*>(conn.get()));
    } else {
      SPDLOG_LOGGER_DEBUG(_logger, "response received on {:p}",
                          static_cast<void*>(conn.get()));
    }
    bool has_to_send_first_inqueue = false;
    {
      lock_guard l(_protect);
      if (!_halt) {
        if (conn->get_state() == connection_base::e_idle &&
            conn->get_keep_alive_end() >
                system_clock::now()) {  // connection available for next
                                        // requests?
          if (_queue.empty()) {  // no request => go to the keepalive container
            SPDLOG_LOGGER_DEBUG(_logger, "nothing to send {:p} wait",
                                static_cast<void*>(conn.get()));
            _keep_alive_conns.insert(conn);
            _busy_conns.erase(conn);
          } else {  // request in queue => send
            SPDLOG_LOGGER_DEBUG(_logger, "recycle of {:p}",
                                static_cast<void*>(conn.get()));
            cb_request::pointer first(std::move(_queue.front()));
            _queue.pop_front();
            send(first, conn);
          }
        } else {  // no keepalive => shutdown
          SPDLOG_LOGGER_DEBUG(_logger, "no keepalive {:p} shutdown",
                              static_cast<void*>(conn.get()));
          conn->shutdown();
          _not_connected_conns.insert(conn);
          _busy_conns.erase(conn);
          has_to_send_first_inqueue = true;
        }
      }
    }
    if (has_to_send_first_inqueue) {
      send_first_queue_request();
    }
    request->callback(error, detail, response);
  }
}

/**
 * @brief shutdown all connections
 * after this method call, instance musn't be used
 *
 */
void client::shutdown() {
  SPDLOG_LOGGER_INFO(_logger, "client::shutdown {}", *_conf);
  lock_guard l(_protect);
  // shutdown all connections
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

/**
 * @brief when anything goes wrong, this method is called
 * cb_request contains a retry counter
 * if this counter is greater than the retry limit, callback is called with the
 * error otherwise, we
 *
 * @param error
 * @param detail
 * @param request
 * @param response
 */
void client::retry(const boost::beast::error_code& error,
                   const std::string& detail,
                   const cb_request::pointer& request,
                   const response_ptr& response) {
  cb_request::pointer to_call;

  if (_halt) {  // object halted => callback without retry
    to_call = request;
  } else {
    if (request->retry_counter++ <
        _conf->get_max_send_retry()) {  // retry in next_retry delay
      duration next_retry = _retry_unit * request->retry_counter;

      if (next_retry > _conf->get_max_retry_interval()) {
        next_retry = _conf->get_max_retry_interval();
      }

      SPDLOG_LOGGER_ERROR(
          _logger, "fail to send request => resent in {} s",
          std::chrono::duration_cast<std::chrono::seconds>(next_retry).count());
      async::defer(_io_context, next_retry,
                   [me = shared_from_this(), request]() {
                     me->send_or_push(request, true);
                   });
    } else {  // to many error => callback with error
      to_call = request;
    }
  }
  if (to_call) {
    SPDLOG_LOGGER_ERROR(_logger,
                        "too many send request error => callback with error");
    to_call->callback(error, detail, response);

    // try to send the first enqueue request in 1s
    if (!_halt) {
      async::defer(_io_context, _retry_unit, [me = shared_from_this()]() {
        me->send_first_queue_request();
      });
    }
  }
}