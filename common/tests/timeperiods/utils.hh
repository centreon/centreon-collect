/**
 * Copyright 2016-2024 Centreon
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

#ifndef TESTS_TIMEPERIOD_UTILS_HH
#define TESTS_TIMEPERIOD_UTILS_HH

#include <string>
#include "common/timeperiods/daterange.hh"
#include "common/timeperiods/timeperiod.hh"

class timeperiod_creator {
 public:
  timeperiod_creator();
  ~timeperiod_creator();
  com::centreon::common::timeperiods::timeperiod* get_timeperiods();
  std::shared_ptr<com::centreon::common::timeperiods::timeperiod>
  get_timeperiods_shared();
  com::centreon::common::timeperiods::timeperiod* new_timeperiod();
  void new_exclusion(
      std::shared_ptr<com::centreon::common::timeperiods::timeperiod> excluded,
      com::centreon::common::timeperiods::timeperiod* target = NULL);
  com::centreon::common::timeperiods::daterange* new_calendar_date(
      int start_year,
      int start_month,
      int start_day,
      int end_year,
      int end_month,
      int end_day,
      com::centreon::common::timeperiods::timeperiod* target = NULL);
  com::centreon::common::timeperiods::daterange* new_specific_month_date(
      int start_month,
      int start_day,
      int end_month,
      int end_day,
      com::centreon::common::timeperiods::timeperiod* target = NULL);
  com::centreon::common::timeperiods::daterange* new_generic_month_date(
      int start_day,
      int end_day,
      com::centreon::common::timeperiods::timeperiod* target = NULL);
  com::centreon::common::timeperiods::daterange*
  new_offset_weekday_of_specific_month(
      int start_month,
      int start_wday,
      int start_offset,
      int end_month,
      int end_wday,
      int end_offset,
      com::centreon::common::timeperiods::timeperiod* target = NULL);
  com::centreon::common::timeperiods::daterange*
  new_offset_weekday_of_generic_month(
      int start_wday,
      int start_offset,
      int end_wday,
      int end_offset,
      com::centreon::common::timeperiods::timeperiod* target = NULL);
  void new_timerange(int start_hour,
                     int start_minute,
                     int end_hour,
                     int end_minute,
                     com::centreon::common::timeperiods::daterange* target);
  void new_timerange(
      int start_hour,
      int start_minute,
      int end_hour,
      int end_minute,
      int day,
      com::centreon::common::timeperiods::timeperiod* target = NULL);

 private:
  std::list<std::shared_ptr<com::centreon::common::timeperiods::timeperiod>>
      _timeperiods;
};

int hmtos(int h, int m);
void set_time(time_t now);
time_t strtotimet(std::string const& str);
std::unique_ptr<com::centreon::common::timeperiods::timeperiod>
new_timeperiod_with_timeranges(const std::string& name,
                               const std::string& alias);

// Declare the external function to control time travel (control the time
// spdlog)
extern "C" void enable_time_travel(bool enable, int added);

// When enabled, time()/gettimeofday() return the real wall clock as long as the
// fake clock is inactive (set_time has not been called, i.e. gl_now == -1).
// Used by ut_common so the timeperiod tests can coexist with tests that need
// real time.
void enable_real_time_fallback(bool enable);

#endif  // !TESTS_TIMEPERIOD_UTILS_HH
