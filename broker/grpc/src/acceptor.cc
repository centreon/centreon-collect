/**
 * Copyright 2022 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */

#include "grpc_stream.grpc.pb.h"

#include "com/centreon/broker/grpc/acceptor.hh"
#include "com/centreon/broker/grpc/stream.hh"
#include "com/centreon/common/pool.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::grpc;
using com::centreon::common::log_v2::log_v2;

static constexpr multiplexing::muxer_filter _grpc_stream_filter =
    multiplexing::muxer_filter(multiplexing::muxer_filter::zero_init());

static constexpr multiplexing::muxer_filter _grpc_forbidden_filter =
    multiplexing::muxer_filter(multiplexing::muxer_filter::zero_init())
        .add_category(io::local);

namespace com::centreon::broker::grpc {

using client_stream_base_class = com::centreon::broker::grpc::stream<
    ::grpc::ServerBidiReactor<::com::centreon::broker::stream::CentreonEvent,
                              ::com::centreon::broker::stream::CentreonEvent>>;

/**
 * @brief bireactor server side
 *
 */
class server_stream : public client_stream_base_class {
  std::weak_ptr<service_impl> _parent;

  void shutdown() override;

 public:
  server_stream(const grpc_config::pointer& conf,
                const std::shared_ptr<service_impl>& serv,
                const std::shared_ptr<asio::io_context> io_context,
                const std::shared_ptr<spdlog::logger>& logger);

  void OnDone() override;
};

/**
 * @brief Construct a new server stream::server stream object
 *
 * @param conf
 */
server_stream::server_stream(const grpc_config::pointer& conf,
                             const std::shared_ptr<service_impl>& serv,
                             const std::shared_ptr<asio::io_context> io_context,
                             const std::shared_ptr<spdlog::logger>& logger)
    : client_stream_base_class(conf, "accepted", io_context, logger),
      _parent(serv) {}

/**
 * @brief shutdown bireactor
 *
 */
void server_stream::shutdown() {
  client_stream_base_class::shutdown();
  Finish(::grpc::Status::CANCELLED);
}

/**
 * @brief handler called when this object mustn't be used any more
 *
 */
void server_stream::OnDone() {
  std::shared_ptr<service_impl> parent = _parent.lock();
  if (parent) {
    parent->unregister(shared_from_this());
  }
  client_stream_base_class::OnDone();
}

}  // namespace com::centreon::broker::grpc

/**
 * @brief Construct a new service impl::service impl object
 *
 * @param conf
 */
service_impl::service_impl(const grpc_config::pointer& conf,
                           const std::shared_ptr<asio::io_context> io_context,
                           const std::shared_ptr<spdlog::logger>& logger)
    : _conf(conf), _logger(logger), _io_context(io_context) {}

/**
 * @brief to call after construction
 *
 */
void service_impl::init() {
  ::grpc::Service::MarkMethodCallback(
      0, new ::grpc::internal::CallbackBidiHandler<
             ::com::centreon::broker::stream::CentreonEvent,
             ::com::centreon::broker::stream::CentreonEvent>(
             [me = shared_from_this()](::grpc::CallbackServerContext* context) {
               return me->exchange(context);
             }));
}

/**
 * @brief called on every stream creation
 * every accepted stream is pushed in _wait_to_open queue
 *
 * @param context
 * @return
 * ::grpc::ServerBidiReactor<::com::centreon::broker::stream::CentreonEvent,
 * ::com::centreon::broker::stream::CentreonEvent>*
 */
::grpc::ServerBidiReactor<::com::centreon::broker::stream::CentreonEvent,
                          ::com::centreon::broker::stream::CentreonEvent>*
service_impl::exchange(::grpc::CallbackServerContext* context) {
  if (!_conf->get_authorization().empty()) {
    const auto& metas = context->client_metadata();

    auto header_search = metas.lower_bound(authorization_header);
    if (header_search == metas.end()) {
      SPDLOG_LOGGER_ERROR(_logger, "header {} not found from {}",
                          authorization_header, context->peer());
      return nullptr;
    }
    bool found = false;
    for (; header_search != metas.end() && !found; ++header_search) {
      if (header_search->first != authorization_header) {
        SPDLOG_LOGGER_ERROR(_logger, "Wrong client authorization token from {}",
                            context->peer());
        return nullptr;
      }
      found = _conf->get_authorization() == header_search->second;
    }
  }

  SPDLOG_LOGGER_DEBUG(_logger, "connection accepted from {}", context->peer());

  std::shared_ptr<server_stream> next_stream = std::make_shared<server_stream>(
      _conf, shared_from_this(), _io_context, _logger);

  server_stream::register_stream(next_stream);
  next_stream->start_read();
  {
    std::lock_guard l(_wait_m);
    _wait_to_open.push_back(next_stream);
  }
  _wait_cond.notify_one();
  return next_stream.get();
}

