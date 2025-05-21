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

#include "process/process_container.hh"

using namespace com::centreon::agent::process;
using namespace com::centreon::agent;

/**
 * @brief Construct a new process::container::container object
 *
 * @param filter_str filter to be applied to the processes
 * @param exclude_filter_str filter used to exclude some processes
 * @param warning_filter_str filter to be applied to the processes in order to
 * store them in _warning_processes
 * @param critical_filter_str filter to be applied to the processes in order to
 * store them in _critical_processes
 * @param warning_rules_str rules to be the container to set the status of the
 * container (Ex: warn_count > 1)
 * @param critical_rules_str rules to be the container to set the status of the
 * container (Ex: crit_count > 1)
 * @param needed_output_fields fields that are needed for the output
 * @param logger logger to use
 */
container::container(std::string_view filter_str,
                     std::string_view exclude_filter_str,
                     std::string_view warning_filter_str,
                     std::string_view critical_filter_str,
                     std::string_view warning_rules_str,
                     std::string_view critical_rules_str,
                     unsigned needed_output_fields,
                     const std::shared_ptr<spdlog::logger>& logger)
    : _needed_output_fields(needed_output_fields),
      _enumerate_buffer_size(1024),
      _logger(logger) {
  filter_str = absl::StripLeadingAsciiWhitespace(filter_str);
  exclude_filter_str = absl::StripLeadingAsciiWhitespace(exclude_filter_str);
  warning_filter_str = absl::StripLeadingAsciiWhitespace(warning_filter_str);
  critical_filter_str = absl::StripLeadingAsciiWhitespace(critical_filter_str);
  warning_rules_str = absl::StripLeadingAsciiWhitespace(warning_rules_str);
  critical_rules_str = absl::StripLeadingAsciiWhitespace(critical_rules_str);

  if (!filter_str.empty()) {
    _filter = std::make_unique<process_filter>(filter_str, logger);
    _needed_output_fields |= _filter->get_fields_mask();
  }
  if (!exclude_filter_str.empty()) {
    _exclude_filter =
        std::make_unique<process_filter>(exclude_filter_str, logger);
    _needed_output_fields |= _exclude_filter->get_fields_mask();
  }

  auto container_checker_builder = [this](filter* filt) {
    if (filt->get_type() == filter::filter_type::label_compare_to_value) {
      filters::label_compare_to_value* flt =
          static_cast<filters::label_compare_to_value*>(filt);
      if (flt->get_label() == "count") {
        flt->set_checker_from_getter([this](const testable&) {
          return _ok_processes.size() + _warning_processes.size() +
                 _critical_processes.size();
        });
      } else if (flt->get_label() == "ok_count") {
        flt->set_checker_from_getter([](const testable& t) {
          return static_cast<const container&>(t)._ok_processes.size();
        });
      } else if (flt->get_label() == "warn_count") {
        flt->set_checker_from_getter([](const testable& t) {
          return static_cast<const container&>(t)._warning_processes.size();
        });
      } else if (flt->get_label() == "crit_count") {
        flt->set_checker_from_getter([](const testable& t) {
          return static_cast<const container&>(t)._critical_processes.size();
        });
      }
      // only counts are taken into account
    }
  };

  if (!warning_filter_str.empty()) {
    _warning_filter =
        std::make_unique<process_filter>(warning_filter_str, logger);
    _needed_output_fields |= _warning_filter->get_fields_mask();
  }

  if (!warning_rules_str.empty()) {
    _warning_rules_filter = std::make_unique<filters::filter_combinator>();
    filter::create_filter(warning_rules_str, logger,
                          _warning_rules_filter.get());
    _warning_rules_filter->apply_checker(container_checker_builder);
  }

  if (!critical_filter_str.empty()) {
    _critical_filter =
        std::make_unique<process_filter>(critical_filter_str, logger);
    _needed_output_fields |= _critical_filter->get_fields_mask();
  }

  if (!critical_rules_str.empty()) {
    _critical_rules_filter = std::make_unique<filters::filter_combinator>();
    filter::create_filter(critical_rules_str, logger,
                          _critical_rules_filter.get());
    _critical_rules_filter->apply_checker(container_checker_builder);
  }

  _enumerate_buffer = std::make_unique<DWORD[]>(_enumerate_buffer_size);
}

/**
 * @brief Callback function used by hung windows enumeration
 *
 * @param hwnd handle to the window
 * @param l_param pointer to the pid_set
 * @return TRUE to continue enumeration, FALSE to stop
 */
static BOOL CALLBACK enum_hung_window_proc(HWND hwnd, LPARAM l_param) {
  pid_set* data = reinterpret_cast<pid_set*>(l_param);
  if (!IsWindowVisible(hwnd))
    return TRUE;
  if (GetWindow(hwnd, GW_OWNER) != NULL)
    return TRUE;
  if (IsHungAppWindow(hwnd)) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    data->insert(pid);
  }
  return TRUE;
}

/**
 * @brief Refresh the process container
 *
 * This function will enumerate all processes and store them in the
 * _ok_processes, _warning_processes and _critical_processes vectors
 *
 */
void container::refresh() {
  DWORD used;
  while (1) {
    EnumProcesses(_enumerate_buffer.get(),
                  _enumerate_buffer_size * sizeof(DWORD), &used);
    if (used == _enumerate_buffer_size * sizeof(DWORD)) {
      _enumerate_buffer_size += 1024;
      _enumerate_buffer = std::make_unique<DWORD[]>(_enumerate_buffer_size);
    } else {
      break;
    }
  }

  // get freeze windows
  // we enumerate all active windows and fill hungs with freezed window pids
  pid_set hungs;
  EnumWindows(&enum_hung_window_proc, reinterpret_cast<LPARAM>(&hungs));
  _refresh(_enumerate_buffer.get(), used / sizeof(DWORD), hungs);
}

/**
 * @brief clear and fill process containers according to filters
 *
 * @param pids array of pids
 * @param nb_pids  number of pids
 * @param hungs pids of hungs windows
 */
void container::_refresh(const DWORD* pids,
                         DWORD nb_pids,
                         const pid_set& hungs) {
  _ok_processes.clear();
  _warning_processes.clear();
  _critical_processes.clear();

  const DWORD* end = pids + nb_pids;
  for (const DWORD* pid = pids; pid != end; ++pid) {
    process_data proc = create_process_data(*pid);
    if (hungs.contains(proc.get_pid())) {
      proc.set_hung();
    }
    if (_filter && !_filter->check(proc)) {
      continue;
    }
    if (_exclude_filter && _exclude_filter->check(proc)) {
      continue;
    }
    if (_critical_filter && _critical_filter->check(proc)) {
      _critical_processes.emplace_back(std::move(proc));
    } else if (_warning_filter && _warning_filter->check(proc)) {
      _warning_processes.emplace_back(std::move(proc));
    } else {
      _ok_processes.push_back(*pid);
    }
  }
}

/**
 * @brief Check the container status
 * Once we have refreshed process list, we call this function to check
 * the container status according to filter (ok_count, warn_count..
 * This function will check the container status and return it
 *
 * @return e_status
 */
e_status container::check_container() const {
  if (_critical_rules_filter && _critical_rules_filter->check(*this)) {
    return e_status::critical;
  } else if (_warning_rules_filter && _warning_rules_filter->check(*this)) {
    return e_status::warning;
  }
  return e_status::ok;
}