/**
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

#ifndef CCE_MOD_OTL_SERVER_GRPC_CONFIG_HH
#define CCE_MOD_OTL_SERVER_GRPC_CONFIG_HH

#include "com/centreon/common/grpc/grpc_config.hh"

namespace com::centreon::engine::modules::opentelemetry {

class grpc_config : public common::grpc::grpc_config {
  static void read_file(const rapidjson::Value& json_config,
                        const std::string_view& key,
                        std::string& file_content);

 public:
  using pointer = std::shared_ptr<grpc_config>;

  grpc_config() {}
  grpc_config(const std::string& hostp, bool crypted)
      : common::grpc::grpc_config(hostp, crypted) {}

  grpc_config(const rapidjson::Value& json_config);

  bool operator==(const grpc_config& right) const;

  inline bool operator!=(const grpc_config& right) const {
    return !(*this == right);
  }
};

struct grpc_config_compare {
  bool operator()(const grpc_config::pointer& left,
                  const grpc_config::pointer& right) const {
    return left->compare(*right) < 0;
  }
};

}  // namespace com::centreon::engine::modules::opentelemetry

#endif  // !CCE_MOD_OTL_SERVER_GRPC_CONFIG_HH
