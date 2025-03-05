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

#include "http_connection.hh"

using namespace com::centreon::common::http;

std::string connection_base::state_to_str(unsigned state) {
  switch (state) {
    case http_connection::e_state::e_not_connected:
      return "e_not_connected";
    case http_connection::e_state::e_connecting:
      return "e_connecting";
    case http_connection::e_state::e_idle:
      return "e_idle";
    case http_connection::e_state::e_send:
      return "e_send";
    case http_connection::e_state::e_receive:
      return "e_receive";
    case http_connection::e_state::e_shutdown:
      return "e_shutdown";
    default:
      return fmt::format("unknown state {}", state);
  }
}

/**************************************************************************
 *    request_base
 **************************************************************************/
request_base::request_base() {
  _connect = _send = _sent = _receive = system_clock::from_time_t(0);
}

request_base::request_base(boost::beast::http::verb method,
                           const std::string_view& server_name,
                           boost::beast::string_view target)
    : request_type(method, target, 11) {
  set(boost::beast::http::field::host, server_name);
  _connect = _send = _sent = _receive = system_clock::from_time_t(0);
}

void request_base::dump(std::ostream& str) const {
  str << "request:" << this;
}

/**************************************************************************
 *    connection_base
 **************************************************************************/
static const std::regex keep_alive_time_out_r("timeout\\s*=\\s*(\\d+)");

/**
 * @brief set the _keep_alive_end
 * according to
 * https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Keep-Alive
 *
 * @param resp response that contains server keepalive information
 */
void connection_base::gest_keepalive(const response_ptr& resp) {
  if (!resp->keep_alive()) {
    SPDLOG_LOGGER_DEBUG(_logger, "recv response {} no keep alive => shutdown",
                        *_conf);
    shutdown();
  } else {
    // if resp contains keep-alive header, try to extract timeout
    auto keep_alive_info = resp->find(boost::beast::http::field::keep_alive);
    if (keep_alive_info != resp->end()) {
      std::match_results<boost::beast::string_view::const_iterator> res;
      if (std::regex_search(keep_alive_info->value().begin(),
                            keep_alive_info->value().end(), res,
                            keep_alive_time_out_r)) {
        unsigned int second_duration;
        if (absl::SimpleAtoi(res[1].str(), &second_duration)) {
          _keep_alive_end =
              system_clock::now() + std::chrono::seconds(second_duration);
        } else {  // timeout parameter invalid => default
          _keep_alive_end = system_clock::now() +
                            _conf->get_default_http_keepalive_duration();
        }
      } else {  // no keep alive limit
        _keep_alive_end =
            system_clock::now() + _conf->get_default_http_keepalive_duration();
      }
    } else {  // no keep alive limit
      _keep_alive_end =
          system_clock::now() + _conf->get_default_http_keepalive_duration();
    }
    SPDLOG_LOGGER_DEBUG(_logger, "recv response {} keep alive until {}", *_conf,
                        std::chrono::duration_cast<std::chrono::seconds>(
                            _keep_alive_end.time_since_epoch())
                            .count());
  }
}

/**
 * @brief used on server side
 * it adds keepalive header with the value of receive timeout
 *
 * @param response
 */
void connection_base::add_keep_alive_to_server_response(
    const response_ptr& response) const {
  if (_conf->get_default_http_keepalive_duration() >= std::chrono::seconds(1)) {
    response->keep_alive(true);
    response->insert(
        boost::beast::http::field::keep_alive,
        fmt::format("timeout={}",
                    std::chrono::duration_cast<std::chrono::seconds>(
                        _conf->get_default_http_keepalive_duration())
                        .count()));
  } else {
    response->keep_alive(false);
  }
}

/**************************************************************************
 *    http_connection
 **************************************************************************/
http_connection::http_connection(
    const std::shared_ptr<asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    const http_config::pointer& conf,
    const ssl_ctx_initializer&)
    : connection_base(io_context, logger, conf),
      _socket(boost::beast::net::make_strand(*io_context)) {
  SPDLOG_LOGGER_DEBUG(_logger, "create http_connection {:p} to {}",
                      static_cast<void*>(this), *conf);
}

http_connection::~http_connection() {
  SPDLOG_LOGGER_DEBUG(_logger, "delete http_connection {:p} to {}",
                      static_cast<void*>(this), *_conf);
  shutdown();
}

/**
 * @brief the static method to use instead of constructor
 *
 * @param io_context
 * @param logger
 * @param conf
 * @return http_connection::pointer
 */
http_connection::pointer http_connection::load(
    const std::shared_ptr<asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    const http_config::pointer& conf,
    const ssl_ctx_initializer&) {
  return pointer(new http_connection(io_context, logger, conf));
}

