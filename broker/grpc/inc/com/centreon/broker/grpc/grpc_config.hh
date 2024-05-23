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

#ifndef CCB_GRPC_CONFIG_HH
#define CCB_GRPC_CONFIG_HH

#include "com/centreon/common/grpc/grpc_config.hh"

namespace com::centreon::broker::grpc {

class grpc_config : public com::centreon::common::grpc::grpc_config {
  const std::string _authorization;
  /**
   * @brief when _grpc_serialized is set, bbdo events are sent on the wire
   * without bbdo stream serialization
   *
   */
  const bool _grpc_serialized;

 public:
  using pointer = std::shared_ptr<grpc_config>;

  grpc_config() : _grpc_serialized(false) {}
  grpc_config(const std::string& hostp)
      : com::centreon::common::grpc::grpc_config(hostp),
        _grpc_serialized(false) {}
  grpc_config(const std::string& hostp,
              bool crypted,
              const std::string& certificate,
              const std::string& cert_key,
              const std::string& ca_cert,
              const std::string& authorization,
              const std::string& ca_name,
              bool compression,
              int second_keepalive_interval,
              bool grpc_serialized)
      : com::centreon::common::grpc::grpc_config(hostp,
                                                 crypted,
                                                 certificate,
                                                 cert_key,
                                                 ca_cert,
                                                 ca_name,
                                                 compression,
                                                 second_keepalive_interval),
        _authorization(authorization),
        _grpc_serialized(grpc_serialized) {}

  constexpr const std::string& get_authorization() const {
    return _authorization;
  }
  constexpr bool get_grpc_serialized() const { return _grpc_serialized; }
};

}  // namespace com::centreon::broker::grpc

#endif  // !CCB_GRPC_CONFIG_HH
