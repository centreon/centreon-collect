/**
 * Copyright 2011-2013 Merethis
 * Copyright 2014-2026 Centreon
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
#ifndef CCC_TIMEPERIODS_TIMEPERIOD_HH
#define CCC_TIMEPERIODS_TIMEPERIOD_HH

#include "absl/container/flat_hash_set.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "common/engine_conf/timeperiod_helper.hh"
#include "common/timeperiods/daterange.hh"

/* Forward declaration. */
namespace com::centreon::common::timeperiods {
class timeperiod;
}

typedef absl::flat_hash_map<
    std::string,
    std::shared_ptr<com::centreon::common::timeperiods::timeperiod>>
    timeperiod_map;
typedef std::unordered_multimap<std::string,
                                com::centreon::common::timeperiods::timeperiod*>
    timeperiodexclusion;

namespace com::centreon::common::timeperiods {

class timeperiod {
  std::string _name;
  std::string _alias;
  timeperiodexclusion _exclusions;

 public:
  timeperiod(const com::centreon::engine::configuration::Timeperiod& obj);
  void set_exclusions(
      const com::centreon::engine::configuration::StringSet& exclusions);
  void set_exceptions(
      const com::centreon::engine::configuration::ExceptionArray& array);
  void set_days(const com::centreon::engine::configuration::DaysArray& array);

  std::string const& get_name() const { return _name; };
  void set_name(const std::string& name);
  const std::string& get_alias() const { return _alias; };
  void set_alias(const std::string& alias);
  const timeperiodexclusion& get_exclusions() const { return _exclusions; };
  timeperiodexclusion& get_exclusions() { return _exclusions; };
  bool check_time_against_period(
      time_t test_time,
      const absl::TimeZone& tz = absl::LocalTimeZone()) const;
  bool check_time_against_period_for_notif(
      time_t test_time,
      const absl::TimeZone& tz = absl::LocalTimeZone()) const;
  time_t get_next_valid_time(
      time_t preferred_time,
      const absl::TimeZone& tz = absl::LocalTimeZone()) const;

  /**
   * @brief Core recurrence query: next valid time at or after @p
   * preferred_time, WITHOUT clamping to now (unlike the overload above).
   *
   * Set @p notif_timeperiod to true in a notification/negotiation context or
   * for BAM reporting: a period with no valid time then yields (time_t)-1
   * instead of @p preferred_time. With false (the default scheduling use) it
   * falls back to @p preferred_time.
   *
   * @param[in]     preferred_time   The instant to search from.
   * @param[in]     notif_timeperiod true for notification/negotiation/BAM.
   * @param[in]     tz               Timezone the period is evaluated in.
   * @param[in,out] chain            Cyclic-exclusion guard (internal); omit /
   *                                 pass nullptr at the top level.
   *
   * @return The next valid time, or (time_t)-1 (see notif_timeperiod).
   */
  time_t get_next_valid_time(
      time_t preferred_time,
      bool notif_timeperiod,
      const absl::TimeZone& tz = absl::LocalTimeZone(),
      absl::flat_hash_set<const timeperiod*>* chain = nullptr) const;

  /**
   * @brief The next instant at which this period stops being valid, at or after
   * @p preferred_time (end of the covering window, start of a cutting
   * exclusion, or @p preferred_time itself when already outside).
   *
   * @param[in]     preferred_time   The instant to search from.
   * @param[in]     notif_timeperiod true for notification/negotiation/BAM.
   * @param[in]     tz               Timezone the period is evaluated in.
   * @param[in,out] chain            Cyclic-exclusion guard (internal); omit /
   *                                 pass nullptr at the top level.
   *
   * @return The next invalid time.
   */
  time_t get_next_invalid_time(
      time_t preferred_time,
      bool notif_timeperiod,
      const absl::TimeZone& tz = absl::LocalTimeZone(),
      absl::flat_hash_set<const timeperiod*>* chain = nullptr) const;
  uint32_t duration_intersect(
      time_t start_time,
      time_t end_time,
      const absl::TimeZone& tz = absl::LocalTimeZone()) const;

  void resolve(const timeperiod_map& all, uint32_t& w, uint32_t& e);

  bool operator==(timeperiod const& obj) noexcept;
  bool operator!=(timeperiod const& obj) noexcept;

  days_array days;
  exception_array exceptions;
};

/**
 * @brief Add a round number of civil days to a midnight instant, DST-safe.
 *
 * @param[in] midnight  A real midnight instant.
 * @param[in] days      Number of civil days to add (may be negative).
 * @param[in] tz        Timezone the civil arithmetic is done in.
 *
 * @return The midnight @p days civil days later.
 */
time_t add_round_days_to_midnight(
    time_t midnight,
    int days,
    const absl::TimeZone& tz = absl::LocalTimeZone());

}  // namespace com::centreon::common::timeperiods

#endif  // !CCC_TIMEPERIODS_TIMEPERIOD_HH
