/**
 * Copyright 2026 Centreon
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

// Characterization tests for timeperiod::_get_next_invalid_time,
// pinning its behaviour before it is refactored (return value instead of an
// out-parameter, then an Abseil rewrite).
//
// What the method computes: from `preferred_time`, the next instant at which
// the timeperiod stops being valid. In practice — as the tests below show — it
// returns the end of the timerange that contains preferred_time (it does NOT
// extend across contiguous ranges), or preferred_time itself when that instant
// is already outside the period, or the start of an exclusion when one cuts the
// current window short.
//
// All timeranges are local seconds-since-midnight, evaluated in UTC so that the
// expected instants are unambiguous. Reference weekdays:
//   Monday    2024-06-24
//   Wednesday 2024-06-26
//   Thursday  2024-06-27

#include <gtest/gtest.h>

#include <ctime>

#include "absl/time/time.h"
#include "common/tests/timeperiods/utils.hh"
#include "common/timeperiods/timeperiod.hh"

using namespace com::centreon::common::timeperiods;

namespace {

absl::TimeZone utc() {
  return absl::UTCTimeZone();
}

// UTC instant from calendar fields (month/day 1-based as a human writes them).
time_t at(int year, int month, int day, int hour, int min, int sec) {
  struct tm t;
  std::memset(&t, 0, sizeof(t));
  t.tm_year = year - 1900;
  t.tm_mon = month - 1;
  t.tm_mday = day;
  t.tm_hour = hour;
  t.tm_min = min;
  t.tm_sec = sec;
  return timegm(&t);
}

// Day index for new_timerange: 0 = Sunday .. 6 = Saturday.
constexpr int kMonday = 1;

time_t next_invalid(timeperiod* tp, time_t preferred, bool notif = false) {
  return timeperiod_test_access::next_invalid_time(*tp, preferred, notif, utc());
}

}  // namespace

// preferred_time inside a weekly range → the end of that range.
TEST(GetNextInvalidTime, InsideWeeklyRangeReturnsRangeEnd) {
  timeperiod_creator creator;
  timeperiod* tp = creator.new_timeperiod();
  creator.new_timerange(9, 0, 17, 0, kMonday);  // Monday 09:00-17:00
  EXPECT_EQ(next_invalid(tp, at(2024, 6, 24, 12, 0, 0)),
            at(2024, 6, 24, 17, 0, 0));
}

// preferred_time after the day's range → already invalid → preferred_time.
TEST(GetNextInvalidTime, AfterRangeReturnsPreferredTime) {
  timeperiod_creator creator;
  timeperiod* tp = creator.new_timeperiod();
  creator.new_timerange(9, 0, 17, 0, kMonday);
  const time_t pref = at(2024, 6, 24, 18, 0, 0);
  EXPECT_EQ(next_invalid(tp, pref), pref);
}

// preferred_time before the day's range → already invalid → preferred_time.
TEST(GetNextInvalidTime, BeforeRangeReturnsPreferredTime) {
  timeperiod_creator creator;
  timeperiod* tp = creator.new_timeperiod();
  creator.new_timerange(9, 0, 17, 0, kMonday);
  const time_t pref = at(2024, 6, 24, 8, 0, 0);
  EXPECT_EQ(next_invalid(tp, pref), pref);
}

// Two contiguous ranges the same day: the method stops at the end of the first
// range containing preferred_time and does NOT extend to the second one.
TEST(GetNextInvalidTime, ContiguousRangesStopAtFirstEnd) {
  timeperiod_creator creator;
  timeperiod* tp = creator.new_timeperiod();
  creator.new_timerange(9, 0, 12, 0, kMonday);
  creator.new_timerange(12, 0, 17, 0, kMonday);
  EXPECT_EQ(next_invalid(tp, at(2024, 6, 24, 10, 0, 0)),
            at(2024, 6, 24, 12, 0, 0));
}

// A 24x7 period: the current day's range ends at the next midnight, and the
// method returns that (it does not roll across days).
TEST(GetNextInvalidTime, TwentyFourSevenReturnsNextMidnight) {
  timeperiod_creator creator;
  timeperiod* tp = creator.new_timeperiod();
  for (int day = 0; day < 7; ++day)
    creator.new_timerange(0, 0, 24, 0, day);
  EXPECT_EQ(next_invalid(tp, at(2024, 6, 26, 12, 0, 0)),  // Wednesday noon
            at(2024, 6, 27, 0, 0, 0));                    // Thursday 00:00
}

// An exclusion that becomes valid before the natural range end cuts the window
// short: the next invalid time is the start of the exclusion.
TEST(GetNextInvalidTime, ExclusionShortensWindow) {
  timeperiod_creator creator;
  timeperiod* main = creator.new_timeperiod();
  creator.new_timerange(9, 0, 17, 0, kMonday);  // targets `main` (front)
  creator.new_timeperiod();                      // the excluded period (front)
  creator.new_timerange(12, 0, 13, 0, kMonday);  // excluded valid 12:00-13:00
  creator.new_exclusion(creator.get_timeperiods_shared(), main);
  EXPECT_EQ(next_invalid(main, at(2024, 6, 24, 10, 0, 0)),
            at(2024, 6, 24, 12, 0, 0));
}

// Same exclusion case, but going through the notification path (notif = true):
// the result is identical here.
TEST(GetNextInvalidTime, ExclusionShortensWindowForNotif) {
  timeperiod_creator creator;
  timeperiod* main = creator.new_timeperiod();
  creator.new_timerange(9, 0, 17, 0, kMonday);
  creator.new_timeperiod();
  creator.new_timerange(12, 0, 13, 0, kMonday);
  creator.new_exclusion(creator.get_timeperiods_shared(), main);
  EXPECT_EQ(next_invalid(main, at(2024, 6, 24, 10, 0, 0), /*notif=*/true),
            at(2024, 6, 24, 12, 0, 0));
}

// preferred_time inside a date-range exception (a specific calendar date) →
// the end of that exception's timerange. Exercises the daterange branch.
TEST(GetNextInvalidTime, InsideCalendarDateExceptionReturnsRangeEnd) {
  timeperiod_creator creator;
  timeperiod* tp = creator.new_timeperiod();
  // calendar_date uses tm_mon (0-based): month 5 == June. 2024-06-24.
  daterange* dr = creator.new_calendar_date(2024, 5, 24, 2024, 5, 24);
  creator.new_timerange(9, 0, 17, 0, dr);  // 09:00-17:00 on that date
  EXPECT_EQ(next_invalid(tp, at(2024, 6, 24, 12, 0, 0)),
            at(2024, 6, 24, 17, 0, 0));
}
