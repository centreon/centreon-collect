/*
 * Copyright 2024 Centreon
 *
 * This file is part of Centreon Engine.
 *
 * Centreon Engine is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * Centreon Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Centreon Engine. If not, see
 * <http://www.gnu.org/licenses/>.
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
  // delay between 2 checks of one service, so we will do all check in that
  // period (in seconds)
  uint32_t _check_interval;
  // limit the number of active checks in order to limit charge
  uint32_t _max_concurrent_checks;
  // period of metric exports (in seconds)
  uint32_t _export_period;
  // after this timeout, process is killed (in seconds)
  uint32_t _check_timeout;

 public:
  agent_config(const rapidjson::Value& json_config_v);

  // used for tests
  agent_config(uint32_t check_interval,
               uint32_t max_concurrent_checks,
               uint32_t export_period,
               uint32_t check_timeout);

  agent_config(uint32_t check_interval,
               uint32_t max_concurrent_checks,
               uint32_t export_period,
               uint32_t check_timeout,
               const std::initializer_list<grpc_config::pointer>& endpoints);

  const grpc_config_set& get_agent_grpc_reverse_conf() const {
    return _agent_grpc_reverse_conf;
  }

  uint32_t get_check_interval() const { return _check_interval; }
  uint32_t get_max_concurrent_checks() const { return _max_concurrent_checks; }
  uint32_t get_export_period() const { return _export_period; }
  uint32_t get_check_timeout() const { return _check_timeout; }

  bool operator==(const agent_config& right) const;

  bool operator!=(const agent_config& right) const { return !(*this == right); }
};

};  // namespace com::centreon::engine::modules::opentelemetry::centreon_agent
#endif