#define BAD_CONNECT_STATE_ERROR(error_string)                             \
  std::string detail =                                                    \
      fmt::format("{:p} " error_string, static_cast<void*>(this), *_conf, \
                  state_to_str(expected));                                \
  SPDLOG_LOGGER_ERROR(_logger, detail);                                   \
  _io_context->post([cb = std::move(callback), detail]() {                \
    cb(std::make_error_code(std::errc::invalid_argument), detail);        \
  });                                                                     \
  return;

/**
 * @brief asynchronous connect to an endpoint
 *
 * @param callback
 */
void http_connection::connect(connect_callback_type&& callback) {
  unsigned expected = e_not_connected;
  if (!_state.compare_exchange_strong(expected, e_connecting)) {
    BAD_CONNECT_STATE_ERROR("can connect to {}, bad state {}");
  }

  SPDLOG_LOGGER_DEBUG(_logger, "{:p} connect to {}", static_cast<void*>(this),
                      *_conf);
  std::lock_guard<std::mutex> l(_socket_m);
  _socket.expires_after(_conf->get_connect_timeout());
  if (_conf->get_endpoints_list().empty())
    _socket.async_connect(
        _conf->get_endpoint(),
        [me = shared_from_this(), cb = std::move(callback)](
            const boost::beast::error_code& err) { me->on_connect(err, cb); });
  else
    _socket.async_connect(_conf->get_endpoints_list(),
                          [me = shared_from_this(), cb = std::move(callback)](
                              const boost::beast::error_code& err,
                              const asio::ip::tcp::endpoint& endpoint
                              [[maybe_unused]]) { me->on_connect(err, cb); });
}

/**
 * @brief connect handler
 * in case of success, it initializes tcp keepalive
 *
 * @param err
 * @param callback
 */
void http_connection::on_connect(const boost::beast::error_code& err,
                                 const connect_callback_type& callback) {
  std::string detail;
  if (err) {
    if (_conf->get_endpoints_list().empty())
      detail =
          fmt::format("{:p} fail connect to {}: {}", static_cast<void*>(this),
                      _conf->get_endpoint(), err.message());
    else
      detail =
          fmt::format("{:p} fail connect to {}: {}", static_cast<void*>(this),
                      _conf->get_server_name(), err.message());
    SPDLOG_LOGGER_ERROR(_logger, detail);
    callback(err, detail);
    shutdown();
    return;
  }
  unsigned expected = e_connecting;
  if (!_state.compare_exchange_strong(expected, e_idle)) {
    BAD_CONNECT_STATE_ERROR("on_connect to {}, bad state {}");
  }

  {
    std::lock_guard l(_socket_m);
    boost::system::error_code ec;
    _peer = _socket.socket().remote_endpoint(ec);
    init_keep_alive();
  }
  SPDLOG_LOGGER_DEBUG(_logger, "{:p} connected to {}", static_cast<void*>(this),
                      _peer);

  callback(err, {});
}

/**
 * @brief to call in case of we are an http server
 * it initialize http keepalive
 * callback is useless in this case but is mandatory to have the same interface
 * than https_connection
 *
 * @param callback called via io_context::post (must have the same signature as
 * https)
 */
void http_connection::_on_accept(connect_callback_type&& callback) {
  unsigned expected = e_not_connected;
  if (!_state.compare_exchange_strong(expected, e_idle)) {
    BAD_CONNECT_STATE_ERROR("on_tcp_connect to {}, bad state {}");
  }

  std::lock_guard l(_socket_m);
  init_keep_alive();

  SPDLOG_LOGGER_DEBUG(_logger, "{:p} accepted from {}",
                      static_cast<void*>(this), _peer);

  _io_context->post([cb = std::move(callback)]() { cb({}, ""); });
}

void http_connection::init_keep_alive() {
  // we put first keepalive option and then keepalive intervals
  // system default interval are 7200s which is too long to maintain a NAT
  boost::system::error_code err_keep_alive;
  asio::socket_base::keep_alive opt1(true);
  _socket.socket().set_option(opt1, err_keep_alive);
  if (err_keep_alive) {
    SPDLOG_LOGGER_ERROR(_logger, "fail to activate keep alive for {}", *_conf);
  } else {
    tcp_keep_alive_interval opt2(_conf->get_second_tcp_keep_alive_interval());
    _socket.socket().set_option(opt2, err_keep_alive);
    if (err_keep_alive) {
      SPDLOG_LOGGER_ERROR(_logger, "fail to modify keep alive interval for {}",
                          *_conf);
    }
    tcp_keep_alive_idle opt3(_conf->get_second_tcp_keep_alive_interval());
    _socket.socket().set_option(opt3, err_keep_alive);
    if (err_keep_alive) {
      SPDLOG_LOGGER_ERROR(
          _logger, "fail to modify first keep alive delay for {}", *_conf);
    }
  }
}

