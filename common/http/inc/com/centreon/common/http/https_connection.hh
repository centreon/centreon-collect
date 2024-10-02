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

#ifndef CCB_HTTPS_CLIENT_CONNECTION_HH__
#define CCB_HTTPS_CLIENT_CONNECTION_HH__

#include <boost/beast/ssl.hpp>
#include "http_connection.hh"

namespace com::centreon::common::http {

/**
 * @brief https version of http_connection
 * interface is the same
 * internaly there is an additional stage handshake (ssl negotiation)
 * also the shutdown is more complicated as we have to signal it with ssl
 * protocol.
 * Fortunaltly, asio gives us an async_shutdown that do the job
 */
class https_connection : public connection_base {
 protected:
  asio::ssl::context _sslcontext;
  using ssl_stream = boost::beast::ssl_stream<boost::beast::tcp_stream>;
  // when handshake fails, ssl_stream is in a dirty state so it's better to
  // recreate another object
  std::unique_ptr<ssl_stream> _stream;

  https_connection(const std::shared_ptr<asio::io_context>& io_context,
                   const std::shared_ptr<spdlog::logger>& logger,
                   const http_config::pointer& conf,
                   const ssl_ctx_initializer& ssl_init);

  void on_handshake(const boost::beast::error_code err,
                    const connect_callback_type& callback);

  void on_connect(const boost::beast::error_code& err,
                  connect_callback_type& callback);

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
  std::shared_ptr<https_connection> shared_from_this() {
    return std::static_pointer_cast<https_connection>(
        connection_base::shared_from_this());
  }

  static pointer load(const std::shared_ptr<asio::io_context>& io_context,
                      const std::shared_ptr<spdlog::logger>& logger,
                      const http_config::pointer& conf,
                      const ssl_ctx_initializer& ssl_init =
                          https_connection::load_client_certificate);

  ~https_connection();

  void shutdown() override;

  void connect(connect_callback_type&& callback) override;

  void send(request_ptr request, send_callback_type&& callback) override;

  void answer(const response_ptr& response,
              answer_callback_type&& callback) override;
  void receive_request(request_callback_type&& callback) override;

  asio::ip::tcp::socket& get_socket() override;

  static void load_client_certificate(asio::ssl::context& ctx,
                                      const http_config::pointer& conf);

  static void load_server_certificate(asio::ssl::context& ctx,
                                      const http_config::pointer& conf);
};

}  // namespace com::centreon::common::http

#endif  // CCB_HTTPS_CLIENT_CONNEXION_HH__
