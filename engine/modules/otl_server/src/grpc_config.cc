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

#include "grpc_config.hh"
#include "com/centreon/common/rapidjson_helper.hh"
#include "com/centreon/engine/log_v2.hh"

using com::centreon::common::rapidjson_helper;

static constexpr std::string_view _grpc_config_schema(R"(
{
    "$schema": "http://json-schema.org/draft-04/schema#",
    "title": "grpc config",
    "properties": {
        "host": {
            "description": "IP or dns name",
            "type": "string",
            "minLength": 5
        },
        "port": {
            "description": "port to listen",
            "type": "integer",
            "minimum": 1024,
            "maximum": 65535
        },
        "encryption": {
            "description": "true if https",
            "type": "boolean"
        },
        "public_cert": {
            "description": "path of certificate file .crt",
            "type": "string"
        },
        "private_key": {
            "description": "path of certificate file .key",
            "type": "string"
        },
        "ca_certificate": {
            "description": "path of authority certificate file .crt",
            "type": "string"
        },
        "ca_name": {
            "description": "name of authority certificate",
            "type": "string"
        },
        "compression": {
            "description": "activate compression",
            "type": "boolean"
        },
        "keepalive_interval": {
            "description": "delay between 2 keepalive tcp packet",
            "type": "integer",
            "minimum": -1,
            "maximum": 3600
        }
    },
    "required": [
                 "host",
                 "port"
                 ],
    "type": "object"
}

)");

using namespace com::centreon::engine::modules::otl_server;

grpc_config::grpc_config(const rapidjson::Value& json_config_v) {
  common::rapidjson_helper json_config(json_config_v);

  static common::json_validator validator(_grpc_config_schema);
  try {
    json_config.validate(validator);
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(log_v2::config(),
                        "forbidden values in grpc otl config: {}", e.what());
    throw;
  }

  _hostport = fmt::format("{}:{}", json_config.get_string("host"),
                          json_config.get_unsigned("port"));
  if (json_config.has_member("encryption"))
    _crypted = json_config.get_bool("encryption");
  else
    _crypted = false;
  read_file(json_config_v, "public_cert", _certificate);
  read_file(json_config_v, "private_key", _cert_key);
  read_file(json_config_v, "ca_certificate", _ca_cert);
  if (json_config.has_member("ca_name"))
    _ca_name = json_config.get_string("ca_name");
  if (json_config.has_member("compression"))
    _compress = json_config.get_bool("compression");
  else
    _compress = false;
  if (json_config.has_member("keepalive_interval"))
    _second_keepalive_interval = json_config.get_int("keepalive_interval");
  else
    _second_keepalive_interval = 30;
}

void grpc_config::read_file(const rapidjson::Value& json_config,
                            const std::string_view& key,
                            std::string& file_content) {
  std::string path;
  try {
    path = rapidjson_helper(json_config).get_string(key.data());
  } catch (const std::exception&) {
    return;
  }
  try {
    boost::trim(path);
    if (path.empty()) {
      return;
    }
    std::ifstream file(path);
    std::stringstream ss;
    ss << file.rdbuf();
    file.close();
    file_content = ss.str();
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(log_v2::config(), "fail to read {}: {}", path,
                        e.what());
    throw;
  }
}
