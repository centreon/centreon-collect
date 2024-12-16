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

#include "streaming_server.hh"
#include "check_exec.hh"
#include "scheduler.hh"
#include "version.hh"

using namespace com::centreon::agent;

namespace com::centreon::agent {

class server_reactor
    : public bireactor<
          ::grpc::ServerBidiReactor<MessageToAgent, MessageFromAgent>> {
  std::shared_ptr<scheduler> _sched;
  std::string _supervised_host;

  void _start();

 public:
  server_reactor(const std::shared_ptr<boost::asio::io_context>& io_context,
                 const std::shared_ptr<spdlog::logger>& logger,
                 const std::string& supervised_hosts,
                 const std::string& peer);

  static std::shared_ptr<server_reactor> load(
      const std::shared_ptr<boost::asio::io_context>& io_context,
      const std::shared_ptr<spdlog::logger>& logger,
      const std::string& supervised_hosts,
      const std::string& peer);

  std::shared_ptr<server_reactor> shared_from_this() {
    return std::static_pointer_cast<server_reactor>(
        bireactor<::grpc::ServerBidiReactor<MessageToAgent, MessageFromAgent>>::
            shared_from_this());
  }

  void on_incomming_request(
      const std::shared_ptr<MessageToAgent>& request) override;

  void on_error() override;

  void shutdown() override;
};

server_reactor::server_reactor(
    const std::shared_ptr<boost::asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    const std::string& supervised_host,
    const std::string& peer)
    : bireactor<::grpc::ServerBidiReactor<MessageToAgent, MessageFromAgent>>(
          io_context,
          logger,
          "server",
          peer),
      _supervised_host(supervised_host) {}

void server_reactor::_start() {
  std::weak_ptr<server_reactor> weak_this(shared_from_this());

  _sched = scheduler::load(
      _io_context, _logger, _supervised_host, scheduler::default_config(),
      [sender = std::move(weak_this)](
          const std::shared_ptr<MessageFromAgent>& request) {
        auto parent = sender.lock();
        if (parent) {
          parent->write(request);
        }
      },
      scheduler::default_check_builder);

  // identifies to engine
  std::shared_ptr<MessageFromAgent> who_i_am =
      std::make_shared<MessageFromAgent>();
  auto infos = who_i_am->mutable_init();

  infos->mutable_centreon_version()->set_major(CENTREON_AGENT_VERSION_MAJOR);
  infos->mutable_centreon_version()->set_minor(CENTREON_AGENT_VERSION_MINOR);
  infos->mutable_centreon_version()->set_patch(CENTREON_AGENT_VERSION_PATCH);
  infos->set_host(_supervised_host);

  write(who_i_am);
}

std::shared_ptr<server_reactor> server_reactor::load(
    const std::shared_ptr<boost::asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    const std::string& supervised_host,
    const std::string& peer) {
  std::shared_ptr<server_reactor> ret = std::make_shared<server_reactor>(
      io_context, logger, supervised_host, peer);
  ret->_start();
  return ret;
}

void server_reactor::on_incomming_request(
    const std::shared_ptr<MessageToAgent>& request) {
  _io_context->post([sched = _sched, request]() { sched->update(request); });
}

void server_reactor::on_error() {
  shutdown();
}

void server_reactor::shutdown() {
  std::lock_guard l(_protect);
  if (_alive) {
    _alive = false;
    _sched->stop();
    bireactor<::grpc::ServerBidiReactor<MessageToAgent,
                                        MessageFromAgent>>::shutdown();
    Finish(::grpc::Status::CANCELLED);
  }
}

}  // namespace com::centreon::agent

/**
 * @brief Construct a new streaming server::streaming server object
 * Not use it, use load instead
 * @param io_context
 * @param conf
 * @param supervised_hosts list of supervised hosts that will be sent to engine
 * in order to have checks configuration
 */
streaming_server::streaming_server(
    const std::shared_ptr<boost::asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    const std::shared_ptr<common::grpc::grpc_config>& conf,
    const std::string& supervised_host)
    : com::centreon::common::grpc::grpc_server_base(conf, logger),
      _io_context(io_context),
      _logger(logger),
      _supervised_host(supervised_host) {
  SPDLOG_LOGGER_INFO(_logger, "create grpc server listening on {}",
                     conf->get_hostport());
}

streaming_server::~streaming_server() {
  SPDLOG_LOGGER_INFO(_logger, "delete grpc server listening on {}",
                     get_conf()->get_hostport());
}

/**
 * @brief register service and start grpc server
 *
 */
void streaming_server::_start() {
  ::grpc::Service::MarkMethodCallback(
      0, new ::grpc::internal::CallbackBidiHandler<
             ::com::centreon::agent::MessageToAgent,
             ::com::centreon::agent::MessageFromAgent>(
             [me = shared_from_this()](::grpc::CallbackServerContext* context) {
               return me->Import(context);
             }));

  _init([this](::grpc::ServerBuilder& builder) {
    builder.RegisterService(this);
  });
}

/**
 * @brief construct and start a new streaming_server
 *
 * @param io_context
 * @param conf
 * @param supervised_hosts list of supervised hosts that will be sent to engine
 * in order to have checks configuration
 * @return std::shared_ptr<streaming_server>
 */
std::shared_ptr<streaming_server> streaming_server::load(
    const std::shared_ptr<boost::asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    const std::shared_ptr<common::grpc::grpc_config>& conf,
    const std::string& supervised_host) {
  std::shared_ptr<streaming_server> ret = std::make_shared<streaming_server>(
      io_context, logger, conf, supervised_host);
  ret->_start();
  return ret;
}

/**
 * @brief shutdown server and incoming connection
 *
 */
void streaming_server::shutdown() {
  SPDLOG_LOGGER_INFO(_logger, "shutdown grpc server listening on {}",
                     get_conf()->get_hostport());
  {
    std::lock_guard l(_protect);
    if (_incoming) {
      _incoming->shutdown();
      _incoming.reset();
    }
  }
  common::grpc::grpc_server_base::shutdown(std::chrono::seconds(10));
}

/**
 * @brief callback called on incoming connection
 *
 * @param context
 * @return ::grpc::ServerBidiReactor<MessageToAgent, MessageFromAgent>* =
 * _incoming
 */
::grpc::ServerBidiReactor<MessageToAgent, MessageFromAgent>*
streaming_server::Import(::grpc::CallbackServerContext* context) {
  SPDLOG_LOGGER_INFO(_logger, "incoming connection from {}", context->peer());
  std::lock_guard l(_protect);
  if (_incoming) {
    _incoming->shutdown();
  }
  _incoming = server_reactor::load(_io_context, _logger, _supervised_host,
                                   context->peer());
  server_reactor::register_stream(_incoming);
  _incoming->start_read();
  return _incoming.get();
}
