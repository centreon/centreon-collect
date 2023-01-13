/*
** Copyright 2022 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#ifndef CCB_HTTP_CLIENT_CONNECTION_HH__
#define CCB_HTTP_CLIENT_CONNECTION_HH__

#include "http_config.hh"

CCB_BEGIN()

namespace http_client {

using request_type =
    boost::beast::http::request<boost::beast::http::string_body>;
using response_type =
    boost::beast::http::response<boost::beast::http::string_body>;
using response_ptr = std::shared_ptr<response_type>;

using send_callback_type = std::function<void(const boost::beast::error_code&,
                                              const std::string&,
                                              const response_ptr&)>;

using connect_callback_type =
    std::function<void(const boost::beast::error_code&, const std::string&)>;

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

class request_base : public request_type {
  time_point _connect;
  time_point _send;
  time_point _sent;
  time_point _receive;

 public:
  request_base();
  request_base(boost::beast::http::verb method,
               boost::beast::string_view target);

  friend class http_connection;
  friend class client;

  time_point get_connect_time() const { return _connect; }
  time_point get_send_time() const { return _send; }
  time_point get_sent_time() const { return _sent; }
  time_point get_receive_time() const { return _receive; }
};

using request_ptr = std::shared_ptr<request_base>;

class connection_base : public std::enable_shared_from_this<connection_base> {
 protected:
  std::atomic_uint _state;
  time_point _keep_alive_end;

  std::shared_ptr<asio::io_context> _io_context;
  std::shared_ptr<spdlog::logger> _logger;

  std::mutex _socket_m;

  boost::beast::flat_buffer _recv_buffer;

  http_config::pointer _conf;

 public:
  enum e_state { e_not_connected, e_connecting, e_idle, e_send, e_receive };

  using pointer = std::shared_ptr<connection_base>;

  connection_base(const std::shared_ptr<asio::io_context>& io_context,
                  const std::shared_ptr<spdlog::logger>& logger,
                  const http_config::pointer& conf)
      : _state(e_not_connected),
        _keep_alive_end(system_clock::from_time_t(0)),
        _io_context(io_context),
        _logger(logger),
        _conf(conf) {}

  static std::string state_to_str(unsigned state);

  virtual ~connection_base() {}

  virtual void shutdown() = 0;

  virtual void connect(connect_callback_type&& callback) = 0;

  virtual void send(request_ptr request, send_callback_type&& callback) = 0;

  e_state get_state() const { return e_state(_state.load()); }

  time_point get_keep_alive_end() const { return _keep_alive_end; }

  void gest_keepalive(const response_ptr& response);
};

class http_connection : public connection_base {
  boost::beast::tcp_stream _socket;

 protected:
  http_connection(const std::shared_ptr<asio::io_context>& io_context,
                  const std::shared_ptr<spdlog::logger>& logger,
                  const http_config::pointer& conf);

  void on_connect(const boost::beast::error_code& err,
                  const connect_callback_type& callback);

  void on_sent(const boost::beast::error_code& err,
               request_ptr request,
               send_callback_type& callback);

  void on_read(const boost::beast::error_code& err,
               const request_ptr& request,
               send_callback_type& callback,
               const response_ptr& resp);

 public:
  std::shared_ptr<http_connection> shared_from_this() {
    return std::static_pointer_cast<http_connection>(
        connection_base::shared_from_this());
  }

  static pointer load(const std::shared_ptr<asio::io_context>& io_context,
                      const std::shared_ptr<spdlog::logger>& logger,
                      const http_config::pointer& conf);

  ~http_connection();

  void shutdown() override;

  void connect(connect_callback_type&& callback) override;

  void send(request_ptr request, send_callback_type&& callback) override;
};

}  // namespace http_client

CCB_END()

namespace fmt {

template <>
struct formatter<com::centreon::broker::http_client::request_base>
    : ostream_formatter {};

template <>
struct formatter<com::centreon::broker::http_client::response_type>
    : ostream_formatter {};

}  // namespace fmt

#endif  // CCB_HTTP_CLIENT_CONNEXION_HH__
