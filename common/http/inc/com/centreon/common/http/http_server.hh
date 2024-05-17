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

#ifndef CCB_HTTP_SERVER_HH__
#define CCB_HTTP_SERVER_HH__

#include "http_config.hh"
#include "http_connection.hh"

namespace com::centreon::common::http {

/**
 * @brief This class is an http(s) server
 * the difference between http and https server is the connection builder passed
 * in constructor parameter
 *
 */
class server : public std::enable_shared_from_this<server> {
 private:
  const std::shared_ptr<asio::io_context> _io_context;
  const std::shared_ptr<spdlog::logger> _logger;
  http_config::pointer _conf;
  connection_creator _conn_creator;
  asio::ip::tcp::acceptor _acceptor;
  std::mutex _acceptor_m;

  void start_accept();

  void on_accept(const boost::beast::error_code& err,
                 const connection_base::pointer& conn);

 public:
  using pointer = std::shared_ptr<server>;

  server(const std::shared_ptr<asio::io_context>& io_context,
         const std::shared_ptr<spdlog::logger>& logger,
         const http_config::pointer& conf,
         connection_creator&& conn_creator);

  ~server();

  static pointer load(const std::shared_ptr<asio::io_context>& io_context,
                      const std::shared_ptr<spdlog::logger>& logger,
                      const http_config::pointer& conf,
                      connection_creator&& conn_creator);

  void shutdown();
};

}  // namespace com::centreon::common::http

#endif
