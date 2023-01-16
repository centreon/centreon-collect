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

#include <boost/serialization/singleton.hpp>

#include <absl/container/flat_hash_map.h>

#include "https_connection.hh"

using namespace com::centreon::broker;
using namespace com::centreon::exceptions;
using namespace com::centreon::broker::http_client;

namespace beast = boost::beast;

CCB_BEGIN()

namespace http_client {
namespace detail {
class certificate_cache
    : public boost::serialization::singleton<certificate_cache> {
  struct certificate {
    time_t last_access;
    std::shared_ptr<std::string> content;
  };

  using cert_container = std::map<std::string, certificate>;

  cert_container _certs;
  std::mutex _protect;

  void clean();

 public:
  std::shared_ptr<std::string> get_certificate(const std::string& path);
};

std::shared_ptr<std::string> certificate_cache::get_certificate(
    const std::string& path) {
  time_t now = time(nullptr);

  {
    std::lock_guard<std::mutex> l(_protect);
    cert_container::iterator exist = _certs.find(path);
    if (exist != _certs.end()) {
      struct stat file_stat;
      if (stat(path.c_str(), &file_stat)) {
        throw msg_fmt("stat fail of certificate: {} : {}", path,
                      strerror(errno));
      }
      if (file_stat.st_mtim.tv_sec >
          exist->second.last_access) {  // modified on disk => reload}
        _certs.erase(exist);
      } else {
        exist->second.last_access = now;
        clean();
        return exist->second.content;
      }
    }
    clean();
  }

  std::ifstream file(path);
  if (file.is_open()) {
    std::stringstream ss;
    ss << file.rdbuf();
    file.close();
    std::lock_guard<std::mutex> l(_protect);
    auto content = std::make_shared<std::string>(ss.str());
    _certs[path] = {now, content};
    return content;
  } else {
    throw msg_fmt("Cannot open certificate file '{}': {}", path,
                  strerror(errno));
  }
}

void certificate_cache::clean() {
  time_t peremption = time(nullptr) - 3600;
  for (cert_container::iterator to_test = _certs.begin();
       !_certs.empty() && to_test != _certs.end();) {
    if (to_test->second.last_access < peremption) {
      to_test = _certs.erase(to_test);
    } else {
      ++to_test;
    }
  }
}

}  // namespace detail
};  // namespace http_client

CCB_END()

https_connection::https_connection(
    const std::shared_ptr<asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    const http_config::pointer& conf)
    : connection_base(io_context, logger, conf),
      _sslcontext(conf->get_ssl_method()),
      _stream(beast::net::make_strand(*io_context), _sslcontext) {
  if (!_conf->get_certificate_path().empty()) {
    std::shared_ptr<std::string> cert_content =
        detail::certificate_cache::get_mutable_instance().get_certificate(
            _conf->get_certificate_path());
    _sslcontext.add_certificate_authority(
        asio::buffer(cert_content->c_str(), cert_content->length()));
  }
  SPDLOG_LOGGER_DEBUG(_logger, "create https_connection to {}", *conf);
}

https_connection::~https_connection() {
  SPDLOG_LOGGER_DEBUG(_logger, "delete https_connection to {}", *_conf);
}

https_connection::pointer https_connection::load(
    const std::shared_ptr<asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    const http_config::pointer& conf) {
  return pointer(new https_connection(io_context, logger, conf));
}

#define BAD_CONNECT_STATE_ERROR(error_string)                      \
  std::string detail =                                             \
      fmt::format(error_string, *_conf, state_to_str(expected));   \
  SPDLOG_LOGGER_ERROR(_logger, detail);                            \
  _io_context->post([cb = std::move(callback), detail]() {         \
    cb(std::make_error_code(std::errc::invalid_argument), detail); \
  });                                                              \
  return;

void https_connection::connect(connect_callback_type&& callback) {
  unsigned expected = e_not_connected;
  if (!_state.compare_exchange_strong(expected, e_connecting)) {
    BAD_CONNECT_STATE_ERROR("can connect to {}, bad state {}");
  }

  SPDLOG_LOGGER_DEBUG(_logger, "connect to {}", *_conf);
  beast::get_lowest_layer(_stream).expires_after(_conf->get_connect_timeout());
  beast::get_lowest_layer(_stream).async_connect(
      _conf->get_endpoint(),
      [me = shared_from_this(), cb = std::move(callback)](
          const beast::error_code& err) mutable { me->on_connect(err, cb); });
}

