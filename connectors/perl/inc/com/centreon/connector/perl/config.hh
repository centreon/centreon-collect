/*
** Copyright 2026 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#ifndef CCCP_CONFIG_HH
#define CCCP_CONFIG_HH

namespace com::centreon::connector::perl {

class config {
  unsigned _max_child = 64;
  unsigned _min_free_memory = 500;
  unsigned _max_opened_fd = 0;
  unsigned _child_max_memory_increase_percent = 10;
  unsigned _child_max_fd_increase_percent = 10;
  unsigned _child_max_thread = 10;
  unsigned _child_max_reuse_script = 100;
  unsigned _minute_idle_check_child_ttl = 15;

  std::string _log_file_path;
  spdlog::level::level_enum _log_level = spdlog::level::level_enum::info;
  std::string _code;
  bool _need_to_stop = false;
  std::string _test_file_path;

 public:
  config(int argc, char** argv);

  unsigned max_child() const { return _max_child; }
  /**
   * @brief
   *
   * @return unsigned in Mo
   */
  unsigned min_free_memory() const { return _min_free_memory; }
  unsigned max_opened_fd() const { return _max_opened_fd; }
  unsigned child_max_memory_increase_percent() const {
    return _child_max_memory_increase_percent;
  }
  unsigned child_max_fd_increase_percent() const {
    return _child_max_fd_increase_percent;
  }
  unsigned child_max_thread() const { return _child_max_thread; }
  unsigned child_max_reuse_script() const { return _child_max_reuse_script; }
  const std::string& log_file_path() const { return _log_file_path; }
  spdlog::level::level_enum log_level() const { return _log_level; }
  const std::string& code() const { return _code; }
  bool need_to_stop() const { return _need_to_stop; }
  const std::string& test_file_path() const { return _test_file_path; }
  unsigned minute_idle_check_child_ttl() const {
    return _minute_idle_check_child_ttl;
  }
};

}  // namespace com::centreon::connector::perl

#endif
