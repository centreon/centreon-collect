/**
 * Copyright 2019 Centreon (https://www.centreon.com/)
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

#include <grpcpp/server_builder.h>

#include <memory>
#include <vector>

#include "com/centreon/engine/host.hh"

#include "com/centreon/engine/enginerpc.hh"
#include "com/centreon/engine/rpc_deprecation_interceptor.hh"

using namespace com::centreon::engine;

enginerpc::enginerpc(const std::string& address, uint16_t port) {
  std::string server_address{fmt::format("{}:{}", address, port)};
  grpc::ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&_service);
  /* Warn when a command RPC deprecated in engine.proto is still called. */
  std::vector<
      std::unique_ptr<grpc::experimental::ServerInterceptorFactoryInterface>>
      creators;
  creators.emplace_back(
      std::make_unique<rpc_deprecation_interceptor_factory>());
  builder.experimental().SetInterceptorCreators(std::move(creators));
  _server = builder.BuildAndStart();
}

void enginerpc::shutdown() {
  if (_server) {
    _server->Shutdown();
    _server->Wait();
  }
}
