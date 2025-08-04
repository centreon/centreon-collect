/**
 * Copyright 2023 Centreon
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
#ifndef CCC_LOG_V2_CONFIG_HH
#define CCC_LOG_V2_CONFIG_HH
#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include "spdlog/spdlog.h"

#include <filesystem>
#include <string>

namespace com::centreon::common::log_v2 {
class config {
 public:
  enum class logger_type {
    LOGGER_STDOUT = 0,
    LOGGER_FILE = 1,
    LOGGER_SYSLOG = 2,
  };

 private:
  const std::string _name;
  /* This is a little hack to avoid to replace the log file set by centengine */
  bool _only_atomic_changes = false;
  logger_type _log_type;
  std::string _dirname;
  std::string _filename;
  std::size_t _max_size;
  uint32_t _flush_interval;
  bool _log_pid;
  bool _log_source;
  /* logger name => level */
  absl::flat_hash_map<std::string, std::string> _loggers;

  /* custom sinks needed by some loggers */
  std::vector<spdlog::sink_ptr> _custom_sinks;
  /* which loggers need custom sinks */
  absl::flat_hash_set<std::string> _loggers_with_custom_sinks;

 public:
  config(const std::string& name,
         logger_type log_type,
         uint32_t flush_interval,
         bool log_pid,
         bool log_source)
      : _log_type{log_type},
        _flush_interval{flush_interval},
        _log_pid{log_pid},
        _log_source{log_source} {
    std::filesystem::path path{name};
    _dirname = path.parent_path();
    _filename = path.filename();
  }

  config(const config& other)
      : _log_type{other._log_type},
        _dirname{other._dirname},
        _filename{other._filename},
        _max_size{other._max_size},
        _flush_interval{other._flush_interval},
        _log_pid{other._log_pid},
        _log_source{other._log_source} {}
  std::string log_path() const {
    return _dirname.empty() ? _filename
                            : fmt::format("{}/{}", _dirname, _filename);
  }

  logger_type log_type() const { return _log_type; }

  /**
   * @brief Set the file name to write into. The name must be the full path of
   * the file, including the parent directories.
   *
   * @param full_path The file to log into.
   */
  void set_log_path(const std::string& full_path) {
    std::filesystem::path p{full_path};
    _dirname = p.parent_path();
    _filename = p.filename();
  }

  size_t max_size() const { return _max_size; }
  uint32_t flush_interval() const { return _flush_interval; }
  void set_flush_interval(uint32_t flush_interval) {
    _flush_interval = flush_interval;
  }
  bool log_pid() const { return _log_pid; }
  void set_log_pid(bool log_pid) { _log_pid = log_pid; }
  bool log_source() const { return _log_source; }
  void set_log_source(bool log_source) { _log_source = log_source; }
  const std::string& dirname() const { return _dirname; }
  void set_dirname(const std::string& dirname) { _dirname = dirname; }
  void set_level(const std::string& name, const std::string& level) {
    _loggers[name] = level;
  }
  absl::flat_hash_map<std::string, std::string>& loggers() { return _loggers; }
  const absl::flat_hash_map<std::string, std::string>& loggers() const {
    return _loggers;
  }
  const std::string& filename() const { return _filename; }
  void set_filename(const std::string& filename) { _filename = filename; }
  void set_max_size(const std::size_t max_size) { _max_size = max_size; }
  void add_custom_sink(const spdlog::sink_ptr& sink) {
    _custom_sinks.push_back(sink);
  }
  const std::vector<spdlog::sink_ptr>& custom_sinks() const {
    return _custom_sinks;
  }
  void apply_custom_sinks(std::initializer_list<std::string> ilist) {
    _loggers_with_custom_sinks = ilist;
  }
  const absl::flat_hash_set<std::string>& loggers_with_custom_sinks() const {
    return _loggers_with_custom_sinks;
  }
  //  const std::string& name() const { return _name; }
  void allow_only_atomic_changes(bool slave) { _only_atomic_changes = slave; }
  bool only_atomic_changes() const { return _only_atomic_changes; }
};
}  // namespace com::centreon::common::log_v2
#endif
