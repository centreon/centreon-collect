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

#ifndef CCE_MOD_OTL_CENTREON_AGENT_AGENT_SERVICE_HH
#define CCE_MOD_OTL_CENTREON_AGENT_AGENT_SERVICE_HH

#include "centreon_agent/agent.grpc.pb.h"
#include "com/centreon/engine/modules/opentelemetry/centreon_agent/agent_config.hh"
#include "com/centreon/engine/modules/opentelemetry/centreon_agent/agent_impl.hh"

namespace com::centreon::engine::modules::opentelemetry::centreon_agent {

/**
 * @brief this class is a grpc service provided by otel_server for incoming
 * centreon monitoring agent connection
 *
 */
class agent_service : public agent::AgentService::Service,
                      public std::enable_shared_from_this<agent_service> {
  std::shared_ptr<boost::asio::io_context> _io_context;
  agent_config::pointer _conf;
  absl::Mutex _conf_m;

  metric_handler _metric_handler;
  std::shared_ptr<spdlog::logger> _logger;

  agent_stat::pointer _stats;

 public:
  agent_service(const std::shared_ptr<boost::asio::io_context>& io_context,
                const agent_config::pointer& conf,
                const metric_handler& handler,
                const std::shared_ptr<spdlog::logger>& logger,
                const agent_stat::pointer& stats);

  void init();

  static std::shared_ptr<agent_service> load(
      const std::shared_ptr<boost::asio::io_context>& io_context,
      const agent_config::pointer& conf,
      const metric_handler& handler,
      const std::shared_ptr<spdlog::logger>& logger,
      const agent_stat::pointer& stats);

  // disable synchronous version of this method
  ::grpc::Status Export(
      ::grpc::ServerContext* /*context*/,
      ::grpc::ServerReaderWriter<agent::MessageToAgent,
                                 agent::MessageFromAgent>* /*stream*/)
      override {
    abort();
    return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
  }

  ::grpc::ServerBidiReactor<agent::MessageFromAgent, agent::MessageToAgent>*
  Export(::grpc::CallbackServerContext* context);

  void update(const agent_config::pointer& conf);

  static void shutdown_all_accepted();
};

}  // namespace com::centreon::engine::modules::opentelemetry::centreon_agent

#endif
