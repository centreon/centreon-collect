/**
 * Copyright 2025 Centreon
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

#ifndef CENTREON_AGENT_CHECK_PROCESS_DATA_HH
#define CENTREON_AGENT_CHECK_PROCESS_DATA_HH

#include <psapi.h>

#include "filter.hh"

namespace com::centreon::agent::process {
/**
 * @brief in order to save CPU, this mask is used to extract only mandatory
 * process datas
 *
 */
enum process_field : unsigned {
  exe_filename = 1,
  times = 2,
  handle = 4,
  memory = 8
};

/**
 * @brief process_data is used to store process information
 *
 */
class process_data : public testable {
 public:
  enum e_state : unsigned { started, unreadable, hung };

 protected:
  DWORD _pid;
  e_state _state;
  std::string _file_name;
  std::string _exe;

  std::chrono::file_clock::time_point _creation_time;
  std::chrono::file_clock::time_point _exit_time;
  std::chrono::file_clock::duration _kernel_time;
  std::chrono::file_clock::duration _user_time;

  unsigned _gdi_handle_count;
  unsigned _user_handle_count;

  PROCESS_MEMORY_COUNTERS_EX2 _memory_counters;

  /**
   * used only for tests
   */
  process_data() {}

 public:
  process_data(const process_data&) = delete;
  process_data& operator=(const process_data&) = delete;
  process_data(process_data&&) = default;

  process_data(DWORD pid,
               unsigned fields,
               const std::shared_ptr<spdlog::logger>& logger);

  const DWORD& get_pid() const { return _pid; }
  e_state get_state() const { return _state; }
  const std::string_view& get_str_state() const;
  void set_hung() { _state = e_state::hung; }

  const std::string& get_file_name() const { return _file_name; }
  const std::string& get_exe() const { return _exe; }

  const std::chrono::file_clock::time_point& get_creation_time() const {
    return _creation_time;
  }

  std::string get_creation_time_str() const;

  const std::chrono::file_clock::time_point& get_exit_time() const {
    return _exit_time;
  }
  const std::chrono::file_clock::duration& get_kernel_time() const {
    return _kernel_time;
  }
  const std::chrono::file_clock::duration& get_user_time() const {
    return _user_time;
  }

  unsigned get_percent_kernel_time() const;
  unsigned get_percent_user_time() const;
  unsigned get_percent_cpu_time() const;

  const unsigned& get_gdi_handle_count() const { return _gdi_handle_count; }
  const unsigned& get_user_handle_count() const { return _user_handle_count; }

  const PROCESS_MEMORY_COUNTERS_EX2& get_memory_counters() const {
    return _memory_counters;
  }
};

}  // namespace com::centreon::agent::process

#endif
