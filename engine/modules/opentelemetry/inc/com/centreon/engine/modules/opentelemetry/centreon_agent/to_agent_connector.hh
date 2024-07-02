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

#ifndef CCE_MOD_OTL_CENTREON_AGENT_AGENT_CLIENT_HH
#define CCE_MOD_OTL_CENTREON_AGENT_AGENT_CLIENT_HH

#include "centreon_agent/agent.grpc.pb.h"
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

 public:
  to_agent_connector(const grpc_config::pointer& agent_endpoint_conf,
                     const std::shared_ptr<boost::asio::io_context>& io_context,
                     const agent_config::pointer& agent_conf,
                     const metric_handler& handler,
                     const std::shared_ptr<spdlog::logger>& logger);

  virtual ~to_agent_connector();

  virtual void start();

  static std::shared_ptr<to_agent_connector> load(
      const grpc_config::pointer& agent_endpoint_conf,
      const std::shared_ptr<boost::asio::io_context>& io_context,
      const agent_config::pointer& agent_conf,
      const metric_handler& handler,
      const std::shared_ptr<spdlog::logger>& logger);

  void refresh_agent_configuration_if_needed(
      const agent_config::pointer& new_conf);

  virtual void shutdown();

  void on_error();
};

}  // namespace com::centreon::engine::modules::opentelemetry::centreon_agent

#endif
