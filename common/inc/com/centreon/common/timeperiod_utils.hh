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

#pragma once
#include "common/engine_conf/timeperiod.pb.h"

namespace com::centreon::common {
namespace detail {

using Daterange = com::centreon::engine::configuration::Daterange;
using Timeperiod = com::centreon::engine::configuration::Timeperiod;
using Timerange = com::centreon::engine::configuration::Timerange;

struct time_info {
  time_t preferred_time;
  struct tm preftime;
  time_t midnight;
};

time_t add_round_days_to_midnight(time_t midnight, time_t skip);
time_t calculate_time_from_day_of_month(int year, int month, int monthday);
time_t calculate_time_from_weekday_of_month(int year,
                                            int month,
                                            int weekday,
                                            int offset);

bool daterange_calendar_date_to_time_t(const Daterange& r,
                                       const time_info& ti,
                                       time_t& start,
                                       time_t& end);
bool daterange_month_date_to_time_t(const Daterange& r,
                                    const time_info& ti,
                                    time_t& start,
                                    time_t& end);
bool daterange_month_day_to_time_t(const Daterange& r,
                                   const time_info& ti,
                                   time_t& start,
                                   time_t& end);
bool daterange_month_week_day_to_time_t(const Daterange& r,
                                        const time_info& ti,
                                        time_t& start,
                                        time_t& end);
bool daterange_week_day_to_time_t(const Daterange& r,
                                  const time_info& ti,
                                  time_t& start,
                                  time_t& end);
bool daterange_to_time_t(const Daterange& r,
                         int type,
                         const time_info& ti,
                         time_t& start,
                         time_t& end);
time_t earliest_midnight_in_daterange(time_t pref,
                                      const Daterange& dr,
                                      time_t dr_start,
                                      time_t dr_end);
bool timerange_to_time_t(const Timerange& tr,
                         const struct tm* midnight,
                         time_t& range_start,
                         time_t& range_end);

const google::protobuf::RepeatedPtrField<Timerange>& days_array_for_wday(
    const com::centreon::engine::configuration::DaysArray& days,
    int wday);

/**
 *  Get the next valid time from timeranges.
 *
 *  @param[in] preferred_time  Preferred time.
 *  @param[in] timeranges      Time ranges.
 *
 *  @return The next valid time found within the day.
 */
template <typename Ranges>
time_t get_next_valid_time_in_timeranges(time_t pref, const Ranges& ranges) {
  time_t earliest_time = (time_t)-1;
  struct tm midnight {};
  localtime_r(&pref, &midnight);
  midnight.tm_hour = 0;
  midnight.tm_min = 0;
  midnight.tm_sec = 0;
  midnight.tm_isdst = -1;
  for (const auto& tr : ranges) {
    time_t range_start, range_end;
    if (!timerange_to_time_t(tr, &midnight, range_start, range_end))
      continue;
    // Time range is in the future.
    if (range_start >= pref) {
      if (earliest_time == (time_t)-1 || range_start < earliest_time)
        earliest_time = range_start;

    }  // Preferred time is within the range.
    else if (pref < range_end) {
      earliest_time = pref;
    }
  }
  return earliest_time;
}

// Forward declarations needed because the templates functions call each
// other.
template <typename ExclusionResolver>
void get_next_invalid_time_per_timeperiod(
    const com::centreon::engine::configuration::Timeperiod& tp,
    time_t preferred_time,
    time_t* invalid_time,
    bool notif,
    ExclusionResolver resolve_exclusion);

template <typename ExclusionResolver>
void get_next_valid_time_per_timeperiod_impl(
    const com::centreon::engine::configuration::Timeperiod& tp,
    time_t preferred_time,
    time_t* valid_time,
    bool notif,
    ExclusionResolver resolve_exclusion);

template <typename ExclusionResolver>
void get_next_invalid_time_per_timeperiod(
    const com::centreon::engine::configuration::Timeperiod& tp,
    time_t preferred_time,
    time_t* invalid_time,
    bool notif,
    ExclusionResolver resolve_exclusion) {
  // If no time can be found, the original preferred time will be set
  // in invalid_time at the end of the loop.
  const time_t orig = preferred_time;

  // Do not compute more than one year ahead (we might compute forever).
  time_t earliest = preferred_time;
  const time_t limit_one_year = preferred_time + 366 * 24 * 60 * 60;

  while (earliest != (time_t)-1 && preferred_time < limit_one_year) {
    preferred_time = earliest;
    earliest = (time_t)-1;

    // Compute time information.
    time_info ti;
    ti.preferred_time = preferred_time;
    localtime_r(&preferred_time, &ti.preftime);
    ti.preftime.tm_sec = 0;
    ti.preftime.tm_min = 0;
    ti.preftime.tm_hour = 0;
    ti.preftime.tm_isdst = -1;
    ti.midnight = mktime(&ti.preftime);

    // XXX: handle range end reached.
    // Browse all date range.
    auto check_exception_list = [&](const auto& list) {
      for (const auto& dr : list) {
        if (earliest != (time_t)-1)
          return;
        time_t ds = (time_t)-1, de = (time_t)-1;
        if (!daterange_to_time_t(dr, dr.type(), ti, ds, de))
          continue;
        if (de != (time_t)-1 && preferred_time >= de)
          continue;

        // Check that date is within range.
        time_t earliest_midnight =
            earliest_midnight_in_daterange(preferred_time, dr, ds, de);
        if (earliest_midnight == (time_t)-1)
          continue;

        // Midnight.
        struct tm mid {};
        localtime_r(&earliest_midnight, &mid);

        // Browse all time range of date range.
        for (const auto& tr : dr.timerange()) {
          // Get range limits
          time_t range_start = (time_t)-1;
          time_t range_end = (time_t)-1;
          if (timerange_to_time_t(tr, &mid, range_start, range_end) &&
              preferred_time >= range_start && preferred_time < range_end)
            earliest = range_end;
        }
      }
    };
    check_exception_list(tp.exceptions().calendar_date());
    if (earliest == (time_t)-1)
      check_exception_list(tp.exceptions().month_date());
    if (earliest == (time_t)-1)
      check_exception_list(tp.exceptions().month_day());
    if (earliest == (time_t)-1)
      check_exception_list(tp.exceptions().month_week_day());
    if (earliest == (time_t)-1)
      check_exception_list(tp.exceptions().week_day());

    /*
    ** Find next available time from normal, weekly rotating schedule.
    ** We do not need to check more than 8 days (today plus 7 days
    ** ahead) because time ranges are recurring the same way every week.
    */
    for (int weekday = ti.preftime.tm_wday, days_into_future = 0;
         days_into_future <= 7 && earliest == (time_t)-1;
         ++weekday, ++days_into_future) {
      if (weekday >= 7)
        weekday -= 7;

      // Calculate start of this future weekday.
      time_t day_start = add_round_days_to_midnight(
          ti.midnight, days_into_future * 24 * 60 * 60);

      struct tm day_midnight {};
      localtime_r(&day_start, &day_midnight);

      // Check all time ranges for this day of the week.
      for (const auto& tr : days_array_for_wday(tp.timeranges(), weekday)) {
        // Get range limits.
        time_t range_start((time_t)-1);
        time_t range_end((time_t)-1);
        if (timerange_to_time_t(tr, &day_midnight, range_start, range_end) &&
            preferred_time >= range_start && preferred_time < range_end)
          earliest = range_end;
      }
    }

    // Find next exclusion time.
    time_t next_exclusion = (time_t)-1;
    for (const auto& name : tp.exclude().data()) {
      const auto* etp = resolve_exclusion(name);
      if (!etp)
        continue;
      time_t valid = (time_t)-1;
      get_next_valid_time_per_timeperiod_impl(*etp, preferred_time, &valid,
                                              notif, resolve_exclusion);
      if (valid != (time_t)-1 &&
          (next_exclusion == (time_t)-1 || valid < next_exclusion))
        next_exclusion = valid;
    }

    /*
     ** If we got an earliest_time this means that current preferred time
     ** is in a perfectly valid range. earliest_time holds the range end
     ** which might still be valid thanks to another date/time range and
     ** need to be checked. This is valid only if no exclusion occurs
     ** before.
     */
    time_t next_day = add_round_days_to_midnight(ti.midnight, 24 * 60 * 60);
    if (next_exclusion != (time_t)-1 && next_exclusion < next_day &&
        (earliest == (time_t)-1 || next_exclusion <= earliest)) {
      earliest = (time_t)-1;
      preferred_time = next_exclusion;
      break;  // We have our time, no need to search anymore.
    }
    if (earliest != (time_t)-1) {
      preferred_time = earliest;
      earliest = (time_t)-1;
    }
  }
  // If we couldn't find a time period there must be none defined.
  if (earliest != (time_t)-1)
    *invalid_time = orig;
  // Else use the calculated time.
  else
    *invalid_time = preferred_time;
}

template <typename ExclusionResolver>
void get_next_valid_time_per_timeperiod_impl(
    const com::centreon::engine::configuration::Timeperiod& tp,
    time_t preferred_time,
    time_t* valid_time,
    bool notif,
    ExclusionResolver resolve_exclusion) {
  // If no time can be found, the original preferred time will be set
  // in valid_time at the end of the loop.
  const time_t orig = preferred_time;

  // Loop through the upcoming year a day at a time.
  time_t earliest = (time_t)-1;
  time_info ti;
  ti.preferred_time = preferred_time;
  const time_t limit = preferred_time + 366 * 24 * 60 * 60;

  while (earliest == (time_t)-1 && ti.preferred_time < limit) {
    // Compute time information.
    localtime_r(&ti.preferred_time, &ti.preftime);
    ti.preftime.tm_sec = ti.preftime.tm_min = ti.preftime.tm_hour = 0;
    ti.preftime.tm_isdst = -1;
    ti.midnight = mktime(&ti.preftime);

    // Browse all date range types in precedence order.
    bool skip_day = false;

    auto check_list = [&](const auto& list) {
      // Browse all date ranges of a given type. The earliest valid
      // time found in any date range will be valid.
      for (const auto& daterange : list) {
        // Get next range limits and check that we are within bounds.
        time_t daterange_start_time = (time_t)-1;
        time_t daterange_end_time = (time_t)-1;
        if (!daterange_to_time_t(daterange, daterange.type(), ti,
                                 daterange_start_time, daterange_end_time))
          continue;
        if (daterange_start_time != (time_t)-1 &&
            daterange_start_time > ti.midnight)
          continue;
        if (daterange_end_time != (time_t)-1 &&
            ti.midnight >= daterange_end_time)
          continue;
        // Only test today. An higher precedence exception might have
        // been skipped because it was not valid on the current day
        // but could be valid tomorrow.
        time_t potential_time = get_next_valid_time_in_timeranges(
            ti.preferred_time, daterange.timerange());

        // Potential time found.
        if (potential_time != (time_t)-1) {
          if (earliest == (time_t)-1 || potential_time < earliest)
            earliest = potential_time;
          skip_day = false;
        } else {
          // No valid potential time. As the date range is valid anyhow,
          // skip this day to handle exceptions precedence.
          skip_day = true;
          return;
        }
      }
    };
    check_list(tp.exceptions().calendar_date());
    if (earliest == (time_t)-1 && !skip_day)
      check_list(tp.exceptions().month_date());
    if (earliest == (time_t)-1 && !skip_day)
      check_list(tp.exceptions().month_day());
    if (earliest == (time_t)-1 && !skip_day)
      check_list(tp.exceptions().month_week_day());
    if (earliest == (time_t)-1 && !skip_day)
      check_list(tp.exceptions().week_day());

    // Check if we should skip this day.
    if (!skip_day && earliest == (time_t)-1) {
      // Try the weekly schedule only if no valid time was found in
      // exceptions for this day.
      time_t potential_time = get_next_valid_time_in_timeranges(
          ti.preferred_time,
          days_array_for_wday(tp.timeranges(), ti.preftime.tm_wday));
      if (potential_time != (time_t)-1)
        earliest = potential_time;
    }

    // Check exclusions.
    bool skipped = false;
    if (earliest != (time_t)-1) {
      time_t max_invalid = (time_t)-1;

      for (const auto& name : tp.exclude().data()) {
        const auto* etp = resolve_exclusion(name);
        if (!etp)
          continue;
        time_t invalid = (time_t)-1;
        get_next_invalid_time_per_timeperiod(*etp, earliest, &invalid, notif,
                                             resolve_exclusion);
        if (invalid != (time_t)-1 &&
            (max_invalid == (time_t)-1 || invalid > max_invalid))
          max_invalid = invalid;
      }
      if (max_invalid != (time_t)-1 && max_invalid != earliest) {
        earliest = (time_t)-1;
        ti.preferred_time = max_invalid;
        skipped = true;
      }
    }
    // Skip if not already done through exceptions.
    if (!skipped)
      ti.preferred_time = add_round_days_to_midnight(ti.midnight, 24 * 60 * 60);
  }
  // If we couldn't find a time period there must be none defined.
  if (earliest == (time_t)-1 && !notif)
    *valid_time = orig;
  // Else use the calculated time.
  else
    *valid_time = earliest;
}

}  // namespace detail

template <typename ExclusionResolver>
inline void get_next_valid_time_per_timeperiod(
    const com::centreon::engine::configuration::Timeperiod& tp,
    time_t preferred_time,
    time_t* valid_time,
    bool notif,
    ExclusionResolver resolve_exclusion) {
  detail::get_next_valid_time_per_timeperiod_impl(
      tp, preferred_time, valid_time, notif, resolve_exclusion);
}

template <typename ExclusionResolver>
inline bool check_time_against_period(
    time_t test_time,
    const com::centreon::engine::configuration::Timeperiod& tp,
    ExclusionResolver resolve_exclusion) {
  time_t v = (time_t)-1;
  detail::get_next_valid_time_per_timeperiod_impl(tp, test_time, &v, false,
                                                  resolve_exclusion);
  return v == test_time;
}

template <typename ExclusionResolver>
inline bool check_time_against_period_for_notif(
    time_t test_time,
    const com::centreon::engine::configuration::Timeperiod& tp,
    ExclusionResolver resolve_exclusion) {
  time_t v = (time_t)-1;
  detail::get_next_valid_time_per_timeperiod_impl(tp, test_time, &v, true,
                                                  resolve_exclusion);
  return v == test_time;
}

template <typename ExclusionResolver>
inline void get_next_valid_time(
    time_t pref_time,
    time_t* valid_time,
    const com::centreon::engine::configuration::Timeperiod* tp,
    ExclusionResolver resolve_exclusion) {
  // Preferred time must be now or in the future.
  time_t preferred = std::max(pref_time, time(nullptr));
  // If no timeperiod, go with the preferred time.
  if (!tp) {
    *valid_time = preferred;
    return;
  }
  // First check for possible timeperiod exclusions
  // before getting a valid_time.
  *valid_time = 0;
  detail::get_next_valid_time_per_timeperiod_impl(*tp, preferred, valid_time,
                                                  false, resolve_exclusion);
}

}  // namespace com::centreon::common
