/*
** Copyright 2024 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#include <grpcpp/create_channel.h>

#include "com/centreon/common/grpc/grpc_client.hh"

using namespace com::centreon::common::grpc;

/**
 * @brief Construct a new grpc client base::grpc client base object
 *
 * @param conf
 * @param logger
 */
grpc_client_base::grpc_client_base(
    const grpc_config::pointer& conf,
    const std::shared_ptr<spdlog::logger>& logger)
    : _conf(conf), _logger(logger) {
  ::grpc::ChannelArguments args;
  args.SetInt(GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS, 1);
  args.SetInt(GRPC_ARG_KEEPALIVE_TIME_MS,
              conf->get_second_keepalive_interval() * 1000);
  args.SetInt(GRPC_ARG_KEEPALIVE_TIMEOUT_MS,
              conf->get_second_keepalive_interval() * 300);
  args.SetInt(GRPC_ARG_HTTP2_MAX_PINGS_WITHOUT_DATA, 0);
  if (!conf->get_ca_name().empty())
    args.SetString(GRPC_SSL_TARGET_NAME_OVERRIDE_ARG, conf->get_ca_name());
  if (conf->is_compressed()) {
    grpc_compression_algorithm algo = grpc_compression_algorithm_for_level(
        GRPC_COMPRESS_LEVEL_HIGH, calc_accept_all_compression_mask());

    const char* algo_name;
    if (grpc_compression_algorithm_name(algo, &algo_name)) {
      SPDLOG_LOGGER_DEBUG(_logger, "client this={:p} activate compression {}",
                          static_cast<void*>(this), algo_name);
    } else {
      SPDLOG_LOGGER_DEBUG(_logger,
                          "client this={:p} activate compression unknown",
                          static_cast<void*>(this));
    }
    args.SetCompressionAlgorithm(algo);
  }
  std::shared_ptr<::grpc::ChannelCredentials> creds;
  if (conf->is_crypted()) {
    ::grpc::SslCredentialsOptions ssl_opts = {conf->get_ca(), conf->get_key(),
                                              conf->get_cert()};
    SPDLOG_LOGGER_INFO(
        _logger,
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
    SPDLOG_LOGGER_INFO(_logger, "unencrypted connection to {}",
                       conf->get_hostport());
    creds = ::grpc::InsecureChannelCredentials();
  }

  if (conf->get_second_max_reconnect_backoff() > 0) {
    args.SetInt(GRPC_ARG_MAX_RECONNECT_BACKOFF_MS,
                conf->get_second_max_reconnect_backoff() * 1000);
  }

  if (conf->get_max_message_length() > 0) {
    args.SetInt(GRPC_ARG_MAX_RECEIVE_MESSAGE_LENGTH,
                conf->get_max_message_length());
    args.SetInt(GRPC_ARG_MAX_SEND_MESSAGE_LENGTH,
                conf->get_max_message_length());
  }

  _channel = ::grpc::CreateCustomChannel(conf->get_hostport(), creds, args);
}
