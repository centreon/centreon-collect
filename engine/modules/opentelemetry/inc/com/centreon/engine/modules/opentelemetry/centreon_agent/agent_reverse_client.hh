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

#ifndef CCE_MOD_OTL_CENTREON_AGENT_AGENT_REVERSE_CLIENT_HH
#define CCE_MOD_OTL_CENTREON_AGENT_AGENT_REVERSE_CLIENT_HH

#include "com/centreon/engine/modules/opentelemetry/centreon_agent/agent_config.hh"
#include "com/centreon/engine/modules/opentelemetry/centreon_agent/agent_stat.hh"
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

  agent_stat::pointer _agent_stats;

  virtual config_to_client::iterator _create_new_client_connection(
      const grpc_config::pointer& agent_endpoint,
      const agent_config::pointer& agent_conf)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(_agents_m);

  virtual void _shutdown_connection(config_to_client::const_iterator to_delete);

 public:
  agent_reverse_client(
      const std::shared_ptr<boost::asio::io_context>& io_context,
      const metric_handler& handler,
      const std::shared_ptr<spdlog::logger>& logger,
      const agent_stat::pointer& stats);

  virtual ~agent_reverse_client();

  void update(const agent_config::pointer& new_conf);
};

}  // namespace com::centreon::engine::modules::opentelemetry::centreon_agent

#endif
