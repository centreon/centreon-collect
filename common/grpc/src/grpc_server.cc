/**
 * Copyright 2024 Centreon (https://www.centreon.com/)
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

#include "com/centreon/common/grpc/grpc_server.hh"

using namespace com::centreon::common::grpc;

/**
 * @brief Construct a new grpc server base::grpc server base object
 * it creates and starts a grpc server
 *
 * @param conf
 * @param logger
 * @param options this lambda has access to ServerBuilder and can register
 * grpc service(s) generated by protoc like service Export of opentelemetry
 */
grpc_server_base::grpc_server_base(
    const grpc_config::pointer& conf,
    const std::shared_ptr<spdlog::logger>& logger)
    : _conf(conf), _logger(logger) {}

void grpc_server_base::_init(const builder_option& options) {
  ::grpc::ServerBuilder builder;

  std::shared_ptr<::grpc::ServerCredentials> server_creds;
  if (_conf->is_crypted() && !_conf->get_cert().empty() &&
      !_conf->get_key().empty()) {
    ::grpc::SslServerCredentialsOptions::PemKeyCertPair pkcp = {
        _conf->get_key(), _conf->get_cert()};

    SPDLOG_LOGGER_INFO(
        _logger,
        "encrypted server listening on {} cert: {}..., key: {}..., ca: {}....",
        _conf->get_hostport(), _conf->get_cert().substr(0, 10),
        _conf->get_key().substr(0, 10), _conf->get_ca().substr(0, 10));

    ::grpc::SslServerCredentialsOptions ssl_opts;
    ssl_opts.pem_root_certs = _conf->get_ca();
    ssl_opts.pem_key_cert_pairs.push_back(pkcp);

    server_creds = ::grpc::SslServerCredentials(ssl_opts);
  } else {
    SPDLOG_LOGGER_INFO(_logger, "unencrypted server listening on {}",
                       _conf->get_hostport());
    server_creds = ::grpc::InsecureServerCredentials();
  }
  builder.AddListeningPort(_conf->get_hostport(), server_creds);
  options(builder);
  builder.AddChannelArgument(GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS, 1);
  builder.AddChannelArgument(GRPC_ARG_KEEPALIVE_TIME_MS,
                             _conf->get_second_keepalive_interval() * 1000);
  builder.AddChannelArgument(GRPC_ARG_KEEPALIVE_TIMEOUT_MS,
                             _conf->get_second_keepalive_interval() * 300);
  builder.AddChannelArgument(GRPC_ARG_HTTP2_MAX_PING_STRIKES, 0);
  builder.AddChannelArgument(GRPC_ARG_HTTP2_MAX_PINGS_WITHOUT_DATA, 0);
  builder.AddChannelArgument(
      GRPC_ARG_HTTP2_MIN_RECV_PING_INTERVAL_WITHOUT_DATA_MS, 60000);

  if (_conf->is_compressed()) {
    grpc_compression_algorithm algo = grpc_compression_algorithm_for_level(
        GRPC_COMPRESS_LEVEL_HIGH, calc_accept_all_compression_mask());
    const char* algo_name;
    if (grpc_compression_algorithm_name(algo, &algo_name))
      SPDLOG_LOGGER_DEBUG(_logger, "server default compression {}", algo_name);
    else
      SPDLOG_LOGGER_DEBUG(_logger, "server default compression unknown");

    builder.SetDefaultCompressionAlgorithm(algo);
    builder.SetDefaultCompressionLevel(GRPC_COMPRESS_LEVEL_HIGH);
  }

  if (_conf->get_max_message_length() > 0) {
    builder.SetMaxReceiveMessageSize(_conf->get_max_message_length());
    builder.SetMaxSendMessageSize(_conf->get_max_message_length());
  }

  _server = builder.BuildAndStart();
}

/**
 * @brief Destroy the grpc_server::grpc_server object
 *
 */
grpc_server_base::~grpc_server_base() {
  shutdown(std::chrono::seconds(15));
}

/**
 * @brief shutdown server
 *
 * @param timeout after this timeout, grpc server will be stopped
 */
void grpc_server_base::shutdown(
    const std::chrono::system_clock::duration& timeout) {
  std::unique_ptr<::grpc::Server> to_shutdown;
  if (!_server) {
    return;
  }
  to_shutdown = std::move(_server);
  SPDLOG_LOGGER_INFO(_logger, "{:p} begin shutdown of grpc server {}",
                     static_cast<const void*>(this), _conf->get_hostport());
  to_shutdown->Shutdown(std::chrono::system_clock::now() + timeout);
  to_shutdown->Wait();
  SPDLOG_LOGGER_INFO(_logger, "{:p} end shutdown of grpc server {}",
                     static_cast<const void*>(this), _conf->get_hostport());
}
