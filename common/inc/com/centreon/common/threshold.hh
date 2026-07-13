/**
 * Copyright 2026 Centreon
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

#ifndef CENTREON_COMMON_THRESHOLD_HH
#define CENTREON_COMMON_THRESHOLD_HH

#include <string_view>
#include "perfdata.hh"

namespace com::centreon::common {
// Threshold range [low, high] following Nagios threshold syntax.
// 'inclusive' mirrors the '@' prefix: true means we alert when the value is
// inside [low, high], false means we alert when it is outside.
class threshold {
 private:
  double _low, _high;
  double _default_low, _default_high;
  bool _inclusive;  // true -> alert when inside range (@), false -> alert when
                    // outside
  double _multiplier{1.0};
  int _error_code;

  bool _disabled;

 public:
  threshold()
      : _low(NAN),
        _high(NAN),
        _default_low(NAN),
        _default_high(NAN),
        _inclusive(false),
        _multiplier(1.0),
        _error_code(0),
        _disabled(false) {}

  threshold(const std::string& str);

  ~threshold() noexcept = default;

  void extract_range(std::string_view str);

  double get_low() const { return _low; }
  void set_low(double l) { _low = l; }

  double get_high() const { return _high; }
  void set_high(double h) { _high = h; }

  bool is_inclusive() const { return _inclusive; }
  void set_inclusive(bool inc) { _inclusive = inc; }

  void set_default_low(double l) { _default_low = l; }
  void set_default_high(double h) { _default_high = h; }

  // Returns true if the threshold must trigger for the provided value.
  bool is_triggered(double value) const;

  void enable() { _disabled = false; }

  void disable() {
    _disabled = true;
    _low = NAN;
    _high = NAN;
  }
  bool is_disabled() const { return _disabled; }

  int get_error() const { return _error_code; }

  bool is_valid() const { return _error_code == 0; }

  void unit_multiplier(double mult) {
    _multiplier = mult;

    if (!std::isnan(_low)) {
      _low *= mult;
    }
    if (!std::isnan(_high)) {
      _high *= mult;
    }
  }

  double unit_multiplier() const { return _multiplier; }

  void set_pref_details_w(common::perfdata& pref,
                          double multiplier = 1.0) const;

  void set_pref_details_c(common::perfdata& pref,
                          double multiplier = 1.0) const;
};

}  // namespace com::centreon::common

#endif
