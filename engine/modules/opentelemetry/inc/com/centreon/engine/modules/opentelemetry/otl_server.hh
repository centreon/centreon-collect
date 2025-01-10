/**
 * Copyright 2024 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */

#ifndef CCE_MOD_OTL_SERVER_OTLSERVER_HH
#define CCE_MOD_OTL_SERVER_OTLSERVER_HH

#include "grpc_config.hh"

#include "otl_data_point.hh"

#include "com/centreon/common/grpc/grpc_server.hh"
#include "com/centreon/engine/modules/opentelemetry/centreon_agent/agent_service.hh"

namespace com::centreon::engine::modules::opentelemetry {

namespace detail {
class metric_service;
};

/**
 * @brief grpc metric receiver server
 * must be constructed with load method
 *
 */
class otl_server : public common::grpc::grpc_server_base {
  std::shared_ptr<detail::metric_service> _service;
  std::shared_ptr<centreon_agent::agent_service> _agent_service;
  absl::Mutex _protect;

  otl_server(const std::shared_ptr<boost::asio::io_context>& io_context,
             const grpc_config::pointer& conf,
             const centreon_agent::agent_config::pointer& agent_config,
             const metric_handler& handler,
             const std::shared_ptr<spdlog::logger>& logger,
             const centreon_agent::agent_stat::pointer& agent_stats);
  void start();

 public:
  using pointer = std::shared_ptr<otl_server>;

  ~otl_server();

  static pointer load(
      const std::shared_ptr<boost::asio::io_context>& io_context,
      const grpc_config::pointer& conf,
      const centreon_agent::agent_config::pointer& agent_config,
      const metric_handler& handler,
      const std::shared_ptr<spdlog::logger>& logger,
      const centreon_agent::agent_stat::pointer& agent_stats);

  void update_agent_config(
      const centreon_agent::agent_config::pointer& agent_config);
};

}  // namespace com::centreon::engine::modules::opentelemetry

#endif