#define BAD_SEND_STATE_ERROR(error_string)                               \
  std::string detail =                                                   \
      fmt::format("{:p}" error_string, static_cast<void*>(this), *_conf, \
                  state_to_str(expected));                               \
  SPDLOG_LOGGER_ERROR(_logger, detail);                                  \
  _io_context->post([cb = std::move(callback), detail]() {               \
    cb(std::make_error_code(std::errc::invalid_argument), detail,        \
       response_ptr());                                                  \
  });                                                                    \
  return;

/**
 * @brief send a request on a connected socket
 *
 * @param request
 * @param callback
 */
void http_connection::send(request_ptr request, send_callback_type&& callback) {
  unsigned expected = e_idle;
  if (!_state.compare_exchange_strong(expected, e_send)) {
    BAD_SEND_STATE_ERROR("send to {}, bad state {}");
  }

  request->_send = system_clock::now();
  if (_logger->level() == spdlog::level::trace) {
    SPDLOG_LOGGER_TRACE(_logger, "{:p} send request {} to {}",
                        static_cast<void*>(this),
                        static_cast<request_type>(*request), _peer);
  } else {
    SPDLOG_LOGGER_DEBUG(_logger, "{:p} send request to {}",
                        static_cast<void*>(this), _peer);
  }
  std::lock_guard<std::mutex> l(_socket_m);
  _socket.expires_after(_conf->get_send_timeout());
  boost::beast::http::async_write(
      _socket, *request,
      [me = shared_from_this(), request, cb = std::move(callback)](
          const boost::beast::error_code& err, size_t) mutable {
        me->on_sent(err, request, cb);
      });
}

/**
 * @brief send handler, is all is ok, async read response
 *
 * @param err
 * @param request
 * @param callback
 */
void http_connection::on_sent(const boost::beast::error_code& err,
                              request_ptr request,
                              send_callback_type& callback) {
  if (err) {
    std::string detail =
        fmt::format("{:p} fail send {} to {}: {}", static_cast<void*>(this),
                    *request, *_conf, err.message());
    SPDLOG_LOGGER_ERROR(_logger, detail);
    callback(err, detail, response_ptr());
    shutdown();
    return;
  }

  unsigned expected = e_send;
  if (!_state.compare_exchange_strong(expected, e_receive)) {
    BAD_SEND_STATE_ERROR("on_sent to {}, bad state {}");
  }

  if (_logger->level() == spdlog::level::trace) {
    SPDLOG_LOGGER_TRACE(_logger, "{:p} {} sent to {}", static_cast<void*>(this),
                        *request, *_conf);
  } else {
    SPDLOG_LOGGER_DEBUG(_logger, "{:p} request sent to {}",
                        static_cast<void*>(this), *_conf);
  }

  request->_sent = system_clock::now();

  response_ptr resp = std::make_shared<response_type>();
  std::lock_guard<std::mutex> l(_socket_m);
  if (_conf->get_receive_timeout() >= std::chrono::seconds(1)) {
    _socket.expires_after(_conf->get_receive_timeout());
  }
  boost::beast::http::async_read(
      _socket, _recv_buffer, *resp,
      [me = shared_from_this(), request, cb = std::move(callback), resp](
          const boost::beast::error_code& ec, std::size_t) mutable {
        me->on_read(ec, request, cb, resp);
      });
}

/**
 * @brief receive handler
 *
 * @param err
 * @param request
 * @param callback
 * @param resp
 */
void http_connection::on_read(const boost::beast::error_code& err,
                              const request_ptr& request,
                              send_callback_type& callback,
                              const response_ptr& resp) {
  if (err) {
    std::string detail =
        fmt::format("{:p} fail receive {} from {}: {}",
                    static_cast<void*>(this), *request, *_conf, err.message());
    if (err != boost::asio::error::operation_aborted) {
      SPDLOG_LOGGER_ERROR(_logger, detail);
    }
    callback(err, detail, response_ptr());
    shutdown();
    return;
  }

  unsigned expected = e_receive;
  if (!_state.compare_exchange_strong(expected, e_idle)) {
    BAD_SEND_STATE_ERROR("on_read to {}, bad state {}");
  }

  request->_receive = system_clock::now();

  // http error code aren't managed in this layer, this the job of the caller
  // everything is fine at the transport level
  if (resp->result_int() >= 400) {
    SPDLOG_LOGGER_ERROR(_logger, "{:p} err response for {} \n\n {}",
                        static_cast<void*>(this),
                        static_cast<request_type>(*request), *resp);
  } else {
    SPDLOG_LOGGER_DEBUG(_logger, "{:p} recv response from {} {}",
                        static_cast<void*>(this), *_conf, *resp);
  }
  // keepalive => idle state, otherwise => shutdown
  gest_keepalive(resp);

  callback(err, {}, resp);
}

