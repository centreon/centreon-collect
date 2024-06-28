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

#include "centreon_agent/agent_service.hh"

using namespace com::centreon::engine::modules::opentelemetry::centreon_agent;

namespace com::centreon::engine::modules::opentelemetry::centreon_agent {

class server_bireactor
    : public agent_impl<::grpc::ServerBidiReactor<agent::MessageFromAgent,
                                                  agent::MessageToAgent>> {
  const std::string _peer;

 public:
  template <class otel_request_handler>
  server_bireactor(const std::shared_ptr<boost::asio::io_context>& io_context,
                   const agent_config::pointer& conf,
                   const otel_request_handler& handler,
                   const std::shared_ptr<spdlog::logger>& logger,
                   const std::string& peer)
      : agent_impl<::grpc::ServerBidiReactor<agent::MessageFromAgent,
                                             agent::MessageToAgent>>(
            io_context,
            "agent_server",
            conf,
            handler,
            logger),
        _peer(peer) {
    SPDLOG_LOGGER_DEBUG(_logger, "connected with agent {}", _peer);
  }

  const std::string& get_peer() const override { return _peer; }

  void on_error() override;
  void shutdown() override;
};

void server_bireactor::on_error() {
  shutdown();
}

void server_bireactor::shutdown() {
  absl::MutexLock l(&_protect);
  if (_alive) {
    _alive = false;
    agent_impl<::grpc::ServerBidiReactor<agent::MessageFromAgent,
                                         agent::MessageToAgent>>::shutdown();
    Finish(::grpc::Status::CANCELLED);
    SPDLOG_LOGGER_DEBUG(_logger, "end of agent connection with {}", _peer);
  }
}

}  // namespace com::centreon::engine::modules::opentelemetry::centreon_agent

/**
 * @brief Construct a new agent service::agent service object
 * don't use it, use agent_service::load instead
 *
 * @param io_context
 * @param handler
 * @param logger
 */
agent_service::agent_service(
    const std::shared_ptr<boost::asio::io_context>& io_context,
    const agent_config::pointer& conf,
    const metric_handler& handler,
    const std::shared_ptr<spdlog::logger>& logger)
    : _io_context(io_context),
      _conf(conf),
      _metric_handler(handler),
      _logger(logger) {
  if (!_conf) {
    _conf = std::make_shared<agent_config>(60, 100, 10, 30);
    SPDLOG_LOGGER_INFO(logger,
                       "no centreon_agent configuration given => we use a "
                       "default configuration ");
  }
}

/**
 * @brief prefered way to construct an agent_service
 *
 * @param io_context
 * @param handler
 * @param logger
 * @return std::shared_ptr<agent_service>
 */
std::shared_ptr<agent_service> agent_service::load(
    const std::shared_ptr<boost::asio::io_context>& io_context,
    const agent_config::pointer& conf,
    const metric_handler& handler,
    const std::shared_ptr<spdlog::logger>& logger) {
  std::shared_ptr<agent_service> ret = std::make_shared<agent_service>(
      io_context, conf, std::move(handler), logger);
  ret->init();
  return ret;
}

/**
 * @brief to call after construction
 *
 */
void agent_service::init() {
  ::grpc::Service::MarkMethodCallback(
      0, new ::grpc::internal::CallbackBidiHandler<
             com::centreon::agent::MessageFromAgent,
             com::centreon::agent::MessageToAgent>(
             [me = shared_from_this()](::grpc::CallbackServerContext* context) {
               return me->Export(context);
             }));
}

/**
 * @brief called by grpc layer on each incoming connection
 *
 * @param context
 * @return ::grpc::ServerBidiReactor<com::centreon::agent::MessageFromAgent,
 * com::centreon::agent::MessageToAgent>*
 */
::grpc::ServerBidiReactor<com::centreon::agent::MessageFromAgent,
                          com::centreon::agent::MessageToAgent>*
agent_service::Export(::grpc::CallbackServerContext* context) {
  std::shared_ptr<server_bireactor> new_reactor;
  {
    absl::MutexLock l(&_conf_m);
    new_reactor = std::make_shared<server_bireactor>(
        _io_context, _conf, _metric_handler, _logger, context->peer());
  }
  server_bireactor::register_stream(new_reactor);
  new_reactor->start_read();

  return new_reactor.get();
}

void agent_service::shutdown_all_accepted() {
  server_bireactor::shutdown_all();
}

void agent_service::update(const agent_config::pointer& conf) {
  absl::MutexLock l(&_conf_m);
  _conf = conf;
}
