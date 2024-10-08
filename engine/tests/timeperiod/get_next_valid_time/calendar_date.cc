/**
 * Copyright 2016 Centreon
 *
 * This file is part of Centreon Engine.
 *
 * Centreon Engine is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * Centreon Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Centreon Engine. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <gtest/gtest.h>
#include <cstring>
#include "com/centreon/clib.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/timeperiod.hh"
#include "test_engine.hh"
#include "tests/timeperiod/utils.hh"

using namespace com::centreon;
using namespace com::centreon::engine;

class GetNextValidTimeCalendarDateTest : public TestEngine {
 public:
  void default_data_set() {
    _creator.new_timeperiod();
    daterange* dr(NULL);
    // 2016-10-25 10:45-14:25
    dr = _creator.new_calendar_date(2016, 9, 25, 2016, 9, 25);
    _creator.new_timerange(10, 45, 14, 25, dr);
    // 2016-10-27-2016-10-28 08:30-12:30,18:30-21:15
    dr = _creator.new_calendar_date(2016, 9, 27, 2016, 9, 28);
    _creator.new_timerange(8, 30, 12, 30, dr);
    _creator.new_timerange(18, 30, 21, 15, dr);
  }

 protected:
  timeperiod_creator _creator;
};

// Given a timeperiod configured with calendar dates
// And we are earlier than these dates
// When get_next_valid_time() is called
// Then the next valid time is the beginning of the next date's timerange
TEST_F(GetNextValidTimeCalendarDateTest, BeforeCalendarDates) {
  default_data_set();
  time_t now(strtotimet("2016-10-24 12:00:00"));
  set_time(now);
  time_t computed((time_t)-1);
  get_next_valid_time(now, &computed, _creator.get_timeperiods());
  ASSERT_EQ(computed, strtotimet("2016-10-25 10:45:00"));
}

// Given a timeperiod configured with calendar dates
// And we are between two calendar dates
// When get_next_valid_time() is called
// Then the next valid time is the beginning of the next date's timerange
TEST_F(GetNextValidTimeCalendarDateTest, BetweenCalendarDates) {
  default_data_set();
  time_t now(strtotimet("2016-10-26 12:00:00"));
  set_time(now);
  time_t computed((time_t)-1);
  get_next_valid_time(now, &computed, _creator.get_timeperiods());
  ASSERT_EQ(computed, strtotimet("2016-10-27 08:30:00"));
}

// Given a timeperiod configured with calendar dates
// And we are within a calendar date
// When get_next_valid_time() is called
// Then the next valid time is now
TEST_F(GetNextValidTimeCalendarDateTest, WithinCalendarDate) {
  default_data_set();
  time_t now(strtotimet("2016-10-28 20:59:00"));
  set_time(now);
  time_t computed((time_t)-1);
  get_next_valid_time(now, &computed, _creator.get_timeperiods());
  ASSERT_EQ(computed, now);
}

// Given a timeperiod configured with calendar dates
// And we are after the calendar dates
// When get_next_valid_time() is called
// Then the next valid time is now
TEST_F(GetNextValidTimeCalendarDateTest, AfterCalendarDates) {
  std::unique_ptr<engine::timeperiod> tiperiod{
      new_timeperiod_with_timeranges("tperiod", "alias")};

  for (int i = 0; i < 7; ++i) {
    timerange_list list_time;
    list_time.emplace_back(8000, 85000);
    tiperiod->days[i] = list_time;
  }

  default_data_set();

  time_t now(strtotimet("2016-10-30 12:00:00"));
  set_time(now);
  time_t computed((time_t)-1);
  get_next_valid_time(now, &computed, tiperiod.get());
  ASSERT_EQ(computed, now);
}
