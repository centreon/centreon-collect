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

// Isolated characterization tests for the internal helper
// calculate_time_from_day_of_month(year, month, monthday, tz). The helper
// returns the midnight (00:00:00) of the requested day of a month, or
// (time_t)-1 when that day does not exist. These tests pin its behaviour
// before the planned Abseil (CivilDay) rewrite so any change can be proven
// equivalent.
//
// Reminder on the struct tm argument convention used by the helper:
//   - year  is a tm_year  -> year - 1900 (2016 is passed as 116);
//   - month is a tm_mon   -> 0-based     (October is passed as 9).
// All tests evaluate in UTC so that "midnight" is unambiguous (no DST), which
// isolates the calendar arithmetic from timezone handling (tested elsewhere).

#include <gtest/gtest.h>

#include <ctime>

#include "absl/time/time.h"
#include "common/timeperiods/timeperiod_detail.hh"

using namespace com::centreon::common::timeperiods;

namespace {

// 2016 is passed as tm_year, October as tm_mon below.
constexpr int k2016 = 2016 - 1900;
constexpr int kOctober = 9;   // tm_mon: October
constexpr int kFebruary = 1;  // tm_mon: February
constexpr int kNovember = 10;

// Expected value helper: the UTC midnight of a human (year, month, day), where
// month and day are 1-based as a human would write them. Independent of the
// code under test (uses timegm), so it is a trustworthy oracle.
time_t utc_midnight(int year, int month, int day) {
  struct tm t;
  std::memset(&t, 0, sizeof(t));
  t.tm_year = year - 1900;
  t.tm_mon = month - 1;
  t.tm_mday = day;
  return timegm(&t);
}

absl::TimeZone utc() {
  return absl::UTCTimeZone();
}

}  // namespace

// A positive monthday is the day number itself: the 3rd of October 2016 is
// 2016-10-03 at midnight.
TEST(CalculateTimeFromDayOfMonth, PositiveDayIsThatDay) {
  EXPECT_EQ(calculate_time_from_day_of_month(k2016, kOctober, 3, utc()),
            utc_midnight(2016, 10, 3));
}

// The last representable positive day is honoured: October has 31 days, so day
// 31 is valid and maps to 2016-10-31.
TEST(CalculateTimeFromDayOfMonth, PositiveLastDayOfA31DayMonth) {
  EXPECT_EQ(calculate_time_from_day_of_month(k2016, kOctober, 31, utc()),
            utc_midnight(2016, 10, 31));
}

// A positive day that does not exist in the month overflows into the next
// month; the helper detects this and returns (time_t)-1 (invalid) rather than
// silently rolling over. November has 30 days, so day 31 is invalid.
TEST(CalculateTimeFromDayOfMonth, PositiveDayOverflowingTheMonthIsInvalid) {
  EXPECT_EQ(calculate_time_from_day_of_month(k2016, kNovember, 31, utc()),
            static_cast<time_t>(-1));
}

// A negative monthday counts from the end of the month: -1 is the last day.
// October 2016 has 31 days, so -1 is 2016-10-31.
TEST(CalculateTimeFromDayOfMonth, NegativeMinusOneIsLastDay) {
  EXPECT_EQ(calculate_time_from_day_of_month(k2016, kOctober, -1, utc()),
            utc_midnight(2016, 10, 31));
}

// -2 is the day before the last: 2016-10-30.
TEST(CalculateTimeFromDayOfMonth, NegativeMinusTwoIsDayBeforeLast) {
  EXPECT_EQ(calculate_time_from_day_of_month(k2016, kOctober, -2, utc()),
            utc_midnight(2016, 10, 30));
}

// The last day of a month is computed from the actual calendar length: in a
// leap year February has 29 days, so -1 yields 2016-02-29.
TEST(CalculateTimeFromDayOfMonth, NegativeLastDayLeapFebruary) {
  EXPECT_EQ(calculate_time_from_day_of_month(k2016, kFebruary, -1, utc()),
            utc_midnight(2016, 2, 29));
}

// And in a common year February has 28 days, so -1 yields 2015-02-28.
TEST(CalculateTimeFromDayOfMonth, NegativeLastDayCommonFebruary) {
  EXPECT_EQ(calculate_time_from_day_of_month(2015 - 1900, kFebruary, -1, utc()),
            utc_midnight(2015, 2, 28));
}

// A negative offset whose magnitude reaches or exceeds the month length is
// clamped to the first day of the month (it cannot roll into the previous
// month). October has 31 days, so the "31st day from the end" — and anything
// beyond — is the 1st: 2016-10-01.
TEST(CalculateTimeFromDayOfMonth, NegativeOffsetBeyondMonthLengthClampsToFirst) {
  EXPECT_EQ(calculate_time_from_day_of_month(k2016, kOctober, -31, utc()),
            utc_midnight(2016, 10, 1));
  EXPECT_EQ(calculate_time_from_day_of_month(k2016, kOctober, -40, utc()),
            utc_midnight(2016, 10, 1));
}
