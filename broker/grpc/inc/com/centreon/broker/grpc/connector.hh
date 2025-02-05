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

#ifndef CCB_GRPC_CONNECTOR_HH
#define CCB_GRPC_CONNECTOR_HH

#include <memory>
#include "com/centreon/broker/io/limit_endpoint.hh"
#include "com/centreon/common/grpc/grpc_client.hh"
#include "grpc_config.hh"

namespace com::centreon::broker {

namespace grpc {
class connector : public io::limit_endpoint,
                  public com::centreon::common::grpc::grpc_client_base {
  std::unique_ptr<com::centreon::broker::stream::centreon_bbdo::Stub> _stub;

 public:
  connector(const grpc_config::pointer& conf);
  ~connector() = default;
  connector& operator=(const connector&) = delete;
  connector(const connector&) = delete;

  std::shared_ptr<io::stream> open() override;

  std::shared_ptr<io::stream> create_stream() override;
};
};  // namespace grpc

}  // namespace com::centreon::broker

#endif  // !CCB_GRPC_CONNECTOR_HH
