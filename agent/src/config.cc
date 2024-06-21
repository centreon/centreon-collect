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
#include <re2/re2.h>

#include "com/centreon/common/rapidjson_helper.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "config.hh"

using namespace com::centreon::agent;
using com::centreon::common::rapidjson_helper;

static constexpr std::string_view _config_schema(R"(
{
    "$schema": "http://json-schema.org/draft-04/schema#",
    "title": "agent config",
    "properties": {
        "host": {
            "description": "IP or dns name, if not given, hostname will be used",
            "type": "string",
            "minLength": 5
        },
        "endpoint": {
            "description": "endpoint of poller where agent have to connect or listen endpoint in case of reverse_connection",
            "type": "string",
            "pattern": "[\\w\\.:]+:\\w+"
        },
        "encryption": {
            "description": "true if https, default: false",
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
        "reverse_connection": {
            "description": "if true, centagent is a server and centengine will connect to, default:false",
            "type": "boolean"
        },
        "log_level": {
            "description": "log level, may be critical, error, info, debug, trace",
            "type": "string",
            "pattern": "critical|error|info|debug|trace"
        },
        "log_type": {
            "description": "stdout or file if log to a file (path must be given by log_file), default: stdout",
            "type": "string",
            "pattern": "stdout|file"
        },
        "log_file": {
            "description": "path of the log file",
            "type": "string",
            "minLength": 5
        },
        "log_max_file_size": {
            "description:": "max size in Mo of the log file before rotate, to be valid log_max_files must be also be specified",
            "type": "integer",
            "min": 1
        },
        "log_max_files": {
            "description:": "max of log files before remove, to be valid log_max_file_size must be also be specified",
            "type": "integer",
            "min": 1
        }
    },
    "required": [
        "endpoint"
    ],
    "type": "object"
}

)");

config::config(const std::string& path) {
  static common::json_validator validator(_config_schema);
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
  if (!RE2::FullMatch(_endpoint, "[\\w\\.:]+:\\w+")) {
    throw exceptions::msg_fmt(
        "bad format for endpoint {}, it must match to the regex: "
        "[\\w\\.:]+:\\w+",
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
  _certificate_file = json_config.get_string("certificate_file", "");
  _private_key_file = json_config.get_string("private_key_file", "");
  _ca_certificate_file = json_config.get_string("ca_certificate_file", "");
  _ca_name = json_config.get_string("ca_name", "");
  _host = json_config.get_string("host", "");
  if (_host.empty()) {
    _host = boost::asio::ip::host_name();
  }
  _reverse_connection = json_config.get_bool("reverse_connection", false);
}