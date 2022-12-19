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
using request_ptr = std::shared_ptr<request_type>;
using response_type =
    boost::beast::http::response<boost::beast::http::string_body>;
using response_ptr = std::shared_ptr<response_type>;

using send_callback_type = std::function<void(const boost::beast::error_code&,
                                              const std::string&,
                                              const response_ptr&)>;

class connection : public std::enable_shared_from_this<connection> {
  std::shared_ptr<asio::io_context> _io_context;
  std::shared_ptr<spdlog::logger> _logger;

  std::atomic_uint _state;
  time_point _keep_alive_end;

  boost::beast::tcp_stream _socket;
  std::mutex _socket_m;

  boost::beast::flat_buffer _recv_buffer;

  http_config::pointer _conf;

 public:
  using connect_callback_type =
      std::function<void(const boost::beast::error_code&, const std::string&)>;

  enum e_state { e_not_connected, e_connecting, e_idle, e_send, e_receive };

 protected:
  connection(const std::shared_ptr<asio::io_context>& io_context,
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
  using pointer = std::shared_ptr<connection>;

  static pointer load(const std::shared_ptr<asio::io_context>& io_context,
                      const std::shared_ptr<spdlog::logger>& logger,
                      const http_config::pointer& conf);

  ~connection();

  void shutdown();

  void connect(connect_callback_type&& callback);

  void send(request_ptr request, send_callback_type&& callback);

  e_state get_state() const { return e_state(_state.load()); }

  bool available(const time_point& now) {
    return _state == e_idle && _keep_alive_end > now;
  }

  time_point get_keep_alive_end() const { return _keep_alive_end; }
};

}  // namespace http_client

CCB_END()

#endif  // CCB_HTTP_CLIENT_CONNEXION_HH__
