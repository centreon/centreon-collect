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

#include "https_connection.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::http_client;

https_connection::https_connection(
    const std::shared_ptr<asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    const http_config::pointer& conf)
    : connection_base(io_context, logger, conf) {
  SPDLOG_LOGGER_DEBUG(_logger, "create https_connection to {}", *conf);
}

https_connection::~https_connection() {
  SPDLOG_LOGGER_DEBUG(_logger, "delete https_connection to {}", *_conf);
  shutdown();
}

https_connection::pointer https_connection::load(
    const std::shared_ptr<asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    const http_config::pointer& conf) {
  return pointer(new https_connection(io_context, logger, conf));
}

void https_connection::connect(connect_callback_type&& callback) {}

void https_connection::on_connect(const boost::beast::error_code& err,
                                  const connect_callback_type& callback) {}

void https_connection::send(request_ptr request,
                            send_callback_type&& callback) {}

void https_connection::on_sent(const boost::beast::error_code& err,
                               request_ptr request,
                               send_callback_type& callback) {}

void https_connection::on_read(const boost::beast::error_code& err,
                               const request_ptr& request,
                               send_callback_type& callback,
                               const response_ptr& resp) {}

void https_connection::shutdown() {}
