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
#include "com/centreon/engine/globals.hh"

#include "centreon_agent/agent.grpc.pb.h"

#include "otl_config.hh"
#include "otl_fmt.hh"

int fmt::formatter< ::opentelemetry::proto::collector::metrics::v1::
                        ExportMetricsServiceRequest>::max_length_log = -1;

bool fmt::formatter< ::opentelemetry::proto::collector::metrics::v1::
                         ExportMetricsServiceRequest>::json_grpc_format = false;

using namespace com::centreon::engine::modules::opentelemetry;
using namespace com::centreon::common;

static constexpr std::string_view _grpc_config_schema(R"(
{
    "$schema": "http://json-schema.org/draft-04/schema#",
    "title": "opentelemetry config",
    "properties": {
        "max_length_grpc_log": {
            "description:": "max dump length of a otel grpc object",
            "type": "integer",
            "min": -1
        },
        "grpc_json_log": {
            "description": "true if we log otl grpc object to json format",
            "type": "boolean"
        },
        "otel_server": {
            "description": "otel grpc config",
            "type": "object"
        },
        "telegraf_conf_server": {
            "description": "http(s) telegraf config server",
            "type": "object"
        },
        "centreon_agent": {
            "description": "config of centreon_agent",
            "type": "object"
        }
      }, 
      "type" : "object"
}
)");

/**
 * @brief Construct a new otl config::otl config object
 *
 * @param file_path path of the config file
 * @throw msg fmt if file can not be read or if json content is not accepted by
 * validator
 */
otl_config::otl_config(const std::string_view& file_path,
                       asio::io_context& io_context) {
  static json_validator validator(_grpc_config_schema);
  rapidjson::Document file_content_d;
  try {
    file_content_d = rapidjson_helper::read_from_file(file_path);
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(config_logger, "incorrect json file {}: {}", file_path,
                        e.what());
    throw;
  }

  rapidjson_helper file_content(file_content_d);

  file_content.validate(validator);
  _max_length_grpc_log = file_content.get_unsigned("max_length_grpc_log", 400);
  _json_grpc_log = file_content.get_bool("grpc_json_log", false);
  if (file_content.has_member("otel_server")) {
    try {
      _grpc_conf =
          std::make_shared<grpc_config>(file_content.get_member("otel_server"));
    } catch (const std::exception& e) {
      SPDLOG_LOGGER_ERROR(config_logger,
                          "fail to parse otl_server object: ", e.what());
      throw;
    }
  }

  if (file_content.has_member("centreon_agent")) {
    try {
      _centreon_agent_config = std::make_shared<centreon_agent::agent_config>(
          file_content.get_member("centreon_agent"));
    } catch (const std::exception& e) {
      SPDLOG_LOGGER_ERROR(
          config_logger,
          "fail to parse centreon agent conf server object: ", e.what());
      throw;
    }
  }

  // nor server nor reverse client?
  if (!_grpc_conf &&
      !(_centreon_agent_config &&
        !_centreon_agent_config->get_agent_grpc_reverse_conf().empty())) {
    throw exceptions::msg_fmt(
        "nor an grpc server, nor a reverse client configured");
  }

  if (!_centreon_agent_config) {
    _centreon_agent_config = std::make_shared<centreon_agent::agent_config>();
  }

  if (file_content.has_member("telegraf_conf_server")) {
    try {
      _telegraf_conf_server_config =
          std::make_shared<telegraf::conf_server_config>(
              file_content.get_member("telegraf_conf_server"), io_context);
    } catch (const std::exception& e) {
      SPDLOG_LOGGER_ERROR(
          config_logger,
          "fail to parse telegraf conf server object: ", e.what());
      throw;
    }
  }
}

/**
 * @brief compare two otl_config
 *
 * @param right
 * @return true if are equals
 * @return false
 */
bool otl_config::operator==(const otl_config& right) const {
  if (!_grpc_conf || !right._grpc_conf) {
    return false;
  }
  bool ret = *_grpc_conf == *right._grpc_conf &&
             _max_length_grpc_log == right._max_length_grpc_log &&
             _json_grpc_log == right._json_grpc_log;

  if (!ret) {
    return false;
  }

  if (_telegraf_conf_server_config && right._telegraf_conf_server_config) {
    return *_telegraf_conf_server_config == *right._telegraf_conf_server_config;
  }
  return !_telegraf_conf_server_config && !right._telegraf_conf_server_config;
}
