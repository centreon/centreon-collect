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

#ifndef CCE_MOD_OTL_CENTREON_AGENT_AGENT_CLIENT_HH
#define CCE_MOD_OTL_CENTREON_AGENT_AGENT_CLIENT_HH

#include "centreon_agent/agent.grpc.pb.h"
#include "centreon_agent/agent_stat.hh"
#include "com/centreon/engine/modules/opentelemetry/centreon_agent/agent_config.hh"

#include "com/centreon/common/grpc/grpc_client.hh"
#include "com/centreon/engine/modules/opentelemetry/otl_data_point.hh"

namespace com::centreon::engine::modules::opentelemetry::centreon_agent {

class agent_connection;

/**
 * @brief this class is used in case of reverse connection
 * it maintains one connection to agent server and reconnect in case of failure
 *
 */
class to_agent_connector
    : public common::grpc::grpc_client_base,
      public std::enable_shared_from_this<to_agent_connector> {
  std::shared_ptr<boost::asio::io_context> _io_context;
  metric_handler _metric_handler;
  agent_config::pointer _conf;

  bool _alive;
  std::unique_ptr<agent::ReversedAgentService::Stub> _stub;

  absl::Mutex _connection_m;
  std::shared_ptr<agent_connection> _connection ABSL_GUARDED_BY(_connection_m);

  agent_stat::pointer _stats;

 public:
  to_agent_connector(const grpc_config::pointer& agent_endpoint_conf,
                     const std::shared_ptr<boost::asio::io_context>& io_context,
                     const agent_config::pointer& agent_conf,
                     const metric_handler& handler,
                     const std::shared_ptr<spdlog::logger>& logger,
                     const agent_stat::pointer& stats);

  virtual ~to_agent_connector();

  virtual void start();

  static std::shared_ptr<to_agent_connector> load(
      const grpc_config::pointer& agent_endpoint_conf,
      const std::shared_ptr<boost::asio::io_context>& io_context,
      const agent_config::pointer& agent_conf,
      const metric_handler& handler,
      const std::shared_ptr<spdlog::logger>& logger,
      const agent_stat::pointer& stats);

  void refresh_agent_configuration_if_needed(
      const agent_config::pointer& new_conf);

  virtual void shutdown();

  void on_error();
};

}  // namespace com::centreon::engine::modules::opentelemetry::centreon_agent

#endif
