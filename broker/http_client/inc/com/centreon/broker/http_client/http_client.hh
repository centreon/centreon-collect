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
/**
 * @class client
 * "broker/http_client/inc/com/centreon/broker/http_client/http_client.hh"
 *
 * @brief this class is the heart of the library
 * it dispatchs requests on connections to the server
 * it reuses also http keepalive connections
 * it also stacks in a fifo requests when all connections are in use
 * all is done asynchronous,
 * all tcp connections use tcp keepalive to maintain NATs tables
 * when you uses it, you must give a request and a callback
 * How to use it:
 * @code
 * auto clt = client::load(io_context, logger, conf);
 * clt->send(request,[](const boost::beast::error_code&,
                                              const std::string&,
                                              const response_ptr&) {});
 * @endcode
 *
 */
class client : public std::enable_shared_from_this<client> {
 public:
  using connection_creator = std::function<connection_base::pointer(
      const std::shared_ptr<asio::io_context>& io_context,
      const std::shared_ptr<spdlog::logger>& logger,
      const http_config::pointer& conf)>;

 protected:
  std::shared_ptr<asio::io_context> _io_context;
  std::shared_ptr<spdlog::logger> _logger;
  http_config::pointer _conf;

  using connection_cont = boost::container::flat_set<connection_base::pointer>;
  connection_cont _not_connected_conns;
  connection_cont _keep_alive_conns;
  connection_cont _busy_conns;

  duration _retry_unit;

  struct cb_request {
    using pointer = std::shared_ptr<cb_request>;
    cb_request(send_callback_type&& cb, const request_ptr& req)
        : callback(std::move(cb)), request(req), retry_counter(0) {}

    send_callback_type callback;
    request_ptr request;
    unsigned retry_counter;
  };

  std::deque<cb_request::pointer> _queue;

  bool _halt;

  mutable std::mutex _protect;

  client(const std::shared_ptr<asio::io_context>& io_context,
         const std::shared_ptr<spdlog::logger>& logger,
         const http_config::pointer& conf,
         connection_creator conn_creator);

  bool connect(const cb_request::pointer& request);

  void on_connect(const boost::beast::error_code& error,
                  const std::string& detail,
                  const cb_request::pointer& request,
                  connection_base::pointer conn);

  void on_sent(const boost::beast::error_code& error,
               const std::string& detail,
               const cb_request::pointer& request,
               const response_ptr& response,
               connection_base::pointer conn);

  void send_first_queue_request();

  void retry(const boost::beast::error_code& error,
             const std::string& detail,
             const cb_request::pointer& request,
             const response_ptr& response);

  bool send_or_push(const cb_request::pointer request,
                    bool push_to_front = false);

  void send(const cb_request::pointer& request, connection_base::pointer conn);

 public:
  using pointer = std::shared_ptr<client>;

  static pointer load(const std::shared_ptr<asio::io_context>& io_context,
                      const std::shared_ptr<spdlog::logger>& logger,
                      const http_config::pointer& conf,
                      connection_creator conn_creator = http_connection::load);

  template <class callback_type>
  bool send(const request_ptr& request, callback_type&& callback) {
    return send_or_push(std::make_shared<cb_request>(callback, request));
  }

  void shutdown();

  template <class visitor_type>
  void visit_queue(visitor_type& visitor) const;

  size_t get_nb_not_connected_cons() const {
    return _not_connected_conns.size();
  }
  size_t get_nb_keep_alive_conns() const { return _keep_alive_conns.size(); }
  size_t get_nb_busy_conns() const { return _busy_conns.size(); }
};

template <class visitor_type>
void client::visit_queue(visitor_type& visitor) const {
  std::lock_guard<std::mutex> l(_protect);
  for (const auto& cb : _queue) {
    visitor(*cb->request);
  }
}

}  // namespace http_client

CCB_END()

#endif  // CCB_HTTP_CLIENT_CLIENT_HH__
