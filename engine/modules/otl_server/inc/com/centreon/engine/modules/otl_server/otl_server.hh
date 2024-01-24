/*
** Copyright 2024 Centreon
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

#ifndef CCE_MOD_OTL_SERVER_OTLSERVER_HH
#define CCE_MOD_OTL_SERVER_OTLSERVER_HH

#include "grpc_config.hh"

namespace com::centreon::engine::modules::otl_server {

namespace detail {
class metric_service;
};

using metric_ptr = std::shared_ptr<::opentelemetry::proto::collector::metrics::
                                       v1::ExportMetricsServiceRequest>;

/**
 * @brief the server grpc model used is the callback model
 * So you need to give to the server this handler to handle incoming requests
 *
 */
using metric_handler = std::function<void(const metric_ptr&)>;

/**
 * @brief grpc metric receiver server
 * must be constructed with load method
 *
 */
class otl_server {
  grpc_config::pointer _conf;
  std::unique_ptr<::grpc::Server> _server;
  std::unique_ptr<detail::metric_service> _service;
  absl::Mutex _protect;

  otl_server(const grpc_config::pointer& conf, const metric_handler& handler);
  void start();

 public:
  using pointer = std::shared_ptr<otl_server>;

  ~otl_server();

  void shutdown(const std::chrono::system_clock::duration& timeout);

  static pointer load(const grpc_config::pointer& conf,
                      const metric_handler& handler);
};

}  // namespace com::centreon::engine::modules::otl_server

#endif
