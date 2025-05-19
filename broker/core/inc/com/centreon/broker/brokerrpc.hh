/**
 * Copyright 2020-2023 Centreon (https://www.centreon.com/)
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

#ifndef CENTREON_BROKER_CORE_SRC_BROKERRPC_HH_
#define CENTREON_BROKER_CORE_SRC_BROKERRPC_HH_

#include <grpcpp/server.h>

#include "com/centreon/broker/broker_impl.hh"

namespace com::centreon::broker {

/**
 * @class brokerrpc brokerrpc.hh "com/centreon/broker/brokerrpc.hh"
 * @brief This is the entry point to the Broker gRPC server
 */
class brokerrpc final {
  broker_impl _service;
  std::unique_ptr<grpc::Server> _server;

 public:
  brokerrpc(const std::string& address,
            uint16_t port,
            const std::string& broker_name);
  brokerrpc() = delete;
  brokerrpc(const brokerrpc&) = delete;
  ~brokerrpc() = default;
  void shutdown();
};

}  // namespace com::centreon::broker

#endif  // CENTREON_BROKER_CORE_SRC_BROKERRPC_HH_
