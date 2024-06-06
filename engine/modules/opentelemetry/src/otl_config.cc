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
        "second_fifo_expiry": {
            "description:": "lifetime of data points in fifos",
            "type": "integer",
            "min": 30
        },
        "max_fifo_size": {
            "description:": "max number of data points in fifos",
            "type": "integer",
            "min": 1
        },
        "server": {
            "description": "otel grpc config",
            "type": "object"
        },
        "telegraf_conf_server": {
            "description": "http(s) telegraf config server",
            "type": "object"
        }
    },
    "required": [
        "server"
    ],
    "type": "object"
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
  rapidjson::Document file_content_d =
      rapidjson_helper::read_from_file(file_path);

  rapidjson_helper file_content(file_content_d);

  file_content.validate(validator);
  _max_length_grpc_log = file_content.get_unsigned("max_length_grpc_log", 400);
  _json_grpc_log = file_content.get_bool("grpc_json_log", false);
  _second_fifo_expiry = file_content.get_unsigned("second_fifo_expiry", 600);
  _max_fifo_size = file_content.get_unsigned("max_fifo_size", 5);
  _grpc_conf = std::make_shared<grpc_config>(file_content.get_member("server"));
  if (file_content.has_member("telegraf_conf_server")) {
    _telegraf_conf_server_config =
        std::make_shared<telegraf::conf_server_config>(
            file_content.get_member("telegraf_conf_server"), io_context);
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
             _json_grpc_log == right._json_grpc_log &&
             _second_fifo_expiry == right._second_fifo_expiry &&
             _max_fifo_size == right._max_fifo_size;

  if (_telegraf_conf_server_config && right._telegraf_conf_server_config) {
    return *_telegraf_conf_server_config == *right._telegraf_conf_server_config;
  }
  return !_telegraf_conf_server_config && !right._telegraf_conf_server_config;
}
