/*
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

#include "com/centreon/broker/grpc/client.hh"

#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"
#include "grpc_stream.grpc.pb.h"
#include "grpc_stream.pb.h"

using namespace com::centreon::exceptions;

using namespace com::centreon::broker;
using namespace com::centreon::broker::grpc;
using com::centreon::common::log_v2::log_v2;

client::client(const grpc_config::pointer& conf)
    : channel("client", conf), _hold_to_remove(false) {
  auto logger = log_v2::instance().get(log_v2::GRPC);
  SPDLOG_LOGGER_TRACE(logger, "this={:p}", static_cast<void*>(this));
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
#ifdef USE_TLS
  if (conf->is_crypted()) {
    SPDLOG_LOGGER_INFO(
        logger, "encrypted connection to {} cert: {}..., key: {}..., ca: {}...",
        conf->get_hostport(), conf->get_cert().substr(0, 10),
        conf->get_key().substr(0, 10), conf->get_ca().substr(0, 10));

    ::grpc::experimental::TlsChannelCredentialsOptions opts;
    creds = ::grpc::experimental::TlsCredentials(opts);
#else
  if (conf->is_crypted()) {
    ::grpc::SslCredentialsOptions ssl_opts = {conf->get_ca(), conf->get_key(),
                                              conf->get_cert()};
    SPDLOG_LOGGER_INFO(
        logger, "encrypted connection to {} cert: {}..., key: {}..., ca: {}...",
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
#endif
  } else {
    SPDLOG_LOGGER_INFO(logger, "unencrypted connection to {}",
                       conf->get_hostport());
    creds = ::grpc::InsecureChannelCredentials();
  }

  _channel = ::grpc::CreateCustomChannel(conf->get_hostport(), creds, args);
  _stub = std::unique_ptr<com::centreon::broker::stream::centreon_bbdo::Stub>(
      com::centreon::broker::stream::centreon_bbdo::NewStub(_channel));
  _context = std::make_unique<::grpc::ClientContext>();
  if (!_conf->get_authorization().empty()) {
    _context->AddMetadata(authorization_header, _conf->get_authorization());
  }
  _stub->async()->exchange(_context.get(), this);
}

client::pointer client::create(const grpc_config::pointer& conf) {
  client::pointer newClient(new client(conf));
  newClient->start();
  return newClient;
}

client::~client() {
  shutdown();
  _stub.reset();
  _channel.reset();
  _context.reset();
}

void client::shutdown() {
  if (_context) {
    _context->TryCancel();
  }
}

void client::start_read(event_ptr& to_read, bool first_read) {
  if (!is_alive()) {
    return;
  }
  StartRead(to_read.get());
  if (first_read) {
    AddHold();
    _hold_to_remove = true;
    StartCall();
  }
}

void client::OnReadDone(bool ok) {
  on_read_done(ok);
}

void client::start_write(const event_ptr& to_send) {
  std::lock_guard<std::recursive_mutex> l(_hold_mutex);
  if (_hold_to_remove) {
    StartWrite(to_send.get());
  }
}

void client::OnWriteDone(bool ok) {
  on_write_done(ok);
  if (!ok) {
    auto logger = log_v2::instance().get(log_v2::GRPC);
    SPDLOG_LOGGER_ERROR(logger,
                        "Write failed, server logs should help to understand "
                        "what's gone wrong");
    remove_hold();
  }
}

/**
 * @brief Remove the hold obtain in the start_read method
 * hold have to be removed after the last OnWriteDone
 *
 */
void client::remove_hold() {
  std::lock_guard<std::recursive_mutex> l(_hold_mutex);
  if (_hold_to_remove) {
    _hold_to_remove = false;
    RemoveHold();
  }
}
