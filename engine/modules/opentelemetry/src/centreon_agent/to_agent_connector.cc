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

#include "com/centreon/common/defer.hh"

#include "centreon_agent/to_agent_connector.hh"

#include "centreon_agent/agent_impl.hh"

using namespace com::centreon::engine::modules::opentelemetry::centreon_agent;

namespace com::centreon::engine::modules::opentelemetry::centreon_agent {

/**
 * @brief reverse connection to an agent
 *
 */
class agent_connection
    : public agent_impl<::grpc::ClientBidiReactor<agent::MessageToAgent,
                                                  agent::MessageFromAgent>> {
  std::weak_ptr<to_agent_connector> _parent;

  std::string _peer;
  ::grpc::ClientContext _context;

 public:
  agent_connection(const std::shared_ptr<boost::asio::io_context>& io_context,
                   const std::shared_ptr<to_agent_connector>& parent,
                   const agent_config::pointer& conf,
                   const metric_handler& handler,
                   const std::shared_ptr<spdlog::logger>& logger,
                   const agent_stat::pointer& stats);

  ::grpc::ClientContext& get_context() { return _context; }

  void on_error() override;

  void shutdown() override;

  const std::string& get_peer() const override { return _peer; }
};

/**
 * @brief Construct a new agent connection::agent connection object
 *
 * @param io_context
 * @param parent to_agent_connector that had created this object
 * @param handler handler called on every metric received
 * @param logger
 */
agent_connection::agent_connection(
    const std::shared_ptr<boost::asio::io_context>& io_context,
    const std::shared_ptr<to_agent_connector>& parent,
    const agent_config::pointer& conf,
    const metric_handler& handler,
    const std::shared_ptr<spdlog::logger>& logger,
    const agent_stat::pointer& stats)
    : agent_impl<::grpc::ClientBidiReactor<agent::MessageToAgent,
                                           agent::MessageFromAgent>>(
          io_context,
          "reverse_client",
          conf,
          handler,
          logger,
          true,
          stats),
      _parent(parent) {
  _peer = parent->get_conf()->get_hostport();
}

/**
 * @brief called by OnReadDone or OnWriteDone when ok = false
 *
 */
void agent_connection::on_error() {
  std::shared_ptr<to_agent_connector> parent = _parent.lock();
  if (parent) {
    parent->on_error();
  }
}

/**
 * @brief shutdown connection before delete
 *
 */
void agent_connection::shutdown() {
  absl::MutexLock l(&_protect);
  if (_alive) {
    _alive = false;
    agent_impl<::grpc::ClientBidiReactor<agent::MessageToAgent,
                                         agent::MessageFromAgent>>::shutdown();
    RemoveHold();
    _context.TryCancel();
  }
}

};  // namespace com::centreon::engine::modules::opentelemetry::centreon_agent
/**
 * @brief Construct a new agent client::agent client object
 * use to_agent_connector instead
 * @param conf
 * @param io_context
 * @param handler handler that will process received metrics
 * @param logger
 */
to_agent_connector::to_agent_connector(
    const grpc_config::pointer& agent_endpoint_conf,
    const std::shared_ptr<boost::asio::io_context>& io_context,
    const agent_config::pointer& agent_conf,
    const metric_handler& handler,
    const std::shared_ptr<spdlog::logger>& logger,
    const agent_stat::pointer& stats)
    : common::grpc::grpc_client_base(agent_endpoint_conf, logger),
      _io_context(io_context),
      _metric_handler(handler),
      _conf(agent_conf),
      _alive(true),
      _stats(stats) {
  _stub = agent::ReversedAgentService::NewStub(_channel);
}

/**
 * @brief Destroy the to agent connector::to agent connector object
 * shutdown connection
 */
to_agent_connector::~to_agent_connector() {
  shutdown();
}

/**
 * @brief construct an start a new client
 *
 * @param conf conf of the agent endpoint
 * @param io_context
 * @param handler handler that will process received metrics
 * @param logger
 * @return std::shared_ptr<to_agent_connector> client created and started
 */
std::shared_ptr<to_agent_connector> to_agent_connector::load(
    const grpc_config::pointer& agent_endpoint_conf,
    const std::shared_ptr<boost::asio::io_context>& io_context,
    const agent_config::pointer& agent_conf,
    const metric_handler& handler,
    const std::shared_ptr<spdlog::logger>& logger,
    const agent_stat::pointer& stats) {
  std::shared_ptr<to_agent_connector> ret =
      std::make_shared<to_agent_connector>(agent_endpoint_conf, io_context,
                                           agent_conf, handler, logger, stats);
  ret->start();
  return ret;
}

/**
 * @brief connect to agent and initialize exchange
 *
 */
void to_agent_connector::start() {
  absl::MutexLock l(&_connection_m);
  if (!_alive) {
    return;
  }
  SPDLOG_LOGGER_INFO(get_logger(), "connect to {}", get_conf()->get_hostport());
  if (_connection) {
    _connection->shutdown();
    _connection.reset();
  }
  _connection =
      std::make_shared<agent_connection>(_io_context, shared_from_this(), _conf,
                                         _metric_handler, get_logger(), _stats);
  agent_connection::register_stream(_connection);
  _stub->async()->Import(&_connection->get_context(), _connection.get());
  _connection->start_read();
  _connection->AddHold();
  _connection->StartCall();
}

/**
 * @brief send conf to agent if something has changed (list of services,
 * commands...)
 *
 */
void to_agent_connector::refresh_agent_configuration_if_needed(
    const agent_config::pointer& new_conf) {
  absl::MutexLock l(&_connection_m);
  if (_connection) {
    _connection->calc_and_send_config_if_needed(new_conf);
  }
}

/**
 * @brief shutdown configuration, once this method has been called, this object
 * is dead and must be deleted
 *
 */
void to_agent_connector::shutdown() {
  absl::MutexLock l(&_connection_m);
  if (_alive) {
    SPDLOG_LOGGER_INFO(get_logger(), "shutdown client of {}",
                       get_conf()->get_hostport());
    if (_connection) {
      _connection->shutdown();
      _connection.reset();
    }
    _alive = false;
  }
}

/**
 * @brief called by connection
 * reconnection is delayed of 10 second
 *
 */
void to_agent_connector::on_error() {
  common::defer(_io_context, std::chrono::seconds(10),
                [me = shared_from_this()] { me->start(); });
}
