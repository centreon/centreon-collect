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

#include <boost/serialization/singleton.hpp>

#include <absl/container/flat_hash_map.h>

#include "https_connection.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::common::http;

namespace beast = boost::beast;

namespace com::centreon::common::http::detail {

/**
 * @brief to avoid read certificate on each request, this singleton do the job
 * it maintains a certificate cache
 * a certificate is reload if it's updated on disk
 * certificates are removed from cached after 1 hour without use it
 * The interface is composed of the get_certificate method
 */
class certificate_cache
    : public boost::serialization::singleton<certificate_cache> {
  struct certificate {
    time_t last_access;
    std::shared_ptr<std::string> content;
  };

  // certificates are pointed by their path
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
    if (exist != _certs.end()) {  // yet in cache
      struct stat file_stat;
      if (stat(path.c_str(), &file_stat)) {  // file removed?
        throw msg_fmt("stat fail of certificate: {} : {}", path,
                      strerror(errno));
      }
      if (file_stat.st_mtim.tv_sec >
          exist->second.last_access) {  // modified on disk => reload}
        _certs.erase(exist);
      } else {  // not modified => return cache content
        exist->second.last_access = now;
        clean();
        return exist->second.content;
      }
    }
    clean();
  }

  // not in cache => load from disk
  std::ifstream file(path);
  if (file.is_open()) {
    try {
      std::stringstream ss;
      ss << file.rdbuf();
      file.close();
      std::lock_guard<std::mutex> l(_protect);
      auto content = std::make_shared<std::string>(ss.str());
      _certs[path] = {now, content};
      return content;
    } catch (const std::exception& e) {
      throw msg_fmt("Cannot read certificate file '{}': {}", path, e.what());
    }
  } else {
    throw msg_fmt("Cannot open certificate file '{}': {}", path,
                  strerror(errno));
  }
}

/**
 * @brief clean unused certificate
 *
 */
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

}  // namespace com::centreon::common::http::detail

/**
 * @brief Construct a new https connection::https connection object
 * it's also load certificate if a non empty certificate path is provided
 *
 * @param io_context
 * @param logger
 * @param conf
 */
https_connection::https_connection(
    const std::shared_ptr<asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    const http_config::pointer& conf,
    const ssl_ctx_initializer& ssl_init)
    : connection_base(io_context, logger, conf),
      _sslcontext(conf->get_ssl_method()) {
  try {
    if (ssl_init)
      ssl_init(_sslcontext, conf);
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(logger, "fail to init ssl context: {}", e.what());
  }
  _stream = std::make_unique<ssl_stream>(beast::net::make_strand(*io_context),
                                         _sslcontext);
  if (!SSL_set_tlsext_host_name(_stream->native_handle(),
                                conf->get_server_name().c_str())) {
    beast::error_code ec{static_cast<int>(::ERR_get_error()),
                         boost::beast::net::error::get_ssl_category()};
    SPDLOG_LOGGER_ERROR(logger, "Failed to initialize the https connection: {}",
                        ec.message());
  }
  SPDLOG_LOGGER_DEBUG(_logger, "create https_connection {:p} to {}",
                      static_cast<void*>(this), *conf);
}

https_connection::~https_connection() {
  SPDLOG_LOGGER_DEBUG(_logger, "delete https_connection {:p} to {}",
                      static_cast<void*>(this), *_conf);
}

/**
 * @brief static method to use instead of constructor
 *
 * @param io_context
 * @param logger
 * @param conf
 * @return https_connection::pointer
 */
https_connection::pointer https_connection::load(
    const std::shared_ptr<asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    const http_config::pointer& conf,
    const ssl_ctx_initializer& ssl_init) {
  return pointer(new https_connection(io_context, logger, conf, ssl_init));
}

#define BAD_CONNECT_STATE_ERROR(error_string)                      \
  std::string detail =                                             \
      fmt::format(error_string, *_conf, state_to_str(expected));   \
  SPDLOG_LOGGER_ERROR(_logger, detail);                            \
  _io_context->post([cb = std::move(callback), detail]() {         \
    cb(std::make_error_code(std::errc::invalid_argument), detail); \
  });                                                              \
  return;

/**
 * @brief tcp connect
 *
 * @param callback
 */
