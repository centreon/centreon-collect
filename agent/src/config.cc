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
            "description": "name of host configured in centreon, if not given, hostname will be used",
            "type": "string",
            "minLength": 5
        },
        "endpoint": {
            "description": "endpoint of poller where agent has to connect or listening endpoint in case of reverse_connection",
            "type": "string",
            "pattern": "[\\w\\.:]+:\\w+"
        },
        "encryption": {
            "description": "Set to true to enable https. Default: false",
            "type": "boolean"
        },
        "public_cert": {
            "description": "Path of the SSL certificate file .crt",
            "type": "string"
        },
        "private_key": {
            "description": "Path of the SSL private key file .key",
            "type": "string"
        },
        "ca_certificate": {
            "description": "Path of the SSL authority certificate file .crt",
            "type": "string"
        },
        "ca_name": {
            "description": "Name of the SSL certification authority",
            "type": "string"
        },
        "reverse_connection": {
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
        "log_files_max_size": {
            "description:": "Maximum size (in megabytes) of the log file before it will be rotated. To be valid, log_files_max_number must be also be provided",
            "type": "integer",
            "min": 1
        },
        "log_files_max_number": {
            "description:": "Maximum number of log files to keep. Supernumerary files will be deleted. To be valid, log_files_max_size must be also be provided",
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
  _log_files_max_size = json_config.get_unsigned("log_files_max_size", 0);
  _log_files_max_number = json_config.get_unsigned("log_files_max_number", 0);
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