/**
 * @brief pop an accepted stream from _wait_to_open queue
 * if it is empty, it waits 5 seconds
 *
 * @return std::shared_ptr<io::stream>  null if no accepted stream
 */
std::shared_ptr<io::stream> service_impl::get_wait() {
  std::unique_lock l(_wait_m);
  _wait_cond.wait_for(l, std::chrono::seconds(5),
                      [this]() { return !_wait_to_open.empty(); });
  if (_wait_to_open.empty()) {
    return std::shared_ptr<io::stream>();
  }
  std::shared_ptr<io::stream> ret(_wait_to_open.front());
  _wait_to_open.pop_front();
  register_accepted(ret);
  return ret;
}

/**
 * @brief test if _wait_to_open queue is empty
 *
 * @return true not empty
 * @return false empty
 */
bool service_impl::has_wait_stream() const {
  std::lock_guard l(_wait_m);
  return !_wait_to_open.empty();
}

/**
 * @brief at shutdown, we have to shutdown all stream not yet managed by a
 * failover
 *
 */
void service_impl::shutdown_all_wait() {
  std::deque<std::shared_ptr<io::stream>> to_stop;
  {
    std::lock_guard l(_wait_m);
    to_stop = std::move(_wait_to_open);
  }

  for (std::shared_ptr<io::stream> strm : to_stop) {
    strm->stop();
  }
}

/**
 * @brief stop all accepted and managed streams
 *
 */
void service_impl::shutdown_all_accepted() {
  std::set<std::shared_ptr<io::stream>> to_shutdown;
  {
    std::lock_guard l(_accepted_m);
    to_shutdown = std::move(_accepted);
  }
  for (auto to_stop : to_shutdown) {
    to_stop->wait_for_all_events_written(500);
    to_stop->stop();
  }
}

/**
 * @brief store to_register in order to shutdown it on server shutdown
 *
 * @param to_register
 */
void service_impl::register_accepted(
    const std::shared_ptr<io::stream>& to_register) {
  std::lock_guard l(_accepted_m);
  _accepted.insert(to_register);
}

/**
 * @brief called by OnDone
 *
 * @param to_register
 */
void service_impl::unregister(
    const std::shared_ptr<io::stream>& to_unregister) {
  {
    std::lock_guard l(_accepted_m);
    _accepted.erase(to_unregister);
  }
  std::lock_guard l(_wait_m);
  auto to_delete =
      std::find(_wait_to_open.begin(), _wait_to_open.end(), to_unregister);
  if (to_delete != _wait_to_open.end()) {
    _wait_to_open.erase(to_delete);
  }
}

/**
 * @brief Construct a new acceptor::acceptor object
 *
 * @param conf
 */
acceptor::acceptor(const grpc_config::pointer& conf)
    : io::endpoint(true, _grpc_stream_filter, _grpc_forbidden_filter),
      com::centreon::common::grpc::grpc_server_base(
          conf,
          log_v2::instance().get(log_v2::GRPC)) {
  _init([this](::grpc::ServerBuilder& builder) {
    _service = std::make_shared<service_impl>(
        std::static_pointer_cast<grpc_config>(get_conf()), get_io_context(),
        get_logger());
    _service->init();
    builder.RegisterService(_service.get());
  });
}

/**
 * @brief Destroy the acceptor::acceptor object
 *
 */
acceptor::~acceptor() {
  if (initialized()) {
    SPDLOG_LOGGER_DEBUG(get_logger(), "begin shutdown of acceptor {} ",
                        _service->get_conf()->get_hostport());
    _service->shutdown_all_wait();
    _service->shutdown_all_accepted();
    SPDLOG_LOGGER_DEBUG(get_logger(), "end shutdown of acceptor {} ",
                        _service->get_conf()->get_hostport());
  }
}

/**
 * @brief get accepted stream
 *
 * @return std::shared_ptr<io::stream> null if no waiting accepted stream
 */
std::shared_ptr<io::stream> acceptor::open() {
  return _service->get_wait();
}

/**
 * @brief test if we have a waiting accepted stream
 *
 * @return true
 * @return false
 */
bool acceptor::is_ready() const {
  return _service->has_wait_stream();
}
