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
#include "com/centreon/engine/globals.hh"

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
          "description": "encryption mode: full, insecure, or no",
            "anyOf": [
              { "type": "boolean" },
              { "type": "string", "enum": ["full", "insecure", "no", "true", "false"] }
            ]
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
        },
        "second_max_reconnect_backoff": {
            "description": "maximum time between subsequent connection attempts, in seconds. Default: 60s",
            "type": "integer",
            "minimum": 0,
            "maximum": 600
        },
        "max_message_length": {
            "description": "maximum protobuf message length in Mo",
            "type": "integer",
            "minimum": 4
        }
    },
    "required": [
                 "host",
                 "port"
                 ],
    "type": "object"
}

)");

using namespace com::centreon::engine::modules::opentelemetry;

/**
 * @brief Construct a new grpc config::grpc config object
 *
 * @param json_config_v content of the json config file
 */
grpc_config::grpc_config(const rapidjson::Value& json_config_v) {
  common::rapidjson_helper json_config(json_config_v);

  static common::json_validator validator(_grpc_config_schema);
  try {
    json_config.validate(validator);
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(config_logger,
                        "forbidden values in grpc otl config: {}", e.what());
    throw;
  }

  std::string hostport = fmt::format("{}:{}", json_config.get_string("host"),
                                     json_config.get_unsigned("port"));
  bool crypted = false;
  std::string certificate, cert_key, ca_cert;
  std::string ca_name;
  bool compress = false;
  int second_keepalive_interval;

  if (json_config.has_member("encryption")) {
    const auto& encryption_value = json_config_v["encryption"];
    if (encryption_value.IsString()) {
      const std::string& encryption = json_config.get_string("encryption");
      crypted = (encryption == "full" || encryption == "true");
    } else if (encryption_value.IsBool()) {
      crypted = encryption_value.GetBool();
    }
  }

  read_file(json_config_v, "public_cert", certificate);
  read_file(json_config_v, "private_key", cert_key);
  read_file(json_config_v, "ca_certificate", ca_cert);
  if (json_config.has_member("ca_name"))
    ca_name = json_config.get_string("ca_name");
  if (json_config.has_member("compression"))
    compress = json_config.get_bool("compression");

  if (json_config.has_member("keepalive_interval"))
    second_keepalive_interval = json_config.get_int("keepalive_interval");
  else
    second_keepalive_interval = 30;

  unsigned second_max_reconnect_backoff =
      json_config.get_unsigned("second_max_reconnect_backoff", 60);

  unsigned max_message_length =
      json_config.get_unsigned("max_message_length", 4) * 1024 * 1024;

  static_cast<common::grpc::grpc_config&>(*this) = common::grpc::grpc_config(
      hostport, crypted, certificate, cert_key, ca_cert, ca_name, compress,
      second_keepalive_interval, second_max_reconnect_backoff,
      max_message_length);
}

/**
 * @brief read a file as certificate
 *
 * @param json_config
 * @param key json key that contains file path
 * @param file_content out: file content
 */
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
    SPDLOG_LOGGER_ERROR(config_logger, "fail to read {}: {}", path, e.what());
    throw;
  }
}

bool grpc_config::operator==(const grpc_config& right) const {
  return static_cast<const common::grpc::grpc_config>(*this) ==
         static_cast<const common::grpc::grpc_config>(right);
}