void https_connection::connect(connect_callback_type&& callback) {
  unsigned expected = e_not_connected;
  if (!_state.compare_exchange_strong(expected, e_connecting)) {
    BAD_CONNECT_STATE_ERROR("can connect to {}, bad state {}");
  }

  SPDLOG_LOGGER_DEBUG(_logger, "{:p} connect to {}", static_cast<void*>(this),
                      *_conf);
  std::lock_guard<std::mutex> l(_socket_m);
  beast::get_lowest_layer(*_stream).expires_after(_conf->get_connect_timeout());
  if (_conf->get_endpoints_list().empty())
    beast::get_lowest_layer(*_stream).async_connect(
        _conf->get_endpoint(),
        [me = shared_from_this(), cb = std::move(callback)](
            const beast::error_code& err) mutable { me->on_connect(err, cb); });
  else
    beast::get_lowest_layer(*_stream).async_connect(
        _conf->get_endpoints_list(),
        [me = shared_from_this(), cb = std::move(callback)](
            const beast::error_code& err,
            const asio::ip::tcp::endpoint& endpoint
            [[maybe_unused]]) mutable { me->on_connect(err, cb); });
}

/**
 * @brief connect handler and tcp keepalive setter
 *
 * @param err
 * @param callback
 */
void https_connection::on_connect(const beast::error_code& err,
                                  connect_callback_type& callback) {
  if (err) {
    std::string detail;
    if (_conf->get_endpoints_list().empty())
      detail = fmt::format("fail connect to {}: {}", _conf->get_endpoint(),
                           err.message());
    else
      detail = fmt::format("fail connect to {}: {}", _conf->get_server_name(),
                           err.message());
    SPDLOG_LOGGER_ERROR(_logger, detail);
    callback(err, detail);
    shutdown();
    return;
  }
  unsigned expected = e_connecting;
  if (!_state.compare_exchange_strong(expected, e_handshake)) {
    BAD_CONNECT_STATE_ERROR("on_tcp_connect to {}, bad state {}");
  }

  std::lock_guard<std::mutex> l(_socket_m);

  boost::system::error_code ec;
  _peer = beast::get_lowest_layer(*_stream).socket().remote_endpoint(ec);

  init_keep_alive();
  SPDLOG_LOGGER_DEBUG(_logger, "{:p} connected to {}", static_cast<void*>(this),
                      _peer);

  // Perform the SSL handshake
  _stream->async_handshake(
      asio::ssl::stream_base::client,
      [me = shared_from_this(), cb = std::move(callback)](
          const beast::error_code& err) { me->on_handshake(err, cb); });
}

void https_connection::init_keep_alive() {
  boost::system::error_code err_keep_alive;
  asio::socket_base::keep_alive opt1(true);
  beast::get_lowest_layer(*_stream).socket().set_option(opt1, err_keep_alive);
  if (err_keep_alive) {
    SPDLOG_LOGGER_ERROR(_logger, "fail to activate keep alive for {}", *_conf);
  } else {
    tcp_keep_alive_interval opt2(_conf->get_second_tcp_keep_alive_interval());
    beast::get_lowest_layer(*_stream).socket().set_option(opt2, err_keep_alive);
    if (err_keep_alive) {
      SPDLOG_LOGGER_ERROR(_logger, "fail to modify keep alive interval for {}",
                          *_conf);
    }
    tcp_keep_alive_idle opt3(_conf->get_second_tcp_keep_alive_interval());
    beast::get_lowest_layer(*_stream).socket().set_option(opt3, err_keep_alive);
    if (err_keep_alive) {
      SPDLOG_LOGGER_ERROR(
          _logger, "fail to modify first keep alive delay for {}", *_conf);
    }
  }
}

/**
 * @brief to call in case of we are an http server
 * it initialize http keepalive and launch ssl handshake
 *
 * @param callback (called once handshake os done)
 */
void https_connection::_on_accept(connect_callback_type&& callback) {
  unsigned expected = e_not_connected;
  if (!_state.compare_exchange_strong(expected, e_handshake)) {
    BAD_CONNECT_STATE_ERROR("on_tcp_connect to {}, bad state {}");
  }

  SPDLOG_LOGGER_DEBUG(_logger, "{:p} accepted from {}",
                      static_cast<void*>(this), _peer);
  std::lock_guard<std::mutex> l(_socket_m);
  init_keep_alive();
  // Perform the SSL handshake
  _stream->async_handshake(
      asio::ssl::stream_base::server,
      [me = shared_from_this(), cb = std::move(callback)](
          const beast::error_code& err) { me->on_handshake(err, cb); });
}

/**
 * @brief ssl handshake handler
 *
 * @param err
 * @param callback
 */
