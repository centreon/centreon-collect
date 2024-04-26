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

#include "com/centreon/broker/grpc/connector.hh"
#include "com/centreon/broker/grpc/stream.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::grpc;
using log_v2 = com::centreon::common::log_v2::log_v2;

static constexpr multiplexing::muxer_filter _grpc_stream_filter =
    multiplexing::muxer_filter().remove_category(io::local);

/**
 * @brief Constructor of the connector that will connect to the given host at
 * the given port. read_timeout is a duration in seconds or -1 if no limit.
 *
 * @param host The host to connect to.
 * @param port The port used for the connection.
 */
connector::connector(const grpc_config::pointer& conf)
    : io::limit_endpoint(false, {}), _conf(conf) {
  auto logger = log_v2::instance().get(log_v2::GRPC);
  ::grpc::ChannelArguments args;
  args.SetInt(GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS, 1);
  args.SetInt(GRPC_ARG_KEEPALIVE_TIME_MS,
              conf->get_second_keepalive_interval() * 1000);
  args.SetInt(GRPC_ARG_KEEPALIVE_TIMEOUT_MS,
              conf->get_second_keepalive_interval() * 300);
  args.SetInt(GRPC_ARG_HTTP2_MAX_PINGS_WITHOUT_DATA, 0);
  if (!conf->get_ca_name().empty())
    args.SetString(GRPC_SSL_TARGET_NAME_OVERRIDE_ARG, conf->get_ca_name());
  if (conf->get_compression() == grpc_config::YES) {
    grpc_compression_algorithm algo = grpc_compression_algorithm_for_level(
        GRPC_COMPRESS_LEVEL_HIGH, calc_accept_all_compression_mask());

    const char* algo_name;
    if (grpc_compression_algorithm_name(algo, &algo_name)) {
      logger->debug("client this={:p} activate compression {}",
                            static_cast<void*>(this), algo_name);
    } else {
      logger->debug("client this={:p} activate compression unknown",
                            static_cast<void*>(this));
    }
    args.SetCompressionAlgorithm(algo);
  }
  std::shared_ptr<::grpc::ChannelCredentials> creds;
  if (conf->is_crypted()) {
    ::grpc::SslCredentialsOptions ssl_opts = {conf->get_ca(), conf->get_key(),
                                              conf->get_cert()};
    SPDLOG_LOGGER_INFO(
        logger,
        "encrypted connection to {} cert: {}..., key: {}..., ca: {}...",
        conf->get_hostport(), conf->get_cert().substr(0, 10),
        conf->get_key().substr(0, 10), conf->get_ca().substr(0, 10));
    creds = ::grpc::SslCredentials(ssl_opts);
#ifdef CAN_USE_JWT
    if (!_conf->get_jwt().empty()) {
      std::shared_ptr<::grpc::CallCredentials> jwt =
          ::grpc::ServiceAccountJWTAccessCredentials(_conf->get_jwt(), 86400);
      creds = ::grpc::CompositeChannelCredentials(creds, jwt);
    }
#endif
  } else {
    SPDLOG_LOGGER_INFO(logger, "unencrypted connection to {}",
                       conf->get_hostport());
    creds = ::grpc::InsecureChannelCredentials();
  }

  _channel = ::grpc::CreateCustomChannel(conf->get_hostport(), creds, args);
  _stub = std::move(
      com::centreon::broker::stream::centreon_bbdo::NewStub(_channel));
}

/**
 * @brief open a new connection
 *
 * @return std::unique_ptr<io::stream>
 */
std::shared_ptr<io::stream> connector::open() {
  auto logger = log_v2::instance().get(log_v2::GRPC);
  SPDLOG_LOGGER_INFO(logger, "Connecting to {}", _conf->get_hostport());
  try {
    return limit_endpoint::open();
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_DEBUG(
        logger, "Unable to establish the connection to {} (attempt {}): {}",
        _conf->get_hostport(), _is_ready_count, e.what());
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
  client_stream(const grpc_config::pointer& conf);
  ::grpc::ClientContext& get_context() { return _context; }
};

/**
 * @brief Construct a new client stream::client stream object
 *
 * @param conf
 */
client_stream::client_stream(const grpc_config::pointer& conf)
    : stream_base_class(conf, "client") {
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
  std::shared_ptr<client_stream> new_stream =
      std::make_shared<client_stream>(_conf);
  client_stream::register_stream(new_stream);
  _stub->async()->exchange(&new_stream->get_context(), new_stream.get());
  new_stream->start_read();
  new_stream->AddHold();
  new_stream->StartCall();
  return new_stream;
}
