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
#include <memory>
#include "absl/container/flat_hash_set.h"
#include "check.hh"
#include "filter.hh"
#include "process/process_data.hh"

using namespace com::centreon::agent::process;
using namespace com::centreon::agent;

container::container(const std::string_view& filter_str,
                     const std::string_view& exclude_filter_str,
                     const std::string_view& warning_filter_str,
                     const std::string_view& critical_filter_str,
                     unsigned needed_output_fields,
                     const std::shared_ptr<spdlog::logger>& logger)
    : _needed_output_fields(needed_output_fields),
      _enumerate_buffer_size(1024),
      _logger(logger) {
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
        flt->set_checker_from_getter(
            [this](const testable&) { return _ok_processes.size(); });
      } else if (flt->get_label() == "warn_count") {
        flt->set_checker_from_getter(
            [this](const testable&) { return _warning_processes.size(); });
      } else if (flt->get_label() == "crit_count") {
        flt->set_checker_from_getter(
            [this](const testable&) { return _critical_processes.size(); });
      }
      // only counts are taken into account on process evaluation so by not
      // provide a checker for others, we disable them
    }
  };

  if (!warning_filter_str.empty()) {
    _warning_filter =
        std::make_unique<process_filter>(warning_filter_str, logger);
    _needed_output_fields |= _warning_filter->get_fields_mask();
    _container_warning_filter = std::make_unique<filters::filter_combinator>();
    filter::create_filter(warning_filter_str, logger,
                          _container_warning_filter.get());
    _container_warning_filter->apply_checker(container_checker_builder);
  }
  if (!critical_filter_str.empty()) {
    _critical_filter =
        std::make_unique<process_filter>(critical_filter_str, logger);
    _needed_output_fields |= _critical_filter->get_fields_mask();
    _container_critical_filter = std::make_unique<filters::filter_combinator>();
    filter::create_filter(critical_filter_str, logger,
                          _container_critical_filter.get());
    _container_critical_filter->apply_checker(container_checker_builder);
  }

  _enumerate_buffer = std::make_unique<DWORD[]>(_enumerate_buffer_size);
}

using pid_set = absl::flat_hash_set<DWORD>;

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

void container::refresh() {
  _ok_processes.clear();
  _warning_processes.clear();
  _critical_processes.clear();
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
  pid_set hungs;
  EnumWindows(&enum_hung_window_proc, reinterpret_cast<LPARAM>(&hungs));

  const DWORD* end = _enumerate_buffer.get() + used / sizeof(DWORD);
  for (DWORD* pid = _enumerate_buffer.get(); pid != end; ++pid) {
    process_data proc(*pid, _needed_output_fields, _logger);
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

e_status container::check_container() const {
  if (_container_critical_filter && _container_critical_filter->check(*this)) {
    return e_status::critical;
  } else if (_container_warning_filter &&
             _container_warning_filter->check(*this)) {
    return e_status::warning;
  }
  return e_status::ok;
}