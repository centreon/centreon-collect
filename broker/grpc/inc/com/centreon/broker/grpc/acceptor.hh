/**
 * Copyright 2022 Centreon
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

#ifndef CCB_GRPC_ACCEPTOR_HH
#define CCB_GRPC_ACCEPTOR_HH

#include <memory>
#include "com/centreon/broker/io/endpoint.hh"
#include "com/centreon/common/grpc/grpc_server.hh"
#include "grpc_config.hh"

namespace com::centreon::broker::grpc {

/**
 * @brief grpc service implementation
 * ::grpc::Server::Shutdown doesn't return until all BiReactor has been stopped,
 * so we must store a reference on each in order to shutdown them first
 * This is the reason of _accepted member
 *
 */
class service_impl
    : public com::centreon::broker::stream::centreon_bbdo::Service,
      public std::enable_shared_from_this<service_impl> {
  grpc_config::pointer _conf;

  std::shared_ptr<spdlog::logger> _logger;

  std::set<std::shared_ptr<io::stream>> _accepted;
  mutable std::mutex _accepted_m;
  std::deque<std::shared_ptr<io::stream>> _wait_to_open;
  std::condition_variable _wait_cond;
  std::shared_ptr<asio::io_context> _io_context;
  mutable std::mutex _wait_m;

 public:
  service_impl(const grpc_config::pointer& conf,
               const std::shared_ptr<asio::io_context> io_context,
               const std::shared_ptr<spdlog::logger>& logger);

  void init();

  // disable synchronous version of this method
  ::grpc::Status exchange(
      ::grpc::ServerContext* /*context*/,
      ::grpc::ServerReaderWriter<
          ::com::centreon::broker::stream::CentreonEvent,
          ::com::centreon::broker::stream::CentreonEvent>* /*stream*/)
      override {
    abort();
    return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
  }

  ::grpc::ServerBidiReactor<::com::centreon::broker::stream::CentreonEvent,
                            ::com::centreon::broker::stream::CentreonEvent>*
  exchange(::grpc::CallbackServerContext* context);

  const grpc_config::pointer& get_conf() const { return _conf; }

  std::shared_ptr<io::stream> get_wait();

  bool has_wait_stream() const;
  void shutdown_all_wait();
  void shutdown_all_accepted();
  void register_accepted(const std::shared_ptr<io::stream>& to_register);
  void unregister(const std::shared_ptr<io::stream>& to_unregister);
};

class acceptor : public io::endpoint,
                 public com::centreon::common::grpc::grpc_server_base {
  std::shared_ptr<service_impl> _service;

 public:
  acceptor(const grpc_config::pointer& conf);
  ~acceptor();

  acceptor(const acceptor&) = delete;
  acceptor& operator=(const acceptor&) = delete;

  std::shared_ptr<io::stream> open() override;
  bool is_ready() const override;
};
}  // namespace com::centreon::broker::grpc

#endif  // !CCB_GRPC_ACCEPTOR_HH
