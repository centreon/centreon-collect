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

#include "com/centreon/broker/multiplexing/muxer_filter.hh"
#include "grpc_stream.grpc.pb.h"

#include "com/centreon/broker/grpc/connector.hh"
#include "com/centreon/broker/grpc/stream.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::grpc;
using log_v2 = com::centreon::common::log_v2::log_v2;

static constexpr multiplexing::muxer_filter _grpc_stream_filter =
    multiplexing::muxer_filter(multiplexing::muxer_filter::zero_init());

static constexpr multiplexing::muxer_filter _grpc_forbidden_filter =
    multiplexing::muxer_filter(multiplexing::muxer_filter::zero_init())
        .add_category(io::local);

/**
 * @brief Constructor of the connector that will connect to the given host at
 * the given port. read_timeout is a duration in seconds or -1 if no limit.
 *
 * @param host The host to connect to.
 * @param port The port used for the connection.
 */
connector::connector(const grpc_config::pointer& conf)
    : io::limit_endpoint(false, _grpc_stream_filter, _grpc_forbidden_filter),
      com::centreon::common::grpc::grpc_client_base(
          conf,
          log_v2::instance().get(log_v2::GRPC)) {
  _stub = std::move(
      com::centreon::broker::stream::centreon_bbdo::NewStub(_channel));
}

/**
 * @brief open a new connection
 *
 * @return std::unique_ptr<io::stream>
 */
std::shared_ptr<io::stream> connector::open() {
  SPDLOG_LOGGER_INFO(get_logger(), "Connecting to {}",
                     get_conf()->get_hostport());
  try {
    return limit_endpoint::open();
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_DEBUG(
        get_logger(),
        "Unable to establish the connection to {} (attempt {}): {}",
        get_conf()->get_hostport(), _is_ready_count, e.what());
    return nullptr;
  }
}

namespace com::centreon::broker::grpc {

using stream_base_class = com::centreon::broker::grpc::stream<
    ::grpc::ClientBidiReactor<::com::centreon::broker::stream::CentreonEvent,
                              ::com::centreon::broker::stream::CentreonEvent>>;

/**
 * @brief bireactor client stream
 * this class is passed to exchange
 *
 */
class client_stream : public stream_base_class {
  ::grpc::ClientContext _context;

  void shutdown() override;

 public:
  client_stream(const grpc_config::pointer& conf,
                const std::shared_ptr<asio::io_context> io_context,
                const std::shared_ptr<spdlog::logger>& logger);
  ::grpc::ClientContext& get_context() { return _context; }
};

/**
 * @brief Construct a new client stream::client stream object
 *
 * @param conf
 */
client_stream::client_stream(const grpc_config::pointer& conf,
                             const std::shared_ptr<asio::io_context> io_context,
                             const std::shared_ptr<spdlog::logger>& logger)
    : stream_base_class(conf, "client", io_context, logger) {
  if (!conf->get_authorization().empty()) {
    _context.AddMetadata(authorization_header, conf->get_authorization());
  }
}

/**
 * @brief as we call StartWrite outside grpc handlers, we add a hold and then we
 * have to remove it
 *
 */
void client_stream::shutdown() {
  stream_base_class::shutdown();
  RemoveHold();
  _context.TryCancel();
}

}  // namespace com::centreon::broker::grpc

/**
 * @brief create a stream from attributes
 * we add hold as StartWrite may be called by write method
 * @return std::unique_ptr<io::stream>
 */
std::shared_ptr<io::stream> connector::create_stream() {
  std::shared_ptr<client_stream> new_stream = std::make_shared<client_stream>(
      std::static_pointer_cast<grpc_config>(get_conf()), get_io_context(),
      get_logger());
  client_stream::register_stream(new_stream);
  _stub->async()->exchange(&new_stream->get_context(), new_stream.get());
  new_stream->start_read();
  new_stream->AddHold();
  new_stream->StartCall();
  return new_stream;
}
