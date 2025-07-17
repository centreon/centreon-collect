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

#include <rapidjson/document.h>

#include "com/centreon/common/rapidjson_helper.hh"
#include "config.hh"

using namespace com::centreon::agent;
using com::centreon::common::rapidjson_helper;

const std::string_view config::config_schema(R"(
{
    "$schema": "http://json-schema.org/draft-04/schema#",
    "title": "agent config",
    "properties": {
        "host": {
            "description": "Name of the host as it is configured in centreon. If omitted, the system hostname will be used",
            "type": "string",
            "minLength": 5
        },
        "endpoint": {
            "description": "Endpoint where agent has to connect to on the poller side or listening endpoint on the agent side in case of reverse_connection",
            "type": "string",
            "pattern": "[\\w\\.:]+:\\w+"
        },
        "encryption": {
            "description": "Set to true to enable https. Default: false",
            "type": "boolean"
        },
        "public_cert": {
            "description": "Path of the public certificate file .crt",
            "type": "string"
        },
        "private_key": {
            "description": "Path of the SSL private key file .key",
            "type": "string"
        },
        "ca_certificate": {
            "description": "Path of the SSL CA file .crt",
            "type": "string"
        },
        "ca_name": {
            "description": "CA Common Name (CN). This is used to verify the server certificate. Don't use it if unsure.",
            "type": "string"
        },
        "reversed_grpc_streaming": {
            "description": "Set to true to make Engine connect to the agent. Requires the agent to be configured as a server. Default: false",
            "type": "boolean"
        },
        "log_level": {
            "description": "Minimal severity level to log, may be critical, error, info, debug, trace",
            "type": "string",
            "pattern": "critical|error|info|debug|trace"
        },
        "log_type": {
            "description": "Define whether logs must be sent to the standard output (stdout) or to a log file (file). A path will be required in log_file field if 'file' is chosen. Default: stdout",
            "type": "string",
            "pattern": "stdout|file"
        },
        "log_file": {
            "description": "Path of the log file. Mandatory if log_type is 'file'",
            "type": "string",
            "minLength": 5
        },
        "log_max_file_size": {
            "description:": "Maximum size (in megabytes) of the log file before it is rotated. To be valid, log_max_files must be also be supplied",
            "type": "integer",
            "min": 1
        },
        "log_max_files": {
            "description:": "Maximum number of log files to keep. Excess files will be deleted. To be valid, log_max_file_size must be also be supplied",
            "type": "integer",
            "min": 1
        },
        "second_max_reconnect_backoff": {
            "description": "Maximum time between subsequent connection attempts, in seconds. Default: 60s",
            "type": "integer",
            "min": 0
        },
        "max_message_length": {
            "description": "maximum protobuf message length in Mo",
            "type": "integer",
            "minimum": 4
        },
        "token":{
            "description": "key for token",
            "type": "string"
        },
        "engine_context_path":{
            "description": "path of file that contains key and salt for decrypt commands, same as /etc/centreon-engine/engine-context.json on poller host",
            "type": "string"
        }
    },
    "required": [
        "endpoint"
    ],
    "type": "object"
}

)");

std::unique_ptr<config> config::_global_conf;

config::config(const std::string& path) {
  static common::json_validator validator(config_schema);
  rapidjson::Document file_content_d;
  try {
    file_content_d = rapidjson_helper::read_from_file(path);
  } catch (const std::exception& e) {
    SPDLOG_ERROR("incorrect json file{}: {} ", path, e.what());
    throw;
  }

  common::rapidjson_helper json_config(file_content_d);

  try {
    json_config.validate(validator);
  } catch (const std::exception& e) {
    SPDLOG_ERROR("forbidden values in agent config: {}", e.what());
    throw;
  }

  _endpoint = json_config.get_string("endpoint");

  // pattern schema doesn't work so we do it ourselves
  if (!RE2::FullMatch(_endpoint, "[\\w\\.\\-:]+:\\w+")) {
    throw exceptions::msg_fmt(
        "bad format for endpoint {}, it must match the regex: "
        "[\\w\\.\\-:]+:\\w+",
        _endpoint);
  }
  _log_level =
      spdlog::level::from_str(json_config.get_string("log_level", "info"));
  _log_type = !strcmp(json_config.get_string("log_type", "stdout"), "file")
                  ? to_file
                  : to_stdout;
  _log_file = json_config.get_string("log_file", "");
  _log_max_file_size = json_config.get_unsigned("log_max_file_size", 0);
  _log_max_files = json_config.get_unsigned("log_max_files", 0);
  _encryption = json_config.get_bool("encryption", false);
  _public_cert_file = json_config.get_string("public_cert", "");
  _private_key_file = json_config.get_string("private_key", "");
  _ca_certificate_file = json_config.get_string("ca_certificate", "");
  _ca_name = json_config.get_string("ca_name", "");
  _host = json_config.get_string("host", "");
  if (_host.empty()) {
    _host = boost::asio::ip::host_name();
  }
  _reverse_connection = json_config.get_bool("reversed_grpc_streaming", false);
  _second_max_reconnect_backoff =
      json_config.get_unsigned("second_max_reconnect_backoff", 60);
  _max_message_length =
      json_config.get_unsigned("max_message_length", 4) * 1024 * 1024;

  if (_reverse_connection) {
    if (json_config.has_member("token")) {
      _trusted_tokens.insert(json_config.get_string("token"));
    }
  } else {
    if (json_config.has_member("token")) {
      _token = json_config.get_string("token");
    }
  }
  _engine_context_path = json_config.get_string("engine_context_path", "");
}
