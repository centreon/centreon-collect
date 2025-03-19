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

#include "streaming_client.hh"
#include "agent_info.hh"
#include "com/centreon/common/defer.hh"

using namespace com::centreon::agent;

/**
 * @brief Construct a new client reactor::client reactor object
 *
 * @param io_context
 * @param parent we will keep a weak_ptr on streaming_client object
 */
client_reactor::client_reactor(
    const std::shared_ptr<boost::asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,

    const std::shared_ptr<streaming_client>& parent,
    const std::string& peer)
    : bireactor<::grpc::ClientBidiReactor<MessageFromAgent, MessageToAgent>>(
          io_context,
          logger,
          "client",
          peer),
      _parent(parent) {}

/**
 * @brief pass request to streaming_client parent
 *
 * @param request
 */
void client_reactor::on_incomming_request(
    const std::shared_ptr<MessageToAgent>& request) {
  std::shared_ptr<streaming_client> parent = _parent.lock();
  if (!parent) {
    shutdown();
  } else {
    parent->on_incomming_request(shared_from_this(), request);
  }
}

/**
 * @brief called whe OnReadDone or OnWriteDone ok parameter is false
 *
 */
void client_reactor::on_error() {
  std::shared_ptr<streaming_client> parent = _parent.lock();
  if (parent) {
    parent->on_error(shared_from_this());
  }
}

/**
 * @brief shutdown connection to engine if not yet done
 *
 */
void client_reactor::shutdown() {
  std::lock_guard l(_protect);
  if (_alive) {
    _alive = false;
    bireactor<::grpc::ClientBidiReactor<MessageFromAgent,
                                        MessageToAgent>>::shutdown();
    RemoveHold();
    _context.TryCancel();
  }
}

/**
 * @brief Construct a new streaming client::streaming client object
 * not use it, use load instead
 *
 * @param io_context
 * @param conf
 * @param supervised_hosts
 */
streaming_client::streaming_client(
    const std::shared_ptr<boost::asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    const std::shared_ptr<common::grpc::grpc_config>& conf,
    const std::string& supervised_host)
    : com::centreon::common::grpc::grpc_client_base(conf, logger),
      _io_context(io_context),
      _logger(logger),
      _supervised_host(supervised_host) {
  _stub = std::move(AgentService::NewStub(_channel));
}

/**
 * @brief to call after construction
 *
 */
void streaming_client::_start() {
  std::weak_ptr<streaming_client> weak_this = shared_from_this();

  _sched = scheduler::load(
      _io_context, _logger, _supervised_host, scheduler::default_config(),
      [sender = std::move(weak_this)](
          const std::shared_ptr<MessageFromAgent>& request) {
        auto parent = sender.lock();
        if (parent) {
          parent->_send(request);
        }
      },
      scheduler::default_check_builder);
  _create_reactor();
}

/**
 * @brief create reactor on current grpc channel
 * and send agent infos (hostname, supervised hosts, collect version)
 *
 */
void streaming_client::_create_reactor() {
  std::lock_guard l(_protect);
  if (_reactor) {
    _reactor->shutdown();
  }
  _reactor = std::make_shared<client_reactor>(
      _io_context, _logger, shared_from_this(), get_conf()->get_hostport());
  client_reactor::register_stream(_reactor);
  _stub->async()->Export(&_reactor->get_context(), _reactor.get());
  _reactor->start_read();
  _reactor->AddHold();
  _reactor->StartCall();

  // identifies to engine
  std::shared_ptr<MessageFromAgent> who_i_am =
      std::make_shared<MessageFromAgent>();
  fill_agent_info(_supervised_host, who_i_am->mutable_init());

  _reactor->write(who_i_am);
}

/**
 * @brief construct a new streaming_client
 *
 * @param io_context
 * @param conf
 * @param supervised_hosts list of host to supervise (match to engine config)
 * @return std::shared_ptr<streaming_client>
 */
std::shared_ptr<streaming_client> streaming_client::load(
    const std::shared_ptr<boost::asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    const std::shared_ptr<common::grpc::grpc_config>& conf,
    const std::string& supervised_host) {
  std::shared_ptr<streaming_client> ret = std::make_shared<streaming_client>(
      io_context, logger, conf, supervised_host);
  ret->_start();
  return ret;
}

/**
 * @brief send a request to engine
 *
 * @param request
 */
void streaming_client::_send(const std::shared_ptr<MessageFromAgent>& request) {
  std::lock_guard l(_protect);
  if (_reactor)
    _reactor->write(request);
}

/**
 * @brief
 *
 * @param caller
 * @param request
 */
void streaming_client::on_incomming_request(
    const std::shared_ptr<client_reactor>& caller [[maybe_unused]],
    const std::shared_ptr<MessageToAgent>& request) {
  // incoming request is used in main thread
  _io_context->post([request, sched = _sched]() { sched->update(request); });
}

/**
 * @brief called by _reactor when something was wrong
 * Then we wait 10s to reconnect to engine
 *
 * @param caller
 */
void streaming_client::on_error(const std::shared_ptr<client_reactor>& caller) {
  std::lock_guard l(_protect);
  if (caller == _reactor) {
    _reactor.reset();
    common::defer(_io_context, std::chrono::seconds(10),
                  [me = shared_from_this()] { me->_create_reactor(); });
  }
}

/**
 * @brief stop and shutdown scheduler and connection
 * After, this object is dead and must be deleted
 *
 */
void streaming_client::shutdown() {
  std::lock_guard l(_protect);
  _sched->stop();
  if (_reactor) {
    _reactor->shutdown();
  }
}
