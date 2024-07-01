/*
 * Copyright 2024 Centreon
 *
 * This file is part of Centreon Engine.
 *
 * Centreon Engine is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * Centreon Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Centreon Engine. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef CCE_MOD_OTL_CENTREON_AGENT_AGENT_SERVICE_HH
#define CCE_MOD_OTL_CENTREON_AGENT_AGENT_SERVICE_HH

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

 public:
  agent_service(const std::shared_ptr<boost::asio::io_context>& io_context,
                const agent_config::pointer& conf,
                const metric_handler& handler,
                const std::shared_ptr<spdlog::logger>& logger);

  void init();

  static std::shared_ptr<agent_service> load(
      const std::shared_ptr<boost::asio::io_context>& io_context,
      const agent_config::pointer& conf,
      const metric_handler& handler,
      const std::shared_ptr<spdlog::logger>& logger);

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
