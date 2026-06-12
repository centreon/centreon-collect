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

#include <gtest/gtest.h>

#include <cstdlib>

#include "com/centreon/common/timeperiod_utils.hh"

using com::centreon::common::detail::add_round_days_to_midnight;
using com::centreon::common::detail::daterange_month_date_to_time_t;
using com::centreon::common::detail::time_info;
using Daterange = com::centreon::engine::configuration::Daterange;

namespace {

constexpr time_t one_day = 24 * 60 * 60;

// Fixture pinning the local timezone to Europe/Paris so the DST transitions
// happen on known dates, then restoring whatever TZ was set before.
class add_round_days_test : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* tz = getenv("TZ");
    _saved_tz = tz ? tz : "";
    _had_tz = tz != nullptr;
    setenv("TZ", "Europe/Paris", 1);
    tzset();
  }

  void TearDown() override {
    if (_had_tz)
      setenv("TZ", _saved_tz.c_str(), 1);
    else
      unsetenv("TZ");
    tzset();
  }

 private:
  std::string _saved_tz;
  bool _had_tz = false;
};

// Build the local-midnight epoch for the given calendar day.
time_t local_midnight(int year, int month, int mday) {
  struct tm t {};
  t.tm_year = year - 1900;
  t.tm_mon = month - 1;
  t.tm_mday = mday;
  t.tm_isdst = -1;
  return mktime(&t);
}

// Break an epoch down to local time for assertions.
struct tm local_tm(time_t when) {
  struct tm t;
  localtime_r(&when, &t);
  return t;
}

// Assert that `when` is exactly local midnight of the expected calendar day.
void expect_local_midnight(time_t when, int year, int month, int mday) {
  struct tm t = local_tm(when);
  EXPECT_EQ(t.tm_year + 1900, year);
  EXPECT_EQ(t.tm_mon + 1, month);
  EXPECT_EQ(t.tm_mday, mday);
  EXPECT_EQ(t.tm_hour, 0);
  EXPECT_EQ(t.tm_min, 0);
  EXPECT_EQ(t.tm_sec, 0);
}

}  // namespace

// A single-day skip from a clean midnight lands on the next midnight, away
// from any DST transition.
TEST_F(add_round_days_test, single_day_no_dst) {
  time_t start = local_midnight(2024, 6, 15);
  time_t result = add_round_days_to_midnight(start, one_day);
  expect_local_midnight(result, 2024, 6, 16);
}

// Skipping several days at once stays at midnight and lands on the right day.
TEST_F(add_round_days_test, multiple_days_no_dst) {
  time_t start = local_midnight(2024, 6, 1);
  time_t result = add_round_days_to_midnight(start, 10 * one_day);
  expect_local_midnight(result, 2024, 6, 11);
}

// When the caller passes a time that is not at midnight (e.g. 13:00 left over
// from a prior mktime/DST adjustment), the function normalizes to the start of
// that calendar day before adding, so it must not overshoot by a day.
TEST_F(add_round_days_test, input_not_at_midnight_is_normalized) {
  time_t midday = local_midnight(2024, 6, 15) + 13 * 60 * 60;
  time_t result = add_round_days_to_midnight(midday, one_day);
  expect_local_midnight(result, 2024, 6, 16);
}

// A zero skip from a non-midnight input simply truncates back to that day's
// midnight.
TEST_F(add_round_days_test, zero_skip_truncates_to_midnight) {
  time_t midday = local_midnight(2024, 6, 15) + 13 * 60 * 60;
  time_t result = add_round_days_to_midnight(midday, 0);
  expect_local_midnight(result, 2024, 6, 15);
}

// Spring-forward: in Europe/Paris the night of 2024-03-31 loses one hour
// (02:00 -> 03:00), so that calendar day is only 23 hours long.  Adding a raw
// 86400 seconds to midnight 03-31 overshoots to 01:00 on 04-01; the function
// must absorb the DST offset and return midnight 04-01.
TEST_F(add_round_days_test, dst_spring_forward_lands_on_midnight) {
  time_t start = local_midnight(2024, 3, 31);
  time_t result = add_round_days_to_midnight(start, one_day);
  expect_local_midnight(result, 2024, 4, 1);
}

// Fall-back: in Europe/Paris the night of 2024-10-27 gains one hour
// (03:00 -> 02:00), so that calendar day is 25 hours long.  Adding a raw 86400
// seconds to midnight 10-27 falls short at 23:00 the same day; the function
// must still return midnight 10-28.
TEST_F(add_round_days_test, dst_fall_back_lands_on_midnight) {
  time_t start = local_midnight(2024, 10, 27);
  time_t result = add_round_days_to_midnight(start, one_day);
  expect_local_midnight(result, 2024, 10, 28);
}

// Regression: a non-midnight end far from any DST transition (01/08/2026 13:00,
// August has no DST change in Europe/Paris) must advance exactly one day to
// 02/08 midnight, NOT overshoot to 03/08.  Without input normalization the
// 13:00 offset is mistaken for a DST shift and the noon-trick lands a day late.
TEST_F(add_round_days_test, non_midnight_input_does_not_overshoot) {
  time_t end = local_midnight(2026, 8, 1) + 13 * 60 * 60;
  time_t result = add_round_days_to_midnight(end, one_day);
  expect_local_midnight(result, 2026, 8, 2);
}

// Crossing the spring-forward boundary from the day before still produces a
// clean sequence of midnights.
TEST_F(add_round_days_test, dst_spring_forward_crossing_from_previous_day) {
  time_t start = local_midnight(2024, 3, 30);
  time_t day1 = add_round_days_to_midnight(start, one_day);
  expect_local_midnight(day1, 2024, 3, 31);
  time_t day2 = add_round_days_to_midnight(day1, one_day);
  expect_local_midnight(day2, 2024, 4, 1);
}