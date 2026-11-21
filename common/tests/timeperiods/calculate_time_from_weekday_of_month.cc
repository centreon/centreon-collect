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
// calculate_time_from_weekday_of_month(year, month, weekday, weekday_offset,
// tz). The helper returns the midnight of the "Nth <weekday> of the month"
// (e.g. the 3rd Monday of October), or (time_t)-1 when that occurrence does not
// exist. These tests pin its behaviour before the planned Abseil
// (NextWeekday / PrevWeekday) rewrite.
//
// Reminder on the struct tm argument convention used by the helper:
//   - year    is a tm_year -> year - 1900 (2016 is passed as 116);
//   - month   is a tm_mon  -> 0-based     (October is passed as 9);
//   - weekday is a tm_wday -> 0-based from Sunday (Sunday = 0, Monday = 1, ...).
// A positive offset counts from the start of the month (1 = first), a negative
// offset counts from the end (-1 = last).
//
// Reference calendar — October 2016:
//   Sun  2  9 16 23 30      Mon  3 10 17 24 31     Tue  4 11 18 25
//   Wed  5 12 19 26         Thu  6 13 20 27        Fri  7 14 21 28
//   Sat  1  8 15 22 29
// (Tuesdays/Wednesdays/Thursdays/Fridays have only 4 occurrences; the other
//  weekdays have 5.)
//
// All tests evaluate in UTC so that "midnight" is unambiguous (no DST).

#include <gtest/gtest.h>

#include <ctime>

#include "absl/time/time.h"
#include "common/timeperiods/timeperiod_detail.hh"

using namespace com::centreon::common::timeperiods;

namespace {

constexpr int k2016 = 2016 - 1900;
constexpr int kOctober = 9;  // tm_mon

// tm_wday values, for readability.
constexpr int kSunday = 0;
constexpr int kMonday = 1;
constexpr int kThursday = 4;
constexpr int kSaturday = 6;

// Trustworthy oracle: the UTC midnight of a human (year, month, day).
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

// Offset 1 selects the first occurrence: the 1st Monday of October 2016 is the
// 3rd.
TEST(CalculateTimeFromWeekdayOfMonth, FirstMonday) {
  EXPECT_EQ(
      calculate_time_from_weekday_of_month(k2016, kOctober, kMonday, 1, utc()),
      utc_midnight(2016, 10, 3));
}

// A middle offset: the 3rd Monday of October 2016 is the 17th.
TEST(CalculateTimeFromWeekdayOfMonth, ThirdMonday) {
  EXPECT_EQ(
      calculate_time_from_weekday_of_month(k2016, kOctober, kMonday, 3, utc()),
      utc_midnight(2016, 10, 17));
}

// A weekday that does occur five times: the 5th Monday of October 2016 is the
// 31st (Mondays fall on 3, 10, 17, 24, 31).
TEST(CalculateTimeFromWeekdayOfMonth, FifthMondayExists) {
  EXPECT_EQ(
      calculate_time_from_weekday_of_month(k2016, kOctober, kMonday, 5, utc()),
      utc_midnight(2016, 10, 31));
}

// The first Saturday: October 1st 2016 is a Saturday.
TEST(CalculateTimeFromWeekdayOfMonth, FirstSaturday) {
  EXPECT_EQ(calculate_time_from_weekday_of_month(k2016, kOctober, kSaturday, 1,
                                                 utc()),
            utc_midnight(2016, 10, 1));
}

// A requested occurrence that does not exist returns (time_t)-1: there is no
// 5th Tuesday in October 2016 (Tuesdays are 4, 11, 18, 25 — only four).
TEST(CalculateTimeFromWeekdayOfMonth, FifthTuesdayDoesNotExistIsInvalid) {
  constexpr int kTuesday = 2;
  EXPECT_EQ(calculate_time_from_weekday_of_month(k2016, kOctober, kTuesday, 5,
                                                 utc()),
            static_cast<time_t>(-1));
}

// Offset -1 selects the last occurrence: the last Monday of October 2016 is the
// 31st.
TEST(CalculateTimeFromWeekdayOfMonth, LastMonday) {
  EXPECT_EQ(
      calculate_time_from_weekday_of_month(k2016, kOctober, kMonday, -1, utc()),
      utc_midnight(2016, 10, 31));
}

// The last Sunday of October 2016 is the 30th.
TEST(CalculateTimeFromWeekdayOfMonth, LastSunday) {
  EXPECT_EQ(
      calculate_time_from_weekday_of_month(k2016, kOctober, kSunday, -1, utc()),
      utc_midnight(2016, 10, 30));
}

// Offset -2 selects the occurrence before the last: the 2nd-to-last Thursday of
// October 2016 is the 20th (Thursdays are 6, 13, 20, 27).
TEST(CalculateTimeFromWeekdayOfMonth, SecondToLastThursday) {
  EXPECT_EQ(calculate_time_from_weekday_of_month(k2016, kOctober, kThursday, -2,
                                                 utc()),
            utc_midnight(2016, 10, 20));
}

// A positive offset larger than 5 cannot select more than the 5 possible
// weekly occurrences, so it is capped at the 5th. October 2016 has five Mondays
// (3, 10, 17, 24, 31), so a "6th Monday" request is clamped to the 5th: the
// 31st.
TEST(CalculateTimeFromWeekdayOfMonth, OffsetBeyondFiveCapsToFifth) {
  EXPECT_EQ(
      calculate_time_from_weekday_of_month(k2016, kOctober, kMonday, 6, utc()),
      utc_midnight(2016, 10, 31));
}

// A negative offset whose magnitude exceeds the number of occurrences is
// clamped to the first occurrence of the weekday. October 2016 has four
// Tuesdays (4, 11, 18, 25); a "6th-from-last Tuesday" request is clamped to the
// first Tuesday: the 4th.
TEST(CalculateTimeFromWeekdayOfMonth, LargeNegativeOffsetClampsToFirst) {
  constexpr int kTuesday = 2;
  EXPECT_EQ(calculate_time_from_weekday_of_month(k2016, kOctober, kTuesday, -6,
                                                 utc()),
            utc_midnight(2016, 10, 4));
}
