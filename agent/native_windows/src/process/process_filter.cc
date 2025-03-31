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

#include "process/process_filter.hh"
#include <chrono>
#include "process/process_data.hh"

using namespace com::centreon::agent;
using namespace com::centreon::agent::process;

/**
 * @brief Construct a new process filter::process filter object
 *
 * @param filter_str
 * @param logger
 */
process_filter::process_filter(const std::string_view filter_str,
                               const std::shared_ptr<spdlog::logger>& logger)
    : _fields_mask(0) {
  try {
    if (!filter::create_filter(filter_str, logger, &_filter)) {
      throw exceptions::msg_fmt("fail to parse process filter: {}", filter_str);
    }

    _filter.apply_checker([this](filter* filt) { _create_checker(filt); });
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(logger, "fail to parse process filter: {}", e.what());
    throw;
  }
}

/**
 * @brief Create a checker for the filter according to field label
 *
 * @param f
 */
void process_filter::_set_label_compare_to_value(
    filters::label_compare_to_value* filt) {
  if (filt->get_label() == "creation") {
    filt->calc_duration();
    filt->change_threshold_to_abs();
    filt->set_checker_from_getter([](const testable& t) {
      return std::chrono::duration_cast<std::chrono::seconds>(
                 std::chrono::file_clock::now() -
                 static_cast<const process_data&>(t).get_creation_time())
          .count();
    });
  } else if (filt->get_label() == "pid") {
    filt->set_checker_from_getter([](const testable& t) {
      return static_cast<const process_data&>(t).get_pid();
    });
  } else if (filt->get_label() == "gdi_handles") {
    _fields_mask |= process_field::handle;
    filt->set_checker_from_getter([](const testable& t) {
      return static_cast<const process_data&>(t).get_gdi_handle_count();
    });
  } else if (filt->get_label() == "handles") {
    _fields_mask |= process_field::handle;
    filt->set_checker_from_getter([](const testable& t) {
      return static_cast<const process_data&>(t).get_gdi_handle_count() +
             static_cast<const process_data&>(t).get_user_handle_count();
    });
  } else if (filt->get_label() == "user_handles") {
    _fields_mask |= process_field::handle;
    filt->set_checker_from_getter([](const testable& t) {
      return static_cast<const process_data&>(t).get_user_handle_count();
    });
  } else if (filt->get_label() == "kernel") {
    _fields_mask |= process_field::times;
    filt->calc_duration();
    filt->set_checker_from_getter([](const testable& t) {
      return std::chrono::duration_cast<std::chrono::seconds>(
                 static_cast<const process_data&>(t).get_kernel_time())
          .count();
    });
  } else if (filt->get_label() == "user") {
    _fields_mask |= process_field::times;
    filt->calc_duration();
    filt->set_checker_from_getter([](const testable& t) {
      return std::chrono::duration_cast<std::chrono::seconds>(
                 static_cast<const process_data&>(t).get_user_time())
          .count();
    });
  } else if (filt->get_label() == "time") {
    _fields_mask |= process_field::times;
    filt->calc_duration();
    filt->set_checker_from_getter([](const testable& t) {
      return std::chrono::duration_cast<std::chrono::seconds>(
                 static_cast<const process_data&>(t).get_kernel_time() +
                 static_cast<const process_data&>(t).get_user_time())
          .count();
    });
  } else if (filt->get_label() == "kernel_percent") {
    _fields_mask |= process_field::times;
    filt->set_checker_from_getter([](const testable& t) {
      return static_cast<const process_data&>(t).get_percent_kernel_time();
    });
  } else if (filt->get_label() == "user_percent") {
    _fields_mask |= process_field::times;
    filt->set_checker_from_getter([](const testable& t) {
      return static_cast<const process_data&>(t).get_percent_user_time();
    });
  } else if (filt->get_label() == "time_percent") {
    _fields_mask |= process_field::times;
    filt->set_checker_from_getter([](const testable& t) {
      return static_cast<const process_data&>(t).get_percent_cpu_time();
    });
  } else if (filt->get_label() == "page_fault") {
    _fields_mask |= process_field::memory;
    filt->set_checker_from_getter([](const testable& t) {
      return static_cast<const process_data&>(t)
          .get_memory_counters()
          .PageFaultCount;
    });
  } else if (filt->get_label() == "pagefile") {
    _fields_mask |= process_field::memory;
    filt->calc_giga_mega_kilo();
    filt->set_checker_from_getter([](const testable& t) {
      return static_cast<const process_data&>(t)
          .get_memory_counters()
          .PagefileUsage;
    });
  } else if (filt->get_label() == "peak_pagefile") {
    _fields_mask |= process_field::memory;
    filt->calc_giga_mega_kilo();
    filt->set_checker_from_getter([](const testable& t) {
      return static_cast<const process_data&>(t)
          .get_memory_counters()
          .PeakPagefileUsage;
    });
  } else if (filt->get_label() == "peak_virtual") {
    _fields_mask |= process_field::memory;
    filt->calc_giga_mega_kilo();
    filt->set_checker_from_getter([](const testable& t) {
      return static_cast<const process_data&>(t)
          .get_memory_counters()
          .PeakPagefileUsage;
    });
  } else if (filt->get_label() == "peak_working_set") {
    _fields_mask |= process_field::memory;
    filt->calc_giga_mega_kilo();
    filt->set_checker_from_getter([](const testable& t) {
      return static_cast<const process_data&>(t)
          .get_memory_counters()
          .PeakWorkingSetSize;
    });
  } else if (filt->get_label() == "working_set") {
    _fields_mask |= process_field::memory;
    filt->calc_giga_mega_kilo();
    filt->set_checker_from_getter([](const testable& t) {
      return static_cast<const process_data&>(t)
          .get_memory_counters()
          .WorkingSetSize;
    });
  } else if (filt->get_label() == "virtual") {
    _fields_mask |= process_field::memory;
    filt->calc_giga_mega_kilo();
    filt->set_checker_from_getter([](const testable& t) {
      return static_cast<const process_data&>(t)
          .get_memory_counters()
          .PrivateUsage;
    });
  } else if (filt->get_label() == "count" || filt->get_label() == "ok_count" ||
             filt->get_label() == "warn_count" ||
             filt->get_label() == "crit_count") {
    // count is not taken into account
    // on process evaluation so by not providing a checker,
    // we disable it
  } else {
    throw exceptions::msg_fmt("label_to_compare: unknown filter label {}",
                              filt->get_label());
  }
}

