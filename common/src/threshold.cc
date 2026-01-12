/**
 * Copyright 2011-2013,2017-2021 Centreon
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

#include "threshold.hh"
#include <cerrno>
#include <charconv>
#include <cstdlib>

using namespace com::centreon::common;

static inline bool to_double(std::string_view s, double& out) {
#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
  auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), out);

  if (ec == std::errc::invalid_argument)
    return false;  // not a number

  if (ec == std::errc::result_out_of_range)
    return false;  // overflow / underflow

  if (ptr != s.data() + s.size())
    return false;

  return true;
#else
  const char* begin = s.data();
  const char* end = s.data() + s.size();

  char* parsed = nullptr;
  errno = 0;  // macro from <cerrno>

  out = std::strtod(begin, &parsed);

  if (errno != 0)
    return false;

  if (parsed != end)
    return false;

  return true;
#endif
}

/**
 *  Default constructor.
 */
threshold::threshold(std::string const& str)
    : _low(NAN),
      _high(NAN),
      _default_low(NAN),
      _default_high(NAN),
      _inclusive(false),
      _multiplier(1.0),
      _error_code(0),
      _disabled(false) {
  extract_range(str);
}

/**
 * Threshold evaluation based on Nagios syntax:
 * - Without '@', alert when the value is outside [low, high].
 * - With '@', alert when the value is inside [low, high].
 * Boundaries are treated as inclusive.
 */
bool threshold::is_triggered(double value) const {
  // If the threshold could not be parsed, do not trigger.
  if (std::isnan(_low) && std::isnan(_high))
    return false;

  const bool _low_trigger = std::isnan(_low) || value >= _low;
  const bool _high_trigger = std::isnan(_high) || value <= _high;
  const bool inside_range = _low_trigger && _high_trigger;

  return _inclusive ? inside_range : !inside_range;
}

void threshold::extract_range(std::string str) {
  bool valid = true;
  _high = NAN;
  _low = NAN;

  if (str.empty()) {
    _disabled = true;
    _error_code = 0;
    return;
  }

  // Exclusive range ?
  if (str[0] == '@') {
    _inclusive = true;
    str = str.substr(1);
  } else
    _inclusive = false;

  // low threshold value.
  size_t sep_pos = str.find(':');

  // Check for valid separator position
  if (str.find(':', sep_pos + 1) != std::string::npos) {
    valid = false;
  }

  if (sep_pos != std::string::npos) {
    if (!to_double(str.substr(0, sep_pos), _low))
      valid = false;
    if (sep_pos + 1 < str.size())
      if (!to_double(str.substr(sep_pos + 1), _high))
        valid = false;
  } else {
    // Single value : low threshold
    if (!to_double(str, _high)) {
      valid = false;
    }
  }

  _error_code = valid ? 0 : 1;
}

/**
 * @brief Populate the warning threshold details of a perfdata entry from this
 * threshold.
 *
 *
 * @param pref The perfdata instance to update with warning bounds and mode.
 * @param multiplier Scaling factor applied to the low and high
 * bounds.default 1.0 (used for percentages)
 *
 * @note If the threshold is disabled, no changes are applied. Unset bounds
 * (NaN) are ignored.
 */
void threshold::set_pref_details_w(common::perfdata& pref,
                                   double multiplier) const {
  if (!is_disabled()) {
    if (!std::isnan(_low)) {
      pref.warning_low(_low * multiplier);
    } else if (!std::isnan(_default_low)) {
      pref.warning_low(_default_low);
    }
    if (!std::isnan(_high)) {
      pref.warning(_high * multiplier);
    } else if (!std::isnan(_default_high)) {
      pref.warning(_default_high);
    }
    pref.warning_mode(_inclusive);
  }
}

/**
 * @brief Populate the critical threshold details of a perfdata entry from this
 * threshold.
 *
 *
 * @param pref The perfdata instance to update with critical bounds and mode.
 * @param multiplier Scaling factor applied to the low and high
 * bounds.default 1.0 (used for percentages)
 *
 * @note If the threshold is disabled, no changes are applied. Unset bounds
 * (NaN) are ignored.
 */
void threshold::set_pref_details_c(common::perfdata& pref,
                                   double multiplier) const {
  if (!is_disabled()) {
    if (!std::isnan(_low)) {
      pref.critical_low(_low * multiplier);
    } else if (!std::isnan(_default_low)) {
      pref.critical_low(_default_low);
    }
    if (!std::isnan(_high)) {
      pref.critical(_high * multiplier);
    } else if (!std::isnan(_default_high)) {
      pref.critical(_default_high);
    }
    pref.critical_mode(_inclusive);
  }
}