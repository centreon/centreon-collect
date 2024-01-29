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

using namespace com::centreon::engine::modules::otl_server;
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
        "server": {
            "description": "otel grpc config",
            "type": "object"
        }
    },
    "required": [
        "server"
    ],
    "type": "object"
})");

/**
 * @brief Construct a new otl config::otl config object
 *
 * @param file_path path of the config file
 * @throw msg fmt if file can not be read or if json content is not accepted by
 * validator
 */
otl_config::otl_config(const std::string_view& file_path) {
  static json_validator validator(_grpc_config_schema);
  rapidjson::Document file_content_d =
      rapidjson_helper::read_from_file(file_path);

  rapidjson_helper file_content(file_content_d);

  file_content.validate(validator);
  _max_length_grpc_log = file_content.get_int("max_length_grpc_log");
  _grpc_conf = std::make_shared<grpc_config>(file_content.get_member("server"));
}

bool otl_config::operator==(const otl_config& right) const {
  if (!_grpc_conf || !right._grpc_conf) {
    return false;
  }
  return *_grpc_conf == *right._grpc_conf &&
         _max_length_grpc_log == right._max_length_grpc_log;
}