/**
 * @brief setter used by label_compare_to_string and label_in
 *
 * @tparam filter_type
 * @param filt
 */
template <class filter_type>
void process_filter::_set_getter(filter_type* filt) {
  if (filt->get_label() == "exe") {
    _fields_mask |= process_field::exe_filename;
    filt->set_checker_from_getter([](const testable& t) {
      return static_cast<const process_data&>(t).get_exe();
    });
  } else if (filt->get_label() == "filename") {
    _fields_mask |= process_field::exe_filename;
    filt->set_checker_from_getter([](const testable& t) {
      return static_cast<const process_data&>(t).get_file_name();
    });
  } else if (filt->get_label() == "status" || filt->get_label() == "state") {
    filt->set_checker_from_getter([](const testable& t) {
      return static_cast<const process_data&>(t).get_str_state();
    });
  } else {
    throw exceptions::msg_fmt("unknown filter label {}", filt->get_label());
  }
}

/**
 * @brief initialize filter checkers
 *
 * @param f
 */
void process_filter::_create_checker(filter* f) {
  switch (f->get_type()) {
    case filter::filter_type::label_compare_to_value:
      _set_label_compare_to_value(
          static_cast<filters::label_compare_to_value*>(f));
      break;
    case filter::filter_type::label_compare_to_string:
      _set_getter(static_cast<filters::label_compare_to_string<char>*>(f));
      break;
    case filter::filter_type::label_in:
      _set_getter(static_cast<filters::label_in<char>*>(f));
      break;
    default:
      break;
  }
}

/**
 * @brief check process
 *
 * @param data process_data to check
 * @return true if process matches the filter
 */
bool process_filter::check(const process_data& data) const {
  return _filter.check(data);
}