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
#include <fmt/format.h>
#include <string>

namespace com::centreon::common::log_v3 {
class config {
  std::string _dirname;
  std::string _filename;
  std::size_t _max_size;
  uint32_t _flush_period;
  bool _log_pid;
  bool _log_source;
  /* logger name => level */
  absl::flat_hash_map<std::string, std::string> _loggers;

 public:
  config(const std::string& dirname,
         const std::string& filename,
         std::size_t max_size,
         uint32_t flush_period,
         bool log_pid,
         bool log_source)
      : _dirname{dirname},
        _filename{filename},
        _max_size{max_size},
        _flush_period{flush_period},
        _log_pid{log_pid},
        _log_source{log_source} {}
  config(const config& other)
      : _dirname{other._dirname},
        _filename{other._filename},
        _max_size{other._max_size},
        _flush_period{other._flush_period},
        _log_pid{other._log_pid},
        _log_source{other._log_source} {}
  std::string log_path() const {
    return fmt::format("{}/{}", _dirname, _filename);
  }
  size_t max_size() const { return _max_size; }
  uint32_t flush_period() const { return _flush_period; }
  void set_flush_period(uint32_t flush_period) { _flush_period = flush_period; }
  bool log_pid() const { return _log_pid; }
  void set_log_pid(bool log_pid) { _log_pid = log_pid; }
  bool log_source() const { return _log_source; }
  void set_log_source(bool log_source) { _log_source = log_source; }
  const std::string& dirname() const { return _dirname; }
  void set_dirname(const std::string& dirname) { _dirname = dirname; }
  void set_level(const std::string& name, const std::string& level) {
    _loggers[name] = level;
  }
  absl::flat_hash_map<std::string, std::string> loggers() { return _loggers; }
  absl::flat_hash_map<std::string, std::string> loggers() const {
    return _loggers;
  }
  const std::string& filename() const { return _filename; }
  void set_filename(const std::string& filename) { _filename = filename; }
  void set_max_size(const std::size_t max_size) { _max_size = max_size; }
};
}  // namespace com::centreon::common::log_v3
#endif
