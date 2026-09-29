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
#include "com/centreon/engine/rpc_deprecation_interceptor.hh"

#include <boost/asio.hpp>
namespace asio = boost::asio;

#include <google/protobuf/descriptor.h>

#include <absl/container/flat_hash_set.h>
#include <mutex>

#include "com/centreon/engine/globals.hh"

namespace com::centreon::engine {

namespace {
/* A deprecated method is reported once per Engine run, on its first call, so
 * that a PHP not yet migrated does not flood the log. */
std::mutex warned_m;
absl::flat_hash_set<std::string> warned;
}  // namespace

/**
 * @brief Constructor.
 *
 * @param method The gRPC method, as ServerRpcInfo gives it:
 * "/com.centreon.engine.Engine/ProcessServiceCheckResult".
 */
rpc_deprecation_interceptor::rpc_deprecation_interceptor(std::string method)
    : _method{std::move(method)} {}

/**
 * @brief Interception hook: on the reception of the call, log the deprecation
 * warning if this method has not been reported yet during this Engine run.
 *
 * @param methods The batch given by gRPC.
 */
void rpc_deprecation_interceptor::Intercept(
    grpc::experimental::InterceptorBatchMethods* methods) {
  if (methods->QueryInterceptionHookPoint(
          grpc::experimental::InterceptionHookPoints::
              POST_RECV_INITIAL_METADATA)) {
    bool first_call = false;
    {
      std::lock_guard<std::mutex> lck(warned_m);
      first_call = warned.insert(_method).second;
    }
    if (first_call)
      external_command_logger->warn(
          "gRPC method {} is deprecated: use Broker's gRPC API instead, the "
          "typed RPC when one exists or ExecuteExternalCommand otherwise "
          "(reported once per Engine run)",
          _method);
  }
  methods->Proceed();
}

/**
 * @brief Tell whether a gRPC method carries `option deprecated = true` in its
 * proto definition.
 *
 * @param grpc_method The method as gRPC names it: "/package.Service/Method".
 *
 * @return true if the method is known and deprecated.
 */
bool rpc_deprecation_interceptor_factory::is_deprecated(
    std::string_view grpc_method) {
  /* "/pkg.Service/Method" -> "pkg.Service.Method" */
  if (grpc_method.empty() || grpc_method.front() != '/')
    return false;
  std::string full_name(grpc_method.substr(1));
  auto slash = full_name.find('/');
  if (slash == std::string::npos)
    return false;
  full_name[slash] = '.';
  const google::protobuf::MethodDescriptor* md =
      google::protobuf::DescriptorPool::generated_pool()->FindMethodByName(
          full_name);
  return md && md->options().deprecated();
}

/**
 * @brief Called by gRPC for every incoming call.
 *
 * @param info Information on the call, in particular its method.
 *
 * @return A new interceptor for a deprecated method, nullptr otherwise.
 */
grpc::experimental::Interceptor*
rpc_deprecation_interceptor_factory::CreateServerInterceptor(
    grpc::experimental::ServerRpcInfo* info) {
  const char* method = info->method();
  if (method && is_deprecated(method))
    return new rpc_deprecation_interceptor(method);
  return nullptr;
}

}  // namespace com::centreon::engine
