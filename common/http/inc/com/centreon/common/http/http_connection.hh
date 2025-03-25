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

#ifndef CCB_HTTP_CLIENT_CONNECTION_HH__
#define CCB_HTTP_CLIENT_CONNECTION_HH__

#include "http_config.hh"

namespace com::centreon::common::http {

/**
 * @brief we use a std::string body
 *
 */
using request_type =
    boost::beast::http::request<boost::beast::http::string_body>;
using response_type =
    boost::beast::http::response<boost::beast::http::string_body>;
using response_ptr = std::shared_ptr<response_type>;

/**
 * @brief the send request callback signature
 * it's used by users of this liabrary
 */
using send_callback_type = std::function<void(const boost::beast::error_code&,
                                              const std::string&,
                                              const response_ptr&)>;

/**
 * @brief the connect callback signature, it's used internaly not by users of
 * the library
 *
 */
using connect_callback_type =
    std::function<void(const boost::beast::error_code&, const std::string&)>;

/**
 * @brief called in server mode when response have been sent
 *
 */
using answer_callback_type =
    std::function<void(const boost::beast::error_code&, const std::string&)>;

/**
 * @brief called after a request have been received (server mode)
 *
 */
using request_callback_type =
    std::function<void(const boost::beast::error_code&,
                       const std::string&,
                       const std::shared_ptr<request_type>&)>;

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

/**
 * @brief this little class add statistics time points to request_type
 *
 */
class request_base : public request_type {
  time_point _connect;
  time_point _send;
  time_point _sent;
  time_point _receive;

 public:
  request_base();
  request_base(boost::beast::http::verb method,
               const std::string_view& server_name,
               boost::beast::string_view target);

  virtual ~request_base() {}
  friend class http_connection;
  friend class https_connection;
  friend class client;

  virtual void dump(std::ostream&) const;

  time_point get_connect_time() const { return _connect; }
  time_point get_send_time() const { return _send; }
  time_point get_sent_time() const { return _sent; }
  time_point get_receive_time() const { return _receive; }
};

inline std::ostream& operator<<(std::ostream& str, const request_base& req) {
  req.dump(str);
  return str;
}

using request_ptr = std::shared_ptr<request_base>;

/**
 * @brief this base class provides only keepalive implementation
 * caller can do 3 things: connect send and shutdown
 *
 */
class connection_base : public std::enable_shared_from_this<connection_base> {
 protected:
  asio::ip::tcp::endpoint _peer;

  // this atomic uint is used to atomically modify object state
  std::atomic_uint _state;
  // http keepalive expiration
  time_point _keep_alive_end;

  std::shared_ptr<asio::io_context> _io_context;
  std::shared_ptr<spdlog::logger> _logger;

  // asio socket are not thread safe
  std::mutex _socket_m;

  boost::beast::flat_buffer _recv_buffer;

  http_config::pointer _conf;

  virtual void _on_accept(connect_callback_type&& callback) = 0;

 public:
  enum e_state {
    e_not_connected,
    e_connecting,
    e_handshake,
    e_idle,
    e_send,
    e_receive,
    e_shutdown
  };

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

  // client version
  virtual void send(request_ptr request, send_callback_type&& callback) = 0;

  // server version
  /**
   * @brief to override in accepted session objects
   * It has to call _on_accept(connect_callback_type&& callback) in order to
   * receive
   *
   */
  virtual void on_accept() {}

  virtual void answer(const response_ptr& response,
                      answer_callback_type&& callback) = 0;
  virtual void receive_request(request_callback_type&& callback) = 0;

  e_state get_state() const { return e_state(_state.load()); }

  time_point get_keep_alive_end() const { return _keep_alive_end; }

  void gest_keepalive(const response_ptr& response);

  virtual asio::ip::tcp::socket& get_socket() = 0;

  asio::ip::tcp::endpoint& get_peer() { return _peer; }
  const asio::ip::tcp::endpoint& get_peer() const { return _peer; }

  virtual void add_keep_alive_to_server_response(
      const response_ptr& response) const;
};

/**
 * @brief This initializer is used by https client or server connection to init
 * ssl context In order to have the same signature, this parameter is passed to
 * http_connection::load with nullptr as default value even if it is not
 * used
 *
 */
using ssl_ctx_initializer =
    std::function<void(asio::ssl::context& ctx, const http_config::pointer&)>;

/**
 * @brief this functor is used by client and server to create needed connection
 * this constructor allow server and client to work either with http or https
 *
 */
using connection_creator = std::function<connection_base::pointer()>;

/**
 * @brief this class manages a tcp connection
 * it's used by client object to send request
 * constructor is protected because load is mandatory
 * this object mustn't be create on stack and must be allocated
 *
 */
class http_connection : public connection_base {
  boost::beast::tcp_stream _socket;

 protected:
  http_connection(const std::shared_ptr<asio::io_context>& io_context,
                  const std::shared_ptr<spdlog::logger>& logger,
                  const http_config::pointer& conf,
                  const ssl_ctx_initializer& ssl_initializer = nullptr);

  void on_connect(const boost::beast::error_code& err,
                  const connect_callback_type& callback);

  void init_keep_alive();

  void on_sent(const boost::beast::error_code& err,
               request_ptr request,
               send_callback_type& callback);

  void on_read(const boost::beast::error_code& err,
               const request_ptr& request,
               send_callback_type& callback,
               const response_ptr& resp);

  void _on_accept(connect_callback_type&& callback) override;

 public:
  std::shared_ptr<http_connection> shared_from_this() {
    return std::static_pointer_cast<http_connection>(
        connection_base::shared_from_this());
  }

  static pointer load(const std::shared_ptr<asio::io_context>& io_context,
                      const std::shared_ptr<spdlog::logger>& logger,
                      const http_config::pointer& conf,
                      const ssl_ctx_initializer& ssl_initializer = nullptr);

  ~http_connection();

  void shutdown() override;

  void connect(connect_callback_type&& callback) override;

  void send(request_ptr request, send_callback_type&& callback) override;

  void answer(const response_ptr& response,
              answer_callback_type&& callback) override;
  void receive_request(request_callback_type&& callback) override;

  asio::ip::tcp::socket& get_socket() override;
};

}  // namespace com::centreon::common::http

namespace fmt {

template <>
struct formatter<com::centreon::common::http::request_type>
    : ostream_formatter {};

template <>
struct formatter<com::centreon::common::http::request_base>
    : ostream_formatter {};

template <>
struct formatter<com::centreon::common::http::response_type>
    : ostream_formatter {};

}  // namespace fmt

#endif  // CCB_HTTP_CLIENT_CONNEXION_HH__