void https_connection::on_connect(const beast::error_code& err,
                                  connect_callback_type& callback) {
  if (err) {
    std::string detail = fmt::format("fail connect to {}: {}",
                                     _conf->get_endpoint(), err.message());
    SPDLOG_LOGGER_ERROR(_logger, detail);
    callback(err, detail);
    shutdown();
    return;
  }
  unsigned expected = e_connecting;
  if (!_state.compare_exchange_strong(expected, e_handshake)) {
    BAD_CONNECT_STATE_ERROR("on_tcp_connect to {}, bad state {}");
  }

  boost::system::error_code err_keep_alive;
  asio::socket_base::keep_alive opt1(true);
  beast::get_lowest_layer(_stream).socket().set_option(opt1, err_keep_alive);
  if (err_keep_alive) {
    SPDLOG_LOGGER_ERROR(_logger, "fail to activate keep alive for {}", *_conf);
  } else {
    tcp_keep_alive_interval opt2(_conf->get_second_tcp_keep_alive_interval());
    beast::get_lowest_layer(_stream).socket().set_option(opt2, err_keep_alive);
    if (err_keep_alive) {
      SPDLOG_LOGGER_ERROR(_logger, "fail to modify keep alive interval for {}",
                          *_conf);
    }
    tcp_keep_alive_idle opt3(_conf->get_second_tcp_keep_alive_interval());
    beast::get_lowest_layer(_stream).socket().set_option(opt3, err_keep_alive);
    if (err_keep_alive) {
      SPDLOG_LOGGER_ERROR(
          _logger, "fail to modify first keep alive delay for {}", *_conf);
    }
  }
  SPDLOG_LOGGER_DEBUG(_logger, "connected to {}", _conf->get_endpoint());

  // Perform the SSL handshake
  _stream.async_handshake(
      asio::ssl::stream_base::client,
      [me = shared_from_this(), cb = std::move(callback)](
          const beast::error_code& err) { me->on_handshake(err, cb); });
}

void https_connection::on_handshake(const beast::error_code err,
                                    const connect_callback_type& callback) {
  if (err) {
    std::string detail = fmt::format("fail handshake to {}: {}",
                                     _conf->get_endpoint(), err.message());
    SPDLOG_LOGGER_ERROR(_logger, detail);
    callback(err, detail);
    shutdown();
    return;
  }

  unsigned expected = e_handshake;
  if (!_state.compare_exchange_strong(expected, e_idle)) {
    BAD_CONNECT_STATE_ERROR("on_handshake to {}, bad state {}");
  }
  SPDLOG_LOGGER_DEBUG(_logger, "handshake done to {}", _conf->get_endpoint());
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

void https_connection::send(request_ptr request,
                            send_callback_type&& callback) {
  unsigned expected = e_idle;
  if (!_state.compare_exchange_strong(expected, e_send)) {
    BAD_SEND_STATE_ERROR("send to {}, bad state {}");
  }

  request->_send = system_clock::now();
  SPDLOG_LOGGER_DEBUG(_logger, "send request to {}", _conf->get_endpoint());

  beast::get_lowest_layer(_stream).expires_after(_conf->get_send_timeout());
  beast::http::async_write(
      _stream, *request,
      [me = shared_from_this(), request, cb = std::move(callback)](
          const beast::error_code& err, size_t bytes_transfered) mutable {
        me->on_sent(err, request, cb);
      });
}

void https_connection::on_sent(const beast::error_code& err,
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

  request->_sent = system_clock::now();

  response_ptr resp = std::make_shared<response_type>();
  beast::http::async_read(
      _stream, _recv_buffer, *resp,
      [me = shared_from_this(), request, cb = std::move(callback), resp](
          const beast::error_code& ec, std::size_t) mutable {
        me->on_read(ec, request, cb, resp);
      });
}

void https_connection::on_read(const beast::error_code& err,
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

  request->_receive = system_clock::now();

  if (resp->result_int() >= 400) {
    SPDLOG_LOGGER_ERROR(_logger, "err response for {} \n\n {}", *request,
                        *resp);
  } else {
    SPDLOG_LOGGER_DEBUG(_logger, "recv response from {} {}", *_conf, *resp);
  }
  gest_keepalive(resp);

  callback(err, {}, resp);
}

void https_connection::shutdown() {
  unsigned old_state = _state.exchange(e_shutdown);
  if (old_state != e_shutdown) {
    SPDLOG_LOGGER_DEBUG(_logger, "begin shutdown {}", *_conf);
    _state = e_shutdown;
    _stream.async_shutdown(
        [me = shared_from_this()](const beast::error_code& err) {
          if (err) {
            SPDLOG_LOGGER_ERROR(me->_logger, "fail shutdown to {}: {}",
                                *me->_conf, err.message());
          }
          me->_state = e_not_connected;
        });
  }
}
