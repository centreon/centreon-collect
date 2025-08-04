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

#ifndef CCE_MOD_OTL_CENTREON_AGENT_AGENT_CONFIG_HH
#define CCE_MOD_OTL_CENTREON_AGENT_AGENT_CONFIG_HH

#include "com/centreon/engine/modules/opentelemetry/grpc_config.hh"

namespace com::centreon::engine::modules::opentelemetry::centreon_agent {

class agent_config {
 public:
  using grpc_config_set =
      absl::btree_set<grpc_config::pointer, grpc_config_compare>;

  using pointer = std::shared_ptr<agent_config>;

 private:
  // all endpoints engine has to connect to
  grpc_config_set _agent_grpc_reverse_conf;
  // limit the number of active checks in order to limit charge
  uint32_t _max_concurrent_checks;
  // period of metric exports (in seconds)
  uint32_t _export_period;
  // after this timeout, process is killed (in seconds)
  uint32_t _check_timeout;

 public:
  agent_config(const rapidjson::Value& json_config_v);

  agent_config();

  // used for tests
  agent_config(uint32_t max_concurrent_checks,
               uint32_t export_period,
               uint32_t check_timeout);

  agent_config(uint32_t max_concurrent_checks,
               uint32_t export_period,
               uint32_t check_timeout,
               const std::initializer_list<grpc_config::pointer>& endpoints);

  const grpc_config_set& get_agent_grpc_reverse_conf() const {
    return _agent_grpc_reverse_conf;
  }

  uint32_t get_max_concurrent_checks() const { return _max_concurrent_checks; }
  uint32_t get_export_period() const { return _export_period; }
  uint32_t get_check_timeout() const { return _check_timeout; }

  bool operator==(const agent_config& right) const;

  bool operator!=(const agent_config& right) const { return !(*this == right); }
};

};  // namespace com::centreon::engine::modules::opentelemetry::centreon_agent
#endif
