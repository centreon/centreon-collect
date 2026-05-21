/**
 * Copyright 2024 Centreon
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

#pragma once

#include "com/centreon/common/timeperiod_utils.hh"

namespace com::centreon::common {

/**
 * @brief Check test_time falls in the given timeperiod.
 * Delegates to the full common implementation
 *
 * @param test_time  Time to test (epoch seconds).
 * @param tp         Timeperiod proto message.
 * @return true if test_time is inside the period.
 */
inline bool is_time_in_timeperiod(
    time_t test_time,
    const com::centreon::engine::configuration::Timeperiod& tp) {
  return com::centreon::common::check_time_against_period(
      test_time, tp,
      [](const std::string&)
          -> const com::centreon::engine::configuration::Timeperiod* {
        return nullptr;
      });
}

/**
 * @brief Check test_time is valid for the named period.
 * Exclusions within the period are resolved via the same map.
 *
 * @param test_time    Time to test.
 * @param period_name  Name of the period to look up in `periods`.
 * @param periods      Map from period name to Timeperiod const pointer.
 * @return true if allowed; also true when period_name is empty or unknown
 *         (fail-open).
 */
template <typename PeriodMap>
inline bool is_time_in_period_by_name(time_t test_time,
                                      const std::string& period_name,
                                      const PeriodMap& periods) {
  if (period_name.empty())
    return true;
  const auto it = periods.find(period_name);
  if (it == periods.end())
    return true;
  return com::centreon::common::check_time_against_period(
      test_time, *it->second,
      [&periods](const std::string& name)
          -> const com::centreon::engine::configuration::Timeperiod* {
        const auto jt = periods.find(name);
        return jt != periods.end() ? jt->second : nullptr;
      });
}

/**
 * @brief next_valid_time_in_period_by_name looked up by name.
 * Exclusions within the period are resolved via the same map.
 *
 * Returns (time_t)-1 when period_name is empty or unknown (fail-open
 * cases should never reach this path), or truly never active.
 */
template <typename PeriodMap>
inline time_t next_valid_time_in_period_by_name(time_t test_time,
                                                const std::string& period_name,
                                                const PeriodMap& periods) {
  if (period_name.empty())
    return (time_t)-1;
  const auto it = periods.find(period_name);
  if (it == periods.end())
    return (time_t)-1;
  time_t valid = 0;
  com::centreon::common::get_next_valid_time(
      test_time, &valid, it->second,
      [&periods](const std::string& name)
          -> const com::centreon::engine::configuration::Timeperiod* {
        const auto jt = periods.find(name);
        return jt != periods.end() ? jt->second : nullptr;
      });
  return valid;
}

}  // namespace com::centreon::common
