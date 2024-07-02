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

#ifndef CCE_MOD_OTL_CENTREON_AGENT_AGENT_REVERSE_CLIENT_HH
#define CCE_MOD_OTL_CENTREON_AGENT_AGENT_REVERSE_CLIENT_HH

#include "com/centreon/engine/modules/opentelemetry/centreon_agent/agent_config.hh"
#include "com/centreon/engine/modules/opentelemetry/otl_data_point.hh"

namespace com::centreon::engine::modules::opentelemetry::centreon_agent {

class to_agent_connector;

class agent_reverse_client {
 protected:
  std::shared_ptr<boost::asio::io_context> _io_context;
  agent_config::pointer _conf;
  const metric_handler _metric_handler;
  std::shared_ptr<spdlog::logger> _logger;

  using config_to_client = absl::btree_map<grpc_config::pointer,
                                           std::shared_ptr<to_agent_connector>,
                                           grpc_config_compare>;
  absl::Mutex _agents_m;
  config_to_client _agents ABSL_GUARDED_BY(_agents_m);

  virtual config_to_client::iterator _create_new_client_connection(
      const grpc_config::pointer& agent_endpoint,
      const agent_config::pointer& agent_conf)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(_agents_m);

  virtual void _shutdown_connection(config_to_client::const_iterator to_delete);

 public:
  agent_reverse_client(
      const std::shared_ptr<boost::asio::io_context>& io_context,
      const metric_handler& handler,
      const std::shared_ptr<spdlog::logger>& logger);

  virtual ~agent_reverse_client();

  void update(const agent_config::pointer& new_conf);
};

}  // namespace com::centreon::engine::modules::opentelemetry::centreon_agent

#endif
