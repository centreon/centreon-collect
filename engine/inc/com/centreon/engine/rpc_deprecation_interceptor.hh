/**
 * Copyright 2026 Centreon (https://www.centreon.com/)
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
 */
#ifndef CCE_ENGINERPC_RPC_DEPRECATION_INTERCEPTOR_HH
#define CCE_ENGINERPC_RPC_DEPRECATION_INTERCEPTOR_HH

#include <grpcpp/support/server_interceptor.h>

namespace com::centreon::engine {

/**
 * @brief Server interceptor attached to the Engine gRPC methods marked
 * `option deprecated = true` in engine.proto: the command RPCs PHP must now
 * send through Broker's ExecuteExternalCommand. It logs a warning the first
 * time each such method is called during the Engine run. The proto file is the
 * single source of truth: nothing to maintain here when a method is (un)marked.
 */
class rpc_deprecation_interceptor final
    : public grpc::experimental::Interceptor {
  const std::string _method;

 public:
  explicit rpc_deprecation_interceptor(std::string method);
  void Intercept(grpc::experimental::InterceptorBatchMethods* methods) override;
};

/**
 * @brief Factory handed to the ServerBuilder: creates an interceptor for the
 * deprecated methods only, nullptr (no interceptor) for the others.
 */
class rpc_deprecation_interceptor_factory final
    : public grpc::experimental::ServerInterceptorFactoryInterface {
 public:
  grpc::experimental::Interceptor* CreateServerInterceptor(
      grpc::experimental::ServerRpcInfo* info) override;

  static bool is_deprecated(std::string_view grpc_method);
};

}  // namespace com::centreon::engine

#endif /* !CCE_ENGINERPC_RPC_DEPRECATION_INTERCEPTOR_HH */
