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

#include "grpc_stream.grpc.pb.h"
#include "grpc_stream.pb.h"

#include "com/centreon/broker/grpc/client.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::exceptions;

using namespace com::centreon::broker;
using namespace com::centreon::broker::grpc;

client::client(const grpc_config::pointer& conf) : channel("client", conf) {
  log_v2::grpc()->trace("{} this={:p}", __PRETTY_FUNCTION__,
                        static_cast<void*>(this));
  ::grpc::ChannelArguments args;
  args.SetInt(GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS, 1);
  args.SetInt(GRPC_ARG_KEEPALIVE_TIME_MS, 30000);
  args.SetInt(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, 10000);
  args.SetInt(GRPC_ARG_HTTP2_MAX_PINGS_WITHOUT_DATA, 0);
  std::shared_ptr<::grpc::ChannelCredentials> creds;
#ifdef USE_TLS
  if (conf->is_crypted()) {
    log_v2::grpc()->info(
        "{} crypted connexion to {} cert: {}..., key: {}..., ca: {}...",
        __PRETTY_FUNCTION__, conf->get_hostport(),
        conf->get_cert().substr(0, 10), conf->get_key().substr(0, 10),
        conf->get_ca().substr(0, 10));

    ::grpc::experimental::TlsChannelCredentialsOptions opts;
    creds = ::grpc::experimental::TlsCredentials(opts);
#else
  if (conf->is_crypted()) {
    ::grpc::SslCredentialsOptions ssl_opts = {conf->get_ca(), conf->get_key(),
                                              conf->get_cert()};
    log_v2::grpc()->info(
        "{} crypted connexion to {} cert: {}..., key: {}..., ca: {}...",
        __PRETTY_FUNCTION__, conf->get_hostport(),
        conf->get_cert().substr(0, 10), conf->get_key().substr(0, 10),
        conf->get_ca().substr(0, 10));
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
    log_v2::grpc()->info("{} uncrypted connexion to {}", __PRETTY_FUNCTION__,
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
  _stub.reset();
  _context.reset();
  _channel.reset();
}

void client::start_read(event_ptr& to_read, bool first_read) {
  StartRead(to_read.get());
  if (first_read) {
    StartCall();
  }
}

void client::OnReadDone(bool ok) {
  on_read_done(ok);
}

void client::start_write(const event_ptr& to_send) {
  StartWrite(to_send.get());
}

void client::OnWriteDone(bool ok) {
  on_write_done(ok);
}
