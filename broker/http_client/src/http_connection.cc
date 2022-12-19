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

#include "http_connection.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::http_client;

/**
 * @brief this option set the interval in seconds between two keepalive sent
 *
 */
using tcp_keep_alive_interval =
    asio::detail::socket_option::integer<IPPROTO_TCP, TCP_KEEPINTVL>;

/**
 * @brief this option set the delay after the first keepalive will be sent
 *
 */
using tcp_keep_alive_idle =
    asio::detail::socket_option::integer<IPPROTO_TCP, TCP_KEEPIDLE>;

static std::string state_to_str(unsigned state) {
  switch (state) {
    case connection::e_state::e_not_connected:
      return "e_not_connected";
    case connection::e_state::e_connecting:
      return "e_connecting";
    case connection::e_state::e_idle:
      return "e_idle";
    case connection::e_state::e_send:
      return "e_send";
    case connection::e_state::e_receive:
      return "e_receive";
    default:
      return fmt::format("unknown state {}", state);
  }
}

connection::connection(const std::shared_ptr<asio::io_context>& io_context,
                       const std::shared_ptr<spdlog::logger>& logger,
                       const http_config::pointer& conf)
    : _io_context(io_context),
      _logger(logger),
      _state(e_not_connected),
      _socket(boost::beast::net::make_strand(*io_context)),
      _conf(conf) {
  SPDLOG_LOGGER_DEBUG(_logger, "create connection to {}", *conf);
}

connection::~connection() {
  SPDLOG_LOGGER_DEBUG(_logger, "delete connection to {}", *_conf);
  shutdown();
}

connection::pointer connection::load(
    const std::shared_ptr<asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    const http_config::pointer& conf) {
  return pointer(new connection(io_context, logger, conf));
}

#define BAD_CONNECT_STATE_ERROR(error_string)                      \
  std::string detail =                                             \
      fmt::format(error_string, *_conf, state_to_str(expected));   \
  SPDLOG_LOGGER_ERROR(_logger, detail);                            \
  _io_context->post([cb = std::move(callback), detail]() {         \
    cb(std::make_error_code(std::errc::invalid_argument), detail); \
  });                                                              \
  return;

void connection::connect(connect_callback_type&& callback) {
  unsigned expected = e_not_connected;
  if (!_state.compare_exchange_strong(expected, e_connecting)) {
    BAD_CONNECT_STATE_ERROR("can connect to {}, bad state {}");
  }

  SPDLOG_LOGGER_DEBUG(_logger, "connect to {}", _conf);
  _socket.expires_after(_conf->get_connect_timeout());
  _socket.async_connect(
      _conf->get_endpoint(),
      [me = shared_from_this(), cb = std::move(callback)](
          const boost::beast::error_code& err) { me->on_connect(err, cb); });
}

void connection::on_connect(const boost::beast::error_code& err,
                            const connect_callback_type& callback) {
  if (err) {
    std::string detail = fmt::format("fail connect to {}: {}",
                                     _conf->get_endpoint(), err.message());
    SPDLOG_LOGGER_ERROR(_logger, detail);
    callback(err, detail);
    shutdown();
    return;
  }
  unsigned expected = e_connecting;
  if (!_state.compare_exchange_strong(expected, e_idle)) {
    BAD_CONNECT_STATE_ERROR("on_connect to {}, bad state {}");
  }

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
  SPDLOG_LOGGER_DEBUG(_logger, "connected to {}", _conf->get_endpoint());
  callback(err, {});
}

#define BAD_SEND_STATE_ERROR(error_string)                        \
  std::string detail =                                            \
      fmt::format(error_string, *_conf, state_to_str(expected));  \
  SPDLOG_LOGGER_ERROR(_logger, detail);                           \
  _io_context->post([cb = std::move(callback), detail]() {        \
    cb(std::make_error_code(std::errc::invalid_argument), detail, \
       response_ptr());                                           \
  });                                                             \
  return;

