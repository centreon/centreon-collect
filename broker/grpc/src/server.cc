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

#include "com/centreon/broker/grpc/server.hh"
#include "com/centreon/broker/grpc/stream.hh"
#include "com/centreon/broker/misc/trash.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker::grpc;
using namespace com::centreon::broker;
using namespace com::centreon::exceptions;
using log_v2 = com::centreon::common::log_v2::log_v2;

/****************************************************************************
 * accepted_service
 ****************************************************************************/
accepted_service::accepted_service(const grpc_config::pointer& conf,
                                   const shared_bool& server_finished)
    : channel("accepted_service", conf),
      _server_finished(server_finished),
      _finished_called(false),
      _logger{log_v2::instance().get(log_v2::GRPC)} {
  SPDLOG_LOGGER_TRACE(_logger, "accepted_service construction this={:p}",
                      static_cast<void*>(this));
}

void accepted_service::start() {
  channel::start_read(true);
}

accepted_service::~accepted_service() {
  SPDLOG_LOGGER_TRACE(_logger, "accepted_service desctruction this={:p}",
                      static_cast<void*>(this));
}

void accepted_service::desactivate() {
  _error = true;
}

void accepted_service::OnCancel() {
  SPDLOG_LOGGER_TRACE(_logger, "this={:p}", static_cast<void*>(this));
  desactivate();
}

void accepted_service::start_read(event_ptr& to_read, bool) {
  if (*_server_finished || _thrown) {
    bool expected = false;
    if (_finished_called.compare_exchange_strong(expected, true)) {
      SPDLOG_LOGGER_TRACE(_logger, "this={:p}  Finish",
                          static_cast<void*>(this));
      Finish(::grpc::Status(::grpc::CANCELLED, "start_read server finished"));
    }
  } else {
    StartRead(to_read.get());
  }
}

void accepted_service::OnReadDone(bool ok) {
  on_read_done(ok);
}

void accepted_service::start_write(const event_with_data::pointer& to_send) {
  if (*_server_finished || _thrown) {
    bool expected = false;
    if (_finished_called.compare_exchange_strong(expected, true)) {
      SPDLOG_LOGGER_TRACE(_logger, "this={:p} Finish",
                          static_cast<void*>(this));
      Finish(::grpc::Status(::grpc::CANCELLED, "start_write server finished"));
    }
  } else {
    StartWrite(&to_send->grpc_event);
  }
}

void accepted_service::OnWriteDone(bool ok) {
  on_write_done(ok);
}

int accepted_service::stop() {
  SPDLOG_LOGGER_TRACE(_logger, "this={:p}", static_cast<void*>(this));
  shutdown();
  return channel::stop();
}

void accepted_service::shutdown() {
  bool expected = false;
  if (_finished_called.compare_exchange_strong(expected, true)) {
    SPDLOG_LOGGER_DEBUG(_logger, "{} this={:p}", static_cast<void*>(this));
    Finish(::grpc::Status(::grpc::CANCELLED, "stop server finished"));
  }
}

/****************************************************************************
 *                              server
 ****************************************************************************/
server::server(const grpc_config::pointer& conf)
    : _conf(conf), _logger{log_v2::instance().get(log_v2::GRPC)} {
  _server_finished = std::make_shared<bool>(false);
}

