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

#include <windows.h>

#include "config.hh"

using namespace com::centreon::agent;

std::unique_ptr<config> config::_global_conf;

/**
 * @brief Construct a new config::config object
 *
 * @param registry_key registry path as
 * HKEY_LOCAL_MACHINE\SOFTWARE\Centreon\CentreonMonitoringAgent
 */
config::config(const std::string& registry_key) {
  HKEY h_key;
  LSTATUS res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, registry_key.c_str(), 0,
                              KEY_READ, &h_key);
  if (res != ERROR_SUCCESS) {
    if (res == ERROR_FILE_NOT_FOUND) {
      throw exceptions::msg_fmt("{} not found", registry_key);
    } else {
      throw exceptions::msg_fmt("unable to read {}", registry_key);
    }
  }

  char str_buffer[4096];

  auto get_sz_reg_or_default = [&](const char* value_name,
                                   const char* default_value) {
    DWORD size = sizeof(str_buffer);
    LSTATUS result = RegGetValueA(h_key, nullptr, value_name, RRF_RT_REG_SZ,
                                  nullptr, str_buffer, &size);
    return (result == ERROR_SUCCESS) ? str_buffer : default_value;
  };

  auto get_bool = [&](const char* value_name) -> bool {
    int32_t value;
    DWORD size = sizeof(value);
    LSTATUS result = RegGetValueA(h_key, nullptr, value_name, RRF_RT_DWORD,
                                  nullptr, &value, &size);
    return result == ERROR_SUCCESS && value;
  };

  auto get_unsigned = [&](const char* value_name,
                          unsigned default_value = 0) -> uint32_t {
    uint32_t value;
    DWORD size = sizeof(value);
    LSTATUS result = RegGetValueA(h_key, nullptr, value_name, RRF_RT_DWORD,
                                  nullptr, &value, &size);
    return result == ERROR_SUCCESS ? value : default_value;
  };

  _endpoint = get_sz_reg_or_default("endpoint", "");

  // pattern schema doesn't work so we do it ourselves
  if (!RE2::FullMatch(_endpoint, "[\\w\\.\\-:]+:\\w+")) {
    RegCloseKey(h_key);
    throw exceptions::msg_fmt(
        "bad format for endpoint {}, it must match the regex: "
        "[\\w\\.\\-:]+:\\w+",
        _endpoint);
  }
  _log_level =
      spdlog::level::from_str(get_sz_reg_or_default("log_level", "info"));

  const char* log_type = get_sz_reg_or_default("log_type", "event-log");
  if (!strcmp(log_type, "file")) {
    _log_type = to_file;
  } else if (!strcmp(log_type, "stdout")) {
    _log_type = to_stdout;
  } else {
    _log_type = to_event_log;
  }

  _log_file = get_sz_reg_or_default("log_file", "");
  _log_max_file_size = get_unsigned("log_max_file_size");
  _log_max_files = get_unsigned("log_max_files");
  _encryption = get_bool("encryption");
  _public_cert_file = get_sz_reg_or_default("public_cert", "");
  _private_key_file = get_sz_reg_or_default("private_key", "");
  _ca_certificate_file = get_sz_reg_or_default("ca_certificate", "");
  _ca_name = get_sz_reg_or_default("ca_name", "");
  _host = get_sz_reg_or_default("host", "");
  if (_host.empty()) {
    _host = boost::asio::ip::host_name();
  }
  _reverse_connection = get_bool("reversed_grpc_streaming");
  _second_max_reconnect_backoff =
      get_unsigned("second_max_reconnect_backoff", 60);
  _max_message_length = get_unsigned("max_message_length", 4) * 1024 * 1024;

  RegCloseKey(h_key);
}
