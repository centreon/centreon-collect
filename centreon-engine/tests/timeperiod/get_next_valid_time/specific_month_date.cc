/*
** Copyright 2016 Centreon
**
** This file is part of Centreon Engine.
**
** Centreon Engine is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Engine is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Engine. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <cstring>
#include <gtest/gtest.h>
#include "com/centreon/engine/deleter/timeperiod.hh"
#include "com/centreon/engine/objects/daterange.hh"
#include "com/centreon/engine/objects/timeperiod.hh"
#include "com/centreon/engine/objects/timerange.hh"
#include "com/centreon/engine/timeperiod.hh"
#include "tests/timeperiod/utils.hh"

using namespace com::centreon::engine;

class         GetNextValidTimeSpecificMonthDateTest : public ::testing::Test {
 public:
  void        SetUp() {
    _timeperiod = new timeperiod();
    daterange* dr(NULL);
    // October 25 10:45-14:25
    dr = add_exception_to_timeperiod(
           _timeperiod,
           DATERANGE_MONTH_DATE,
           0,
           9,
           25,
           0,
           0,
           0,
           9,
           25,
           0,
           0,
           0);
    add_timerange_to_daterange(
      dr,
      hmtos(10, 45),
      hmtos(14, 25));
    // October 27-October 28 08:30-12:30,18:30-21:15
    dr = add_exception_to_timeperiod(
           _timeperiod,
           DATERANGE_MONTH_DATE,
           0,
           9,
           27,
           0,
           0,
           0,
           9,
           28,
           0,
           0,
           0);
    add_timerange_to_daterange(
      dr,
      hmtos(8, 30),
      hmtos(12, 30));
    add_timerange_to_daterange(
      dr,
      hmtos(18, 30),
      hmtos(21, 15));
  }

  void        TearDown() {
    deleter::timeperiod(_timeperiod);
    _timeperiod = NULL;
    return ;
  }

 protected:
  timeperiod* _timeperiod;
};

// Given a timeperiod configured with specific month dates
// And we are earlier than these dates
// When get_next_valid_time() is called
// Then the next valid time is the beginning of the next date's timerange
TEST_F(GetNextValidTimeSpecificMonthDateTest, BeforeSpecificMonthDates) {
  time_t now(strtotimet("2016-10-24 12:00:00"));
  set_time(now);
  time_t expected((time_t)-1);
  get_next_valid_time(now, &expected, _timeperiod);
  ASSERT_EQ(expected, strtotimet("2016-10-25 10:45:00"));
}

// Given a timeperiod configured with specific month dates
// And we are between two specific month dates
// When get_next_valid_time() is called
// Then the next valid time is the beginning of the next date's timerange
TEST_F(GetNextValidTimeSpecificMonthDateTest, BetweenCalendarDates) {
  time_t now(strtotimet("2016-10-26 12:00:00"));
  set_time(now);
  time_t expected((time_t)-1);
  get_next_valid_time(now, &expected, _timeperiod);
  ASSERT_EQ(expected, strtotimet("2016-10-27 08:30:00"));
}

// Given a timeperiod configured with specific month dates
// And we are within a specific month date
// When get_next_valid_time() is called
// Then the next valid time is now
TEST_F(GetNextValidTimeSpecificMonthDateTest, WithinCalendarDate) {
  time_t now(strtotimet("2016-10-28 20:59:00"));
  set_time(now);
  time_t expected((time_t)-1);
  get_next_valid_time(now, &expected, _timeperiod);
  ASSERT_EQ(expected, now);
}

// Given a timeperiod configured with specific month dates
// And we are after these dates in the month
// When get_next_valid_time() is called
// Then the next valid time is the first specific month date in the next month
TEST_F(GetNextValidTimeSpecificMonthDateTest, AfterCalendarDates) {
  time_t now(strtotimet("2016-10-30 13:37:42"));
  set_time(now);
  time_t expected((time_t)-1);
  get_next_valid_time(now, &expected, _timeperiod);
  ASSERT_EQ(expected, strtotimet("2016-10-25 10:45:00"));
}