/**
 * @brief server side, send response to client
 * in case of receive time out < 1s, socket is closed after response sent
 *
 * @param response
 * @param callback
 */
void http_connection::answer(const response_ptr& response,
                             answer_callback_type&& callback) {
  unsigned expected = e_idle;
  if (!_state.compare_exchange_strong(expected, e_send)) {
    std::string detail = fmt::format(
        "{:p}"
        "answer to {}, bad state {}",
        static_cast<void*>(this), _peer, state_to_str(expected));
    SPDLOG_LOGGER_ERROR(_logger, detail);
    _io_context->post([cb = std::move(callback), detail]() {
      cb(std::make_error_code(std::errc::invalid_argument), detail);
    });
    return;
  }

  add_keep_alive_to_server_response(response);

  std::lock_guard<std::mutex> l(_socket_m);
  _socket.expires_after(_conf->get_send_timeout());
  boost::beast::http::async_write(
      _socket, *response,
      [me = shared_from_this(), response, cb = std::move(callback)](
          const boost::beast::error_code& ec, size_t) mutable {
        if (ec) {
          SPDLOG_LOGGER_ERROR(me->_logger,
                              "{:p} fail to send response {} to {}: {}",
                              static_cast<void*>(me.get()), *response,
                              me->get_peer(), ec.message());
          me->shutdown();
        } else {
          unsigned expected = e_send;
          me->_state.compare_exchange_strong(expected, e_idle);
          if (me->_conf->get_receive_timeout() < std::chrono::seconds(1)) {
            me->shutdown();
          }
        }
        SPDLOG_LOGGER_TRACE(me->_logger, "{:p} response sent: {}",
                            static_cast<void*>(me.get()), *response);
        cb(ec, "");
      });
}

/**
 * @brief wait for incoming request
 *
 * @param callback
 */
void http_connection::receive_request(request_callback_type&& callback) {
  unsigned expected = e_idle;
  if (!_state.compare_exchange_strong(expected, e_receive)) {
    std::string detail = fmt::format(
        "{:p}"
        "receive_request from {}, bad state {}",
        static_cast<void*>(this), _peer, state_to_str(expected));
    SPDLOG_LOGGER_ERROR(_logger, detail);
    _io_context->post([cb = std::move(callback), detail]() {
      cb(std::make_error_code(std::errc::invalid_argument), detail,
         std::shared_ptr<request_type>());
    });
    return;
  }

  std::lock_guard<std::mutex> l(_socket_m);
  if (_conf->get_receive_timeout() >= std::chrono::seconds(1))
    _socket.expires_after(_conf->get_receive_timeout());

  auto req = std::make_shared<request_type>();
  boost::beast::http::async_read(
      _socket, _recv_buffer, *req,
      [me = shared_from_this(), req, cb = std::move(callback)](
          const boost::beast::error_code& ec, std::size_t) mutable {
        if (ec) {
          if (ec == boost::beast::http::error::end_of_stream) {
            SPDLOG_LOGGER_DEBUG(
                me->_logger, "{:p} fail to receive request from {}: {}",
                static_cast<void*>(me.get()), me->get_peer(), ec.message());
          } else {
            SPDLOG_LOGGER_ERROR(
                me->_logger, "{:p} fail to receive request from {}: {}",
                static_cast<void*>(me.get()), me->get_peer(), ec.message());
          }
          me->shutdown();
        } else {
          unsigned expected = e_receive;
          me->_state.compare_exchange_strong(expected, e_idle);
        }
        SPDLOG_LOGGER_TRACE(me->_logger, "{:p} receive: {}",
                            static_cast<void*>(me.get()), *req);
        cb(ec, "", req);
      });
}

/**
 * @brief shutdown socket and close
 *
 */
void http_connection::shutdown() {
  SPDLOG_LOGGER_DEBUG(_logger, "{:p} shutdown {}", static_cast<void*>(this),
                      *_conf);
  _state = e_not_connected;
  boost::system::error_code err;
  std::lock_guard<std::mutex> l(_socket_m);
  _socket.socket().shutdown(boost::asio::socket_base::shutdown_both, err);
  _socket.close();
}

/**
 * @brief get underlay socket
 *
 * @return asio::ip::tcp::socket&
 */
asio::ip::tcp::socket& http_connection::get_socket() {
  return _socket.socket();
}
