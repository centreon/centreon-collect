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

#include "centreon_agent/agent_reverse_client.hh"
#include "centreon_agent/to_agent_connector.hh"

using namespace com::centreon::engine::modules::opentelemetry::centreon_agent;

/**
 * @brief Construct a new agent reverse client::agent reverse client object
 *
 * @param io_context
 * @param handler handler that will process received metrics
 * @param logger
 */
agent_reverse_client::agent_reverse_client(
    const std::shared_ptr<boost::asio::io_context>& io_context,
    const metric_handler& handler,
    const std::shared_ptr<spdlog::logger>& logger,
    const agent_stat::pointer& stats)
    : _io_context(io_context),
      _metric_handler(handler),
      _logger(logger),
      _agent_stats(stats) {}

/**
 * @brief Destroy the agent reverse client::agent reverse client object
 * it also shutdown all connectors
 *
 */
agent_reverse_client::~agent_reverse_client() {
  absl::MutexLock l(&_agents_m);
  for (auto& conn : _agents) {
    conn.second->shutdown();
  }
  _agents.clear();
}

/**
 * @brief update agent list by doing a symmetric difference
 *
 * @param new_conf
 */
void agent_reverse_client::update(const agent_config::pointer& new_conf) {
  absl::MutexLock l(&_agents_m);

  auto connection_iterator = _agents.begin();

  if (!new_conf) {
    while (connection_iterator != _agents.end()) {
      _shutdown_connection(connection_iterator);
      connection_iterator = _agents.erase(connection_iterator);
    }
    return;
  }

  auto conf_iterator = new_conf->get_agent_grpc_reverse_conf().begin();

  while (connection_iterator != _agents.end() &&
         conf_iterator != new_conf->get_agent_grpc_reverse_conf().end()) {
    int compare_res = connection_iterator->first->compare(**conf_iterator);
    if (compare_res > 0) {
      connection_iterator =
          _create_new_client_connection(*conf_iterator, new_conf);
      ++connection_iterator;
      ++conf_iterator;
    } else if (compare_res < 0) {
      _shutdown_connection(connection_iterator);
      connection_iterator = _agents.erase(connection_iterator);
    } else {
      connection_iterator->second->refresh_agent_configuration_if_needed(
          new_conf);
      ++connection_iterator;
      ++conf_iterator;
    }
  }

  while (connection_iterator != _agents.end()) {
    _shutdown_connection(connection_iterator);
    connection_iterator = _agents.erase(connection_iterator);
  }

  for (; conf_iterator != new_conf->get_agent_grpc_reverse_conf().end();
       ++conf_iterator) {
    _create_new_client_connection(*conf_iterator, new_conf);
  }
}

/**
 * @brief create and start a new agent reversed connection
 *
 * @param agent_endpoint endpoint to connect
 * @param new_conf global agent configuration
 * @return agent_reverse_client::config_to_client::iterator iterator to the new
 * element inserted
 */
agent_reverse_client::config_to_client::iterator
agent_reverse_client::_create_new_client_connection(
    const grpc_config::pointer& agent_endpoint,
    const agent_config::pointer& agent_conf) {
  auto insert_res = _agents.try_emplace(
      agent_endpoint,
      to_agent_connector::load(agent_endpoint, _io_context, agent_conf,
                               _metric_handler, _logger, _agent_stats));
  return insert_res.first;
}

/**
 * @brief only shutdown client connection, no container erase
 *
 * @param to_delete
 */
void agent_reverse_client::_shutdown_connection(
    config_to_client::const_iterator to_delete) {
  to_delete->second->shutdown();
}
