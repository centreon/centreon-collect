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

#include "process/process_data.hh"
#include <string>
#include "windows_util.hh"

using namespace com::centreon::agent;
using namespace com::centreon::agent::process;

namespace com::centreon::agent::process::detail {
struct handle_close {
  void operator()(HANDLE* h) { CloseHandle(*h); }
};
}  // namespace com::centreon::agent::process::detail
process_data::process_data(DWORD pid,
                           unsigned fields,
                           const std::shared_ptr<spdlog::logger>& logger)
    : _pid(pid) {
  HANDLE h_process =
      OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);

  if (h_process == nullptr) {
    _state = e_state::unreadable;
    return;
  }
  _state = e_state::started;

  std::unique_ptr<HANDLE, detail::handle_close> h_process_ptr(&h_process);

  char buff[MAX_PATH + 1];
  buff[MAX_PATH] = '\0';
  if (fields & process_field::exe_filename) {
    if (GetModuleFileNameExA(h_process, nullptr, buff, MAX_PATH) == 0) {
      SPDLOG_LOGGER_ERROR(logger, "GetModuleFileNameEx failed of process {}",
                          pid);
    } else {
      _file_name = buff;
      size_t last_sep = _file_name.find_last_of("\\/");
      if (last_sep != std::string::npos) {
        _exe = _file_name.substr(last_sep + 1);
      } else {
        _exe = _file_name;
      }
    }
  }

  if (fields & process_field::times) {
    FILETIME creation_time, exit_time, kernel_time, user_time;
    if (GetProcessTimes(h_process, &creation_time, &exit_time, &kernel_time,
                        &user_time) == 0) {
      SPDLOG_LOGGER_ERROR(logger, "GetProcessTimes failed of process {}", pid);
    } else {
      _creation_time = convert_filetime_to_tp(creation_time);
      _exit_time = convert_filetime_to_tp(exit_time);
      _kernel_time = convert_filetime_to_duration(kernel_time);
      _user_time = convert_filetime_to_duration(user_time);
    }
  } else {
    _creation_time = _exit_time = {};
    _kernel_time = _user_time = {};
  }

  if (fields & process_field::handle) {
    _gdi_handle_count = GetGuiResources(h_process, GR_GDIOBJECTS);
    _user_handle_count = GetGuiResources(h_process, GR_USEROBJECTS);
  } else {
    _gdi_handle_count = _user_handle_count = 0;
  }

  if (fields & process_field::memory) {
    _memory_counters.cb = sizeof(_memory_counters);
    if (GetProcessMemoryInfo(
            h_process,
            reinterpret_cast<PPROCESS_MEMORY_COUNTERS>(&_memory_counters),
            sizeof(_memory_counters)) == 0) {
      SPDLOG_LOGGER_ERROR(logger, "GetProcessMemoryInfo failed of process {}",
                          pid);
    }
  } else {
    memset(&_memory_counters, 0, sizeof(_memory_counters));
  }
}
constexpr std::string_view _started = "started";
constexpr std::string_view _unreadable = "unreadable";
constexpr std::string_view _hung = "hung";
constexpr std::string_view _unknown = "_unknown";

const std::string_view& process_data::get_str_state() const {
  switch (_state) {
    case e_state::started:
      return _started;
    case e_state::unreadable:
      return _unreadable;
    case e_state::hung:
      return _hung;
    default:
      return _unknown;
  }
}

std::string process_data::get_creation_time_str() const {
  return std::format("{:%FT%T}", _creation_time);
}

inline unsigned calc_percent_duration(
    const std::chrono::file_clock::time_point& process_start,
    const std::chrono::file_clock::duration& consumed_time) {
  std::chrono::file_clock::duration process_time =
      std::chrono::file_clock::now() - process_start;
  return process_time.count() ? (consumed_time * 100) / process_time : 0;
}

unsigned process_data::get_percent_kernel_time() const {
  return calc_percent_duration(_creation_time, _kernel_time);
}
unsigned process_data::get_percent_user_time() const {
  return calc_percent_duration(_creation_time, _user_time);
}

unsigned process_data::get_percent_cpu_time() const {
  return calc_percent_duration(_creation_time, _kernel_time + _user_time);
}
