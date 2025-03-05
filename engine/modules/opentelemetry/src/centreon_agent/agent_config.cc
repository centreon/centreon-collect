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

#include "com/centreon/common/rapidjson_helper.hh"

#include "centreon_agent/agent_config.hh"

#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::engine::modules::opentelemetry::centreon_agent;
using namespace com::centreon::common;

static constexpr std::string_view _config_schema(R"(
{
    "$schema": "http://json-schema.org/draft-04/schema#",
    "title": "centreon agent config",
    "properties": {
        "check_interval": {
            "description": "interval in seconds between two checks",
            "type": "integer",
            "minimum": 10
        },
        "max_concurrent_checks": {
            "description": "maximum of running checks at the same time",
            "type": "integer",
            "minimum": 1
        },
        "export_period": {
            "description": "period in second of agent metric export",
            "type": "integer",
            "minimum": 10
        },
        "check_timeout": {
            "description": "check running timeout",
            "type": "integer",
            "minimum": 1
        },
        "reverse_connections": {
            "description": "array of agent endpoints (reverse mode, engine connects to centreon-agent) ",
            "type": "array",
            "items": {
              "type" : "object"
            }
        }
    },
    "type": "object"
}
)");

constexpr unsigned default_check_interval = 60;
constexpr unsigned default_max_concurrent_checks = 100;
constexpr unsigned default_export_period = 60;
constexpr unsigned default_check_timeout = 30;

/**
 * @brief Construct a new agent config::agent from json data
 *
 * @param json_config_v
 */
agent_config::agent_config(const rapidjson::Value& json_config_v) {
  static json_validator validator(_config_schema);

  rapidjson_helper file_content(json_config_v);

  file_content.validate(validator);

  _check_interval =
      file_content.get_unsigned("check_interval", default_check_interval);
  _max_concurrent_checks = file_content.get_unsigned(
      "max_concurrent_checks", default_max_concurrent_checks);
  _export_period =
      file_content.get_unsigned("export_period", default_export_period);
  _check_timeout =
      file_content.get_unsigned("check_timeout", default_check_timeout);

  if (file_content.has_member("reverse_connections")) {
    const auto& reverse_array = file_content.get_member("reverse_connections");
    for (auto conf_iter = reverse_array.Begin();
         conf_iter != reverse_array.End(); ++conf_iter) {
      _agent_grpc_reverse_conf.insert(
          std::make_shared<grpc_config>(*conf_iter));
    }
  }
}

/**
 * @brief default constructor with the same values as default json values
 *
 */
agent_config::agent_config()
    : _check_interval(default_check_interval),
      _max_concurrent_checks(default_max_concurrent_checks),
      _export_period(default_export_period),
      _check_timeout(default_check_timeout) {}

/**
 * @brief Constructor used by tests
 *
 * @param check_interval
 * @param max_concurrent_checks
 * @param export_period
 * @param check_timeout
 */
agent_config::agent_config(uint32_t check_interval,
                           uint32_t max_concurrent_checks,
                           uint32_t export_period,
                           uint32_t check_timeout)
    : _check_interval(check_interval),
      _max_concurrent_checks(max_concurrent_checks),
      _export_period(export_period),
      _check_timeout(check_timeout) {}

/**
 * @brief Constructor used by tests
 *
 * @param check_interval
 * @param max_concurrent_checks
 * @param export_period
 * @param check_timeout
 * @param endpoints
 */
agent_config::agent_config(
    uint32_t check_interval,
    uint32_t max_concurrent_checks,
    uint32_t export_period,
    uint32_t check_timeout,
    const std::initializer_list<grpc_config::pointer>& endpoints)
    : _agent_grpc_reverse_conf(endpoints),
      _check_interval(check_interval),
      _max_concurrent_checks(max_concurrent_checks),
      _export_period(export_period),
      _check_timeout(check_timeout) {}

/**
 * @brief equality operator
 *
 * @param right
 * @return true
 * @return false
 */
bool agent_config::operator==(const agent_config& right) const {
  if (_check_interval != right._check_interval ||
      _max_concurrent_checks != right._max_concurrent_checks ||
      _export_period != right._export_period ||
      _check_timeout != right._check_timeout ||
      _agent_grpc_reverse_conf.size() != right._agent_grpc_reverse_conf.size())
    return false;

  for (auto rev_conf_left = _agent_grpc_reverse_conf.begin(),
            rev_conf_right = right._agent_grpc_reverse_conf.begin();
       rev_conf_left != _agent_grpc_reverse_conf.end();
       ++rev_conf_left, ++rev_conf_right) {
    if (**rev_conf_left != **rev_conf_right)
      return false;
  }
  return true;
}
