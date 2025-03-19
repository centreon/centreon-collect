/**
 * Copyright 2024 Centreon
 * Licensed under the Apache License, Version 2.0(the "License");
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

#ifndef CENTREON_AGENT_CONFIG_HH
#define CENTREON_AGENT_CONFIG_HH

#include "com/centreon/common/grpc/grpc_config.hh"

namespace com::centreon::agent {

class config {
 public:
  enum log_type { to_stdout, to_file, to_event_log };

  static const std::string_view config_schema;

 private:
  std::string _endpoint;
  spdlog::level::level_enum _log_level;
  log_type _log_type;
  std::string _log_file;
  unsigned _log_files_max_size;
  unsigned _log_files_max_number;

  bool _encryption;
  std::string _public_cert_file;
  std::string _private_key_file;
  std::string _ca_certificate_file;
  std::string _ca_name;
  std::string _host;
  bool _reverse_connection;
  unsigned _second_max_reconnect_backoff;
  unsigned _max_message_length;
  std::string _token;

  static std::unique_ptr<config> _global_conf;

 public:
  static const config& load(const std::string& path) {
    _global_conf = std::make_unique<config>(path);
    return *_global_conf;
  }

  /**
   * @brief used only for UT
   *
   * @param reverse_connection
   * @return const config&
   */
  static const config& load(bool reverse_connection) {
    _global_conf = std::make_unique<config>(reverse_connection);
    return *_global_conf;
  }

  static const config& instance() { return *_global_conf; }

  config(const std::string& path);

  /**
   * @brief used only for UT
   *
   * @param reverse_connection
   */
  config(bool reverse_connection) : _reverse_connection(reverse_connection) {}

  const std::string& get_endpoint() const { return _endpoint; }
  spdlog::level::level_enum get_log_level() const { return _log_level; };
  log_type get_log_type() const { return _log_type; }
  const std::string& get_log_file() const { return _log_file; }
  unsigned get_log_files_max_size() const { return _log_files_max_size; }
  unsigned get_log_files_max_number() const { return _log_files_max_number; }

  bool use_encryption() const { return _encryption; }
  const std::string& get_public_cert_file() const { return _public_cert_file; }
  const std::string& get_private_key_file() const { return _private_key_file; }
  const std::string& get_ca_certificate_file() const {
    return _ca_certificate_file;
  }
  const std::string& get_ca_name() const { return _ca_name; }
  const std::string& get_host() const { return _host; }
  bool use_reverse_connection() const { return _reverse_connection; }
  unsigned get_second_max_reconnect_backoff() const {
    return _second_max_reconnect_backoff;
  }
  unsigned get_max_message_length() const { return _max_message_length; }
  const std::string& get_token() const { return _token; }
};
};  // namespace com::centreon::agent

#endif
