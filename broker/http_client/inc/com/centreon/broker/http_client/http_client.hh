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

#ifndef CCB_HTTP_CLIENT_CLIENT_HH__
#define CCB_HTTP_CLIENT_CLIENT_HH__

#include "http_connection.hh"

CCB_BEGIN()

namespace http_client {

class client : public std::enable_shared_from_this<client> {
 protected:
  std::shared_ptr<asio::io_context> _io_context;
  std::shared_ptr<spdlog::logger> _logger;
  http_config::pointer _conf;

  using connection_cont = boost::container::flat_set<connection_base::pointer>;
  connection_cont _not_connected_conns;
  connection_cont _keep_alive_conns;
  connection_cont _busy_conns;

  struct cb_request {
    using pointer = std::shared_ptr<cb_request>;
    cb_request(send_callback_type& cb, const request_ptr& req)
        : callback(std::move(cb)), request(req), retry_counter(0) {}

    send_callback_type callback;
    request_ptr request;
    unsigned retry_counter;
  };

  std::deque<cb_request::pointer> _queue;

  asio::system_timer _retry_timer;
  bool _retry_timer_active;
  duration _retry_interval;

  bool _halt;

  mutable std::mutex _protect;

  using connection_creator = std::function<connection_base::pointer(
      const std::shared_ptr<asio::io_context>& io_context,
      const std::shared_ptr<spdlog::logger>& logger,
      const http_config::pointer& conf)>;

  client(const std::shared_ptr<asio::io_context>& io_context,
         const std::shared_ptr<spdlog::logger>& logger,
         const http_config::pointer& conf,
         unsigned max_connections,
         connection_creator conn_creator);

  bool connect();

  void on_connect(const boost::beast::error_code& error,
                  const std::string& detail,
                  connection_base::pointer conn);

  void on_sent(const boost::beast::error_code& error,
               const std::string& detail,
               const cb_request::pointer& request,
               const response_ptr& response,
               connection_base::pointer conn);

  void start_retry_connect_timer();
  void retry_connect_timer_handler(const boost::system::error_code& err);

  void send_first_queue_request(connection_base::pointer conn);

 public:
  using pointer = std::shared_ptr<client>;

  static pointer load(const std::shared_ptr<asio::io_context>& io_context,
                      const std::shared_ptr<spdlog::logger>& logger,
                      const http_config::pointer& conf,
                      unsigned max_connections,
                      connection_creator conn_creator = http_connection::load);

  bool send(const request_ptr& request, send_callback_type&& callback);

  void shutdown();
};
}  // namespace http_client

CCB_END()

#endif  // CCB_HTTP_CLIENT_CLIENT_HH__