void https_connection::on_handshake(const beast::error_code err,
                                    const connect_callback_type& callback) {
  if (err) {
    std::string detail =
        fmt::format("fail handshake with {}: {}", _peer, err.message());
    SPDLOG_LOGGER_ERROR(_logger, detail);
    callback(err, detail);
    shutdown();
    return;
  }

  unsigned expected = e_handshake;
  if (!_state.compare_exchange_strong(expected, e_idle)) {
    BAD_CONNECT_STATE_ERROR("on_handshake with {}, bad state {}");
  }
  SPDLOG_LOGGER_DEBUG(_logger, "{:p} handshake done with {}",
                      static_cast<void*>(this), _peer);
  callback(err, {});
}

#define BAD_SEND_STATE_ERROR(error_string)                              \
  std::string detail =                                                  \
      fmt::format(error_string, static_cast<const void*>(this), *_conf, \
                  state_to_str(expected));                              \
  SPDLOG_LOGGER_ERROR(_logger, detail);                                 \
  _io_context->post([cb = std::move(callback), detail]() {              \
    cb(std::make_error_code(std::errc::invalid_argument), detail,       \
       response_ptr());                                                 \
  });                                                                   \
  return;

/**
 * @brief send request over ssl stream
 *
 * @param request
 * @param callback
 */
void https_connection::send(request_ptr request,
                            send_callback_type&& callback) {
  unsigned expected = e_idle;
  if (!_state.compare_exchange_strong(expected, e_send)) {
    BAD_SEND_STATE_ERROR("{:p} send to {}, bad state {}");
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
  beast::get_lowest_layer(*_stream).expires_after(_conf->get_send_timeout());
  beast::http::async_write(
      *_stream, *request,
      [me = shared_from_this(), request, cb = std::move(callback)](
          const beast::error_code& err, size_t) mutable {
        me->on_sent(err, request, cb);
      });
}

/**
 * @brief send handler, if all is fine launch an async_read
 *
 * @param err
 * @param request
 * @param callback
 */
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
    BAD_SEND_STATE_ERROR("{:p} on_sent to {}, bad state {}");
  }

  if (_logger->level() == spdlog::level::trace) {
    SPDLOG_LOGGER_TRACE(_logger, "{} sent to {}", *request, *_conf);
  } else {
    SPDLOG_LOGGER_DEBUG(_logger, "request sent to {}", *_conf);
  }

  request->_sent = system_clock::now();

  std::lock_guard<std::mutex> l(_socket_m);
  if (_conf->get_receive_timeout() >= std::chrono::seconds(1)) {
    beast::get_lowest_layer(*_stream).expires_after(
        _conf->get_receive_timeout());
  }
  response_ptr resp = std::make_shared<response_type>();
  beast::http::async_read(
      *_stream, _recv_buffer, *resp,
      [me = shared_from_this(), request, cb = std::move(callback), resp](
          const beast::error_code& ec, std::size_t) mutable {
        me->on_read(ec, request, cb, resp);
      });
}

/**
 * @brief read handler
 *
 * @param err
 * @param request
 * @param callback
 * @param resp
 */
void https_connection::on_read(const beast::error_code& err,
                               const request_ptr& request,
                               send_callback_type& callback,
                               const response_ptr& resp) {
  if (err) {
    std::string detail = fmt::format("fail receive {} from {}: {}", *request,
                                     *_conf, err.message());
    if (err != boost::asio::error::operation_aborted) {
      SPDLOG_LOGGER_ERROR(_logger, detail);
    }
    callback(err, detail, response_ptr());
    shutdown();
    return;
  }

  unsigned expected = e_receive;
  if (!_state.compare_exchange_strong(expected, e_idle)) {
    BAD_SEND_STATE_ERROR("{:p} on_read to {}, bad state {}");
  }

  request->_receive = system_clock::now();

  if (resp->result_int() >= 400) {
    SPDLOG_LOGGER_ERROR(_logger, "err response for {} \n\n {}",
                        static_cast<request_type>(*request), *resp);
  } else {
    SPDLOG_LOGGER_DEBUG(_logger, "{:p} recv response from {} {}",
                        static_cast<void*>(this), *_conf, *resp);
  }
  gest_keepalive(resp);

  callback(err, {}, resp);
}

/**
 * @brief server part, send response to client
 * in case of receive time out < 1s, socket is closed after response sent
 *
 * @param response
 * @param callback
 */
