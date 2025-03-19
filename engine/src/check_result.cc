/**
 * Copyright 2011 - 2024 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */

#include "com/centreon/engine/check_result.hh"

#include "com/centreon/engine/checks/checker.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"

using namespace com::centreon::engine;

check_result::check_result()
    : _object_check_type{check_source::service_check},
      _notifier{nullptr},
      _check_type(checkable::check_type::check_passive),
      _check_options{0},
      _reschedule_check{false},
      _latency{0},
      _start_time{0, 0},
      _finish_time{0, 0},
      _early_timeout{false},
      _exited_ok{false},
      _return_code{0} {}

check_result::check_result(enum check_source object_check_type,
                           notifier* notifier,
                           enum checkable::check_type check_type,
                           unsigned check_options,
                           bool reschedule_check,
                           double latency,
                           struct timeval start_time,
                           struct timeval finish_time,
                           bool early_timeout,
                           bool exited_ok,
                           int return_code,
                           std::string output)
    : _object_check_type{object_check_type},
      _notifier{notifier},
      _check_type(check_type),
      _check_options{check_options},
      _reschedule_check{reschedule_check},
      _latency{latency},
      _start_time(start_time),
      _finish_time(finish_time),
      _early_timeout{early_timeout},
      _exited_ok{exited_ok},
      _return_code{return_code},
      _output{std::move(output)} {}

void check_result::set_object_check_type(enum check_source object_check_type) {
  _object_check_type = object_check_type;
}

void check_result::set_notifier(notifier* notifier) {
  _notifier = notifier;
}

void check_result::set_finish_time(struct timeval finish_time) {
  _finish_time = finish_time;
}

void check_result::set_start_time(struct timeval start_time) {
  _start_time = start_time;
}

void check_result::set_return_code(int return_code) {
  _return_code = return_code;
}

void check_result::set_early_timeout(bool early_timeout) {
  _early_timeout = early_timeout;
}

/**
 * @brief Set the check output to the check_result. A boolean is also here
 * to check or not if the string is legal UTF-8. If it may be non UTF-8,
 * we have to convert it and so set check_encoding to true.
 *
 * @param output The string to set as output
 * @param check_encoding A boolean telling if the string has to be checked.
 */
void check_result::set_output(std::string const& output) {
  _output = output;
}

void check_result::set_exited_ok(bool exited_ok) {
  _exited_ok = exited_ok;
}

void check_result::set_reschedule_check(bool reschedule_check) {
  _reschedule_check = reschedule_check;
}

void check_result::set_check_type(enum checkable::check_type check_type) {
  _check_type = check_type;
}

void check_result::set_latency(double latency) {
  _latency = latency;
}

void check_result::set_check_options(unsigned check_options) {
  _check_options = check_options;
}

namespace com::centreon::engine {

std::ostream& operator<<(std::ostream& stream, const check_result& res) {
  stream << " start_time=" << res.get_start_time().tv_sec
         << " finish_time=" << res.get_finish_time().tv_sec
         << " timeout=" << res.get_early_timeout()
         << " ok=" << res.get_exited_ok()
         << " ret_code=" << res.get_return_code()
         << " output:" << res.get_output();
  return stream;
}

}  // namespace com::centreon::engine
