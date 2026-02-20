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
#ifndef COMMON_GRPC_CLIENT_HH
#define COMMON_GRPC_CLIENT_HH

#include <grpcpp/channel.h>
#include <grpcpp/impl/codegen/interceptor.h>
#include "com/centreon/common/grpc/grpc_config.hh"

namespace com::centreon::common::grpc {

/*
 * @brief Interceptor that injects a token into the metadata of each outgoing
 * gRPC request.
 */
class TokenInjectingInterceptor : public ::grpc::experimental::Interceptor {
  std::string _token;

 public:
  TokenInjectingInterceptor(const std::string& token) : _token(token) {}

  void Intercept(
      ::grpc::experimental::InterceptorBatchMethods* methods) override {
    if (methods->QueryInterceptionHookPoint(
            ::grpc::experimental::InterceptionHookPoints::
                PRE_SEND_INITIAL_METADATA)) {
      auto* meta = methods->GetSendInitialMetadata();
      meta->insert({"authorization", "Bearer " + _token});
    }
    methods->Proceed();
  }
};
/*
 * @brief Factory for creating TokenInjectingInterceptor instances.
 */
class TokenInterceptorFactory
    : public ::grpc::experimental::ClientInterceptorFactoryInterface {
  std::string _token;

 public:
  TokenInterceptorFactory(std::string token) : _token(std::move(token)) {}

  ::grpc::experimental::Interceptor* CreateClientInterceptor(
      ::grpc::experimental::ClientRpcInfo* info [[maybe_unused]]) override {
    return new TokenInjectingInterceptor(_token);
  }
};

/**
 * @brief base class to create a grpc client
 * It only creates grpc channel not stub
 *
 */
class grpc_client_base {
  grpc_config::pointer _conf;
  std::shared_ptr<spdlog::logger> _logger;

 protected:
  std::shared_ptr<::grpc::Channel> _channel;

 public:
  grpc_client_base(const grpc_config::pointer& conf,
                   const std::shared_ptr<spdlog::logger>& logger);

  const grpc_config::pointer& get_conf() const { return _conf; }
  const std::shared_ptr<spdlog::logger>& get_logger() const { return _logger; }
};

}  // namespace com::centreon::common::grpc

#endif
