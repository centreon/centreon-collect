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
  unsigned _log_max_file_size;
  unsigned _log_max_files;

  common::grpc::grpc_config::e_security_mode _security_mode;
  std::string _public_cert_file;
  std::string _private_key_file;
  std::string _ca_certificate_file;
  std::string _ca_name;
  std::string _ca_fingerprint;
  std::string _host;
  std::string _host_template;
  bool _reverse_connection;
  unsigned _second_max_reconnect_backoff;
  unsigned _max_message_length;
  std::string _token;
  std::string _path_to_custom_checks;

  std::shared_ptr<const absl::flat_hash_set<std::string>> _trusted_tokens;
  absl::flat_hash_map<std::string, std::string> _custom_checks;
  static std::unique_ptr<config> _global_conf;

 public:
  static const config& load(const std::string& path) {
    _global_conf = std::make_unique<config>(path);
    _global_conf->read_custom_checks();
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

  /**
   * @brief unlike instance(), it can be called when no configuration has been
   * loaded (some UT)
   *
   * @return const config* nullptr if no configuration loaded
   */
  static const config* instance_ptr() { return _global_conf.get(); }

  /**
   * @brief re-read the custom checks file and refresh commands of the global
   * configuration
   *
   * @return true if commands have been refreshed
   */
  static bool reload_custom_checks() {
    return _global_conf && _global_conf->read_custom_checks();
  }

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
  unsigned get_log_max_file_size() const { return _log_max_file_size; }
  unsigned get_log_max_files() const { return _log_max_files; }

  common::grpc::grpc_config::e_security_mode get_security_mode() const {
    return _security_mode;
  }
  bool use_encryption() const {
    return _security_mode != common::grpc::grpc_config::NONE;
  }
  const std::string& get_public_cert_file() const { return _public_cert_file; }
  const std::string& get_private_key_file() const { return _private_key_file; }
  const std::string& get_ca_certificate_file() const {
    return _ca_certificate_file;
  }
  const std::string& get_ca_name() const { return _ca_name; }
  const std::string& get_ca_fingerprint() const { return _ca_fingerprint; }
  const std::string& get_host() const { return _host; }
  const std::string& get_host_template() const { return _host_template; }
  bool use_reverse_connection() const { return _reverse_connection; }
  unsigned get_second_max_reconnect_backoff() const {
    return _second_max_reconnect_backoff;
  }
  unsigned get_max_message_length() const { return _max_message_length; }

  const std::string& get_token() const { return _token; }
  const std::shared_ptr<const absl::flat_hash_set<std::string>>&
  get_trusted_tokens() const {
    return _trusted_tokens;
  }

  const std::string& get_path_to_custom_checks() const {
    return _path_to_custom_checks;
  }

  const absl::flat_hash_map<std::string, std::string>& get_custom_checks()
      const {
    return _custom_checks;
  }

  /**
   * @brief read the custom checks file and replace commands on success
   * If the file can't be read or is malformed, the previous commands are kept
   *
   * @return true if commands have been refreshed
   */
  bool read_custom_checks() {
    if (_path_to_custom_checks.empty()) {
      return false;
    }
    // lambda for trimming spaces
    auto trimming = [](std::string& s) {
      s = absl::StripLeadingAsciiWhitespace(s);
      s = absl::StripTrailingAsciiWhitespace(s);
    };

    try {
      std::ifstream f(_path_to_custom_checks);
      if (!f) {
        throw exceptions::msg_fmt("could not open file {}",
                                  _path_to_custom_checks);
      }
      absl::flat_hash_map<std::string, std::string> read_checks;
      std::string line;
      unsigned line_number = 0;
      while (std::getline(f, line)) {
        ++line_number;
        trimming(line);
        // skip comments, empty lines and section headers like [custom_checks]
        if (line.empty() || line[0] == ';' || line[0] == '[') {
          continue;
        }
        auto pos = line.find('=');
        if (pos == std::string::npos) {
          throw exceptions::msg_fmt(
              "line {} is not in the <name>=<command> format", line_number);
        }
        std::string name = line.substr(0, pos);
        std::string path = line.substr(pos + 1);
        trimming(name);
        trimming(path);
        if (name.empty() || path.empty()) {
          throw exceptions::msg_fmt("line {} has an empty name or command",
                                    line_number);
        }
        read_checks.emplace(std::move(name), std::move(path));
      }
      if (f.bad()) {
        throw exceptions::msg_fmt("error while reading file {}",
                                  _path_to_custom_checks);
      }
      _custom_checks = std::move(read_checks);
      for (const auto& [name, path] : _custom_checks) {
        SPDLOG_INFO("custom check loaded: name: {}, path: {}", name, path);
      }
      return true;
    } catch (const std::exception& e) {
      SPDLOG_ERROR(
          "could not read custom checks from file {}: the error: {} => "
          "previous custom checks are kept",
          _path_to_custom_checks, e.what());
      return false;
    }
  }
};
};  // namespace com::centreon::agent

#endif
