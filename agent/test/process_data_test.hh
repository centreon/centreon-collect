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

#ifndef CHECK_EVENT_LOG_DATA_TEST_HH
#define CHECK_EVENT_LOG_DATA_TEST_HH

#include "process/process_data.hh"

class mock_process_data : public com::centreon::agent::process::process_data {
 public:
  mock_process_data& operator<<(DWORD pid) {
    _pid = pid;
    return *this;
  }

  mock_process_data& operator<<(e_state state) {
    _state = state;
    return *this;
  }

  mock_process_data& operator<<(const std::string& file_name) {
    _file_name = file_name;
    size_t last_sep = _file_name.find_last_of("\\/");
    if (last_sep != std::string::npos) {
      _exe = _file_name.substr(last_sep + 1);
    } else {
      _exe = _file_name;
    }
    return *this;
  }

  struct times {
    std::chrono::file_clock::time_point creation_time;
    std::chrono::file_clock::time_point exit_time;
    std::chrono::file_clock::duration kernel_time;
    std::chrono::file_clock::duration user_time;
  };

  mock_process_data& operator<<(const times& t) {
    _creation_time = t.creation_time;
    _exit_time = t.exit_time;
    _kernel_time = t.kernel_time;
    _user_time = t.user_time;
    return *this;
  }

  struct handle_count {
    unsigned gdi_handle_count;
    unsigned user_handle_count;
  };

  mock_process_data& operator<<(const handle_count& hc) {
    _gdi_handle_count = hc.gdi_handle_count;
    _user_handle_count = hc.user_handle_count;
    return *this;
  }

  mock_process_data& operator<<(const PROCESS_MEMORY_COUNTERS_EX2& mc) {
    _memory_counters = mc;
    return *this;
  }

  template <typename... Args>
  mock_process_data(Args&&... args) {
    (*this << ... << args);
  }
};

#endif