void connection::send(request_ptr request, send_callback_type&& callback) {
  unsigned expected = e_idle;
  if (!_state.compare_exchange_strong(expected, e_send)) {
    BAD_SEND_STATE_ERROR("send to {}, bad state {}");
  }

  SPDLOG_LOGGER_DEBUG(_logger, "send request to {}", _conf->get_endpoint());

  _socket.expires_after(_conf->get_send_timeout());
  boost::beast::http::async_write(
      _socket, *request,
      [me = shared_from_this(), request, cb = std::move(callback)](
          const boost::beast::error_code& err,
          size_t bytes_transfered) mutable { me->on_sent(err, request, cb); });
}

void connection::on_sent(const boost::beast::error_code& err,
                         request_ptr request,
                         send_callback_type& callback) {
  if (err) {
    std::string detail =
        fmt::format("fail send {} to {}: {}", *request, *_conf, err.message());
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
    SPDLOG_LOGGER_TRACE(_logger, "{} sent to {}", *request, *_conf);
  } else {
    SPDLOG_LOGGER_DEBUG(_logger, "request sent to {}", *_conf);
  }

  response_ptr resp = std::make_shared<response_type>();
  boost::beast::http::async_read(
      _socket, _recv_buffer, *resp,
      [me = shared_from_this(), request, cb = std::move(callback), resp](
          const boost::beast::error_code& ec, std::size_t) mutable {
        me->on_read(ec, request, cb, resp);
      });
}

static const std::regex keep_alive_time_out_r("timeout\\s*=\\s*(\\d+)");

void connection::on_read(const boost::beast::error_code& err,
                         const request_ptr& request,
                         send_callback_type& callback,
                         const response_ptr& resp) {
  if (err) {
    std::string detail = fmt::format("fail receive {} from {}: {}", *request,
                                     *_conf, err.message());
    SPDLOG_LOGGER_ERROR(_logger, detail);
    callback(err, detail, response_ptr());
    shutdown();
    return;
  }

  unsigned expected = e_receive;
  if (!_state.compare_exchange_strong(expected, e_idle)) {
    BAD_SEND_STATE_ERROR("on_read to {}, bad state {}");
  }

  if (resp->result_int() >= 400) {
    SPDLOG_LOGGER_ERROR(_logger, "err response for {} \n\n {}", *request,
                        *resp);
  } else {
    SPDLOG_LOGGER_DEBUG(_logger, "recv response from {} {}", *_conf, *resp);
  }
  if (!resp->keep_alive()) {
    SPDLOG_LOGGER_DEBUG(_logger, "recv response {} no keep alive => shutdown",
                        *_conf);
    shutdown();
  } else {
    auto keep_alive_info = resp->find(boost::beast::http::field::keep_alive);
    if (keep_alive_info != resp->end()) {
      std::match_results<boost::beast::string_view::const_iterator> res;
      if (std::regex_search(keep_alive_info->value().begin(),
                            keep_alive_info->value().end(), res,
                            keep_alive_time_out_r)) {
        uint second_duration;
        if (absl::SimpleAtoi(res[1].str(), &second_duration)) {
          _keep_alive_end =
              system_clock::now() + std::chrono::seconds(second_duration);
        } else {
          _keep_alive_end = system_clock::now() + std::chrono::hours(1);
        }
      } else {  // no keep alive limit
        _keep_alive_end = system_clock::now() + std::chrono::hours(1);
      }
    } else {  // no keep alive limit
      _keep_alive_end = system_clock::now() + std::chrono::hours(1);
    }
    SPDLOG_LOGGER_DEBUG(_logger, "recv response {} keep alive until {}", *_conf,
                        std::chrono::duration_cast<std::chrono::seconds>(
                            _keep_alive_end.time_since_epoch())
                            .count());
  }

  callback(err, {}, resp);
}

void connection::shutdown() {
  SPDLOG_LOGGER_DEBUG(_logger, "shutdown {}", _conf);
  boost::system::error_code err;
  _socket.socket().shutdown(boost::asio::socket_base::shutdown_both, err);
  _socket.close();
}