void server::start() {
  ::grpc::Service::MarkMethodCallback(
      0, new ::grpc::internal::CallbackBidiHandler<
             ::com::centreon::broker::stream::CentreonEvent,
             ::com::centreon::broker::stream::CentreonEvent>(
             [me = shared_from_this()](::grpc::CallbackServerContext* context) {
               return me->exchange(context);
             }));

  ::grpc::ServerBuilder builder;

  std::shared_ptr<::grpc::ServerCredentials> server_creds;
#ifdef USE_TLS
  if (false && !_conf->get_cert().empty() && !_conf->get_key().empty()) {
    std::vector<::grpc::experimental::IdentityKeyCertPair> key_cert = {
        {_conf->get_key(), _conf->get_cert()}};
    std::shared_ptr<::grpc::experimental::StaticDataCertificateProvider>
        cert_provider = std::make_shared<
            ::grpc::experimental::StaticDataCertificateProvider>(
            _conf->get_ca(), key_cert);
    ::grpc::experimental::TlsServerCredentialsOptions creds_opts(cert_provider);
    creds_opts.set_root_cert_name("Root");
    server_creds = ::grpc::experimental::TlsServerCredentials(creds_opts);
#else
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
#endif
  } else {
    SPDLOG_LOGGER_INFO(_logger, "unencrypted server listening on {}",
                       _conf->get_hostport());
    server_creds = ::grpc::InsecureServerCredentials();
  }
  builder.AddListeningPort(_conf->get_hostport(), server_creds);
  builder.RegisterService(this);
  builder.AddChannelArgument(GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS, 1);
  builder.AddChannelArgument(GRPC_ARG_KEEPALIVE_TIME_MS,
                             _conf->get_second_keepalive_interval() * 1000);
  builder.AddChannelArgument(GRPC_ARG_KEEPALIVE_TIMEOUT_MS,
                             _conf->get_second_keepalive_interval() * 300);
  builder.AddChannelArgument(GRPC_ARG_HTTP2_MAX_PING_STRIKES, 0);
  builder.AddChannelArgument(GRPC_ARG_HTTP2_MAX_PINGS_WITHOUT_DATA, 0);
  builder.AddChannelArgument(
      GRPC_ARG_HTTP2_MIN_RECV_PING_INTERVAL_WITHOUT_DATA_MS, 60000);

  if (_conf->get_compression() == grpc_config::YES) {
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
  _server = std::unique_ptr<::grpc::Server>(builder.BuildAndStart());
}

server::pointer server::create(const grpc_config::pointer& conf) {
  server::pointer ret(new server(conf));
  ret->start();
  return ret;
}

::grpc::ServerBidiReactor<grpc_event_type, grpc_event_type>* server::exchange(
    ::grpc::CallbackServerContext* context) {
  // authorization header match?
  if (!_conf->get_authorization().empty()) {
    const auto& metas = context->client_metadata();

    auto header_search = metas.lower_bound(authorization_header);
    if (header_search == metas.end()) {
      SPDLOG_LOGGER_ERROR(_logger, "header {} not found", authorization_header);
      return nullptr;
    }
    bool found = false;
    for (; header_search != metas.end() && !found; ++header_search) {
      if (header_search->first != authorization_header) {
        SPDLOG_LOGGER_ERROR(_logger, "Wrong client authorization token");
        return nullptr;
      }
      found = _conf->get_authorization() == header_search->second;
    }
  }
  accepted_service::pointer serv;
  {
    unique_lock l(_protect);
    serv = std::make_shared<accepted_service>(_conf, _server_finished);
    _accepted.push(serv);
    _accept_cond.notify_one();
  }

  serv->start();
  return serv.get();
}

std::unique_ptr<io::stream> server::open() {
  std::unique_ptr<io::stream> ret;
  {
    unique_lock l(_protect);
    if (!_accepted.empty()) {
      ret = std::make_unique<stream>(_accepted.front());
      _accepted.pop();
    }
  }
  return ret;
}

std::unique_ptr<io::stream> server::open(
    const system_clock::time_point& dead_line) {
  std::unique_ptr<io::stream> ret;
  {
    unique_lock l(_protect);
    if (!_accepted.empty()) {
      ret = std::make_unique<stream>(_accepted.front());
      _accepted.pop();
    } else {
      _accept_cond.wait_until(l, dead_line,
                              [this]() { return !_accepted.empty(); });
      if (!_accepted.empty()) {
        ret = std::make_unique<stream>(_accepted.front());
        _accepted.pop();
      }
    }
  }
  return ret;
}

bool server::is_ready() const {
  unique_lock l(_protect);
  return !_accepted.empty();
}

void server::shutdown() {
  *_server_finished = true;
  std::unique_ptr<::grpc::Server> to_shutdown;
  std::queue<accepted_service::pointer> accepted_to_shutdown;
  {
    unique_lock l(_protect);
    if (!_accepted.empty()) {
      accepted_to_shutdown.swap(_accepted);
    }
    if (_server) {
      to_shutdown = std::move(_server);
    }
  }
  while (!accepted_to_shutdown.empty()) {
    auto svc = accepted_to_shutdown.front();
    accepted_to_shutdown.pop();
    svc->to_trash();
  }
  if (to_shutdown) {
    SPDLOG_LOGGER_DEBUG(_logger, "grpc server shutdown");
    to_shutdown->Shutdown(std::chrono::system_clock::now() +
                          std::chrono::seconds(15));
  }
}
