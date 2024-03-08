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
#include "com/centreon/engine/log_v2.hh"

#include "telegraf/conf_server.hh"

using namespace com::centreon::engine::modules::opentelemetry::telegraf;

static constexpr std::string_view _config_schema(R"(
{
    "$schema": "http://json-schema.org/draft-04/schema#",
    "title": "grpc config",
    "properties": {
        "listen_address": {
            "description": "IP",
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
        "keepalive_interval": {
            "description": "delay between 2 keepalive tcp packet, 0 no keepalive packets",
            "type": "integer",
            "minimum": 0,
            "maximum": 3600
        }
    }
    "type": "object"
}

)");

conf_server_config::conf_server_config(const rapidjson::Value& json_config_v) {
  common::rapidjson_helper json_config(json_config_v);

  static common::json_validator validator(_config_schema);
  try {
    json_config.validate(validator);
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(log_v2::config(),
                        "forbidden values in telegraf conf server config: {}",
                        e.what());
    throw;
  }
  _listen_address = json_config.get_string("listen_address", "0.0.0.0");
  _crypted = json_config.get_bool("encryption", true);
  _port = json_config.get_unsigned("port", _crypted ? 443 : 80);
  _second_keep_alive_interval =
      json_config.get_unsigned("keepalive_interval", 30);
}

bool conf_server_config::operator==(const conf_server_config& right) const {
  return _listen_address == right._listen_address && _port == right._port &&
         _crypted == right._crypted &&
         _second_keep_alive_interval == right._second_keep_alive_interval;
}
