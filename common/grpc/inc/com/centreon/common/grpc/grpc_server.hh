/*
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
#ifndef COMMON_GRPC_SERVER_HH
#define COMMON_GRPC_SERVER_HH

#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include "com/centreon/common/grpc/grpc_config.hh"

namespace com::centreon::common::grpc {

/**
 * @brief base class to create a grpc server
 *
 */
class grpc_server_base {
  grpc_config::pointer _conf;
  std::shared_ptr<spdlog::logger> _logger;
  std::unique_ptr<::grpc::Server> _server;

 protected:
  using builder_option = std::function<void(::grpc::ServerBuilder&)>;
  void _init(const builder_option& options);

 public:
  grpc_server_base(const grpc_config::pointer& conf,
                   const std::shared_ptr<spdlog::logger>& logger);

  virtual ~grpc_server_base();

  void shutdown(const std::chrono::system_clock::duration& timeout);

  grpc_server_base(const grpc_server_base&) = delete;
  grpc_server_base& operator=(const grpc_server_base&) = delete;

  const grpc_config::pointer& get_conf() const { return _conf; }
  const std::shared_ptr<spdlog::logger>& get_logger() const { return _logger; }

  bool initialized() const { return _server.get(); }
};

}  // namespace com::centreon::common::grpc

#endif