void https_connection::answer(const response_ptr& response,
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
  beast::get_lowest_layer(*_stream).expires_after(_conf->get_send_timeout());
  beast::http::async_write(
      *_stream, *response,
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
 * @brief server part: wait for incoming request
 *
 * @param callback
 */
void https_connection::receive_request(request_callback_type&& callback) {
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
    beast::get_lowest_layer(*_stream).expires_after(
        _conf->get_receive_timeout());
  auto req = std::make_shared<request_type>();
  beast::http::async_read(
      *_stream, _recv_buffer, *req,
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
 * @brief perform an async shutdown
 * unlike http_connection synchronous shutdown can't be done
 *
 */
void https_connection::shutdown() {
  unsigned old_state = _state.exchange(e_shutdown);
  if (old_state != e_shutdown) {
    SPDLOG_LOGGER_DEBUG(_logger, "{:p} begin shutdown {}",
                        static_cast<void*>(this), *_conf);
    _state = e_shutdown;
    std::lock_guard<std::mutex> l(_socket_m);
    _stream->async_shutdown(
        [me = shared_from_this(), this](const beast::error_code& err) {
          if (err) {
            SPDLOG_LOGGER_ERROR(_logger, "fail shutdown to {}: {}", *_conf,
                                err.message());
          }
          boost::system::error_code shutdown_err;
          std::lock_guard<std::mutex> l(_socket_m);
          beast::get_lowest_layer(*_stream).socket().shutdown(
              asio::ip::tcp::socket::shutdown_both, shutdown_err);
          beast::get_lowest_layer(*_stream).close();
          _stream = std::make_unique<ssl_stream>(
              beast::net::make_strand(*_io_context), _sslcontext);
          _state = e_not_connected;
        });
  }
}

/**
 * @brief get underlay socket
 *
 * @return asio::ip::tcp::socket&
 */
asio::ip::tcp::socket& https_connection::get_socket() {
  return beast::get_lowest_layer(*_stream).socket();
}

/**
 * @brief This helper can be passed to https_connection constructor fourth param
 * when implementing https client as:
 * @code {.c++}
 * auto conn = https_connection::load(g_io_context, logger,
 * conf,(asio::ssl::context& ctx, const http_config::pointer & conf) {
 *   https_connection::load_client_certificate(ctx, conf);
 * });
 *
 * @endcode
 *
 *
 * @param ctx
 * @param conf
 */
void https_connection::load_client_certificate(
    asio::ssl::context& ctx,
    const http_config::pointer& conf) {
  if (!conf->get_certificate_path().empty()) {
    std::shared_ptr<std::string> cert_content =
        detail::certificate_cache::get_mutable_instance().get_certificate(
            conf->get_certificate_path());
    ctx.add_certificate_authority(
        asio::buffer(cert_content->c_str(), cert_content->length()));
  }
}

/**
 * @brief This helper can be passed to https_connection constructor fourth param
 * when implementing https server as:
 * @code {.c++}
 * auto conn = https_connection::load(g_io_context, logger,
 * conf,(asio::ssl::context& ctx, const http_config::pointer & conf) {
 *   https_connection::load_client_certificate(ctx, conf);
 * });
 *
 * @endcode
 * You can generate cert and key with this command:
 * @code {.shell}
 * openssl req -newkey rsa:2048 -nodes -keyout ../server.key -x509 -days 10000
 * -out ../server.crt
 * -subj "/C=US/ST=CA/L=Los Angeles/O=Beast/CN=www.example.com"
 * -addext "subjectAltName=DNS:localhost,IP:172.17.0.1"
 * @endcode
 *
 *
 * @param ctx
 * @param conf
 */
void https_connection::load_server_certificate(
    asio::ssl::context& ctx,
    const http_config::pointer& conf) {
  if (!conf->get_certificate_path().empty() && !conf->get_key_path().empty()) {
    std::shared_ptr<std::string> cert_content =
        detail::certificate_cache::get_mutable_instance().get_certificate(
            conf->get_certificate_path());
    std::shared_ptr<std::string> key_content =
        detail::certificate_cache::get_mutable_instance().get_certificate(
            conf->get_key_path());

    ctx.use_certificate_chain(
        boost::asio::buffer(cert_content->c_str(), cert_content->length()));
    ctx.use_private_key(
        boost::asio::buffer(key_content->c_str(), key_content->length()),
        boost::asio::ssl::context::file_format::pem);
    ctx.set_options(boost::asio::ssl::context::default_workarounds |
                    boost::asio::ssl::context::no_sslv2);
  }
}
