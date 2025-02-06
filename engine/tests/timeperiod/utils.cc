/**
 * Copyright 2016-2024 Centreon
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
#include <dlfcn.h>
#include <array>
#include <cstring>
#include <ctime>
#include <list>
#include <memory>
#include <unordered_map>

#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/timerange.hh"
#include "tests/timeperiod/utils.hh"
#ifndef LEGACY_CONF
#include "common/engine_conf/timeperiod_helper.hh"
#endif

using namespace com::centreon::engine;
// Global time.
static time_t gl_now((time_t)-1);

/**
 *  Create new timeperiod creator.
 */
timeperiod_creator::timeperiod_creator() {}

/**
 *  Delete timeperiod creator and associated timeperiods.
 */
timeperiod_creator::~timeperiod_creator() {
  _timeperiods.clear();
}

/**
 *  Get generated timeperiods.
 *
 *  @return Timeperiods list.
 */
timeperiod* timeperiod_creator::get_timeperiods() {
  return (_timeperiods.begin()->get());
}

std::shared_ptr<timeperiod> timeperiod_creator::get_timeperiods_shared() {
  return (*_timeperiods.begin());
}

#ifdef LEGACY_CONF
/**
 *  Create a new timeperiod.
 *
 *  @return The newly created timeperiod.
 */
timeperiod* timeperiod_creator::new_timeperiod() {
  std::shared_ptr<timeperiod> tp{new timeperiod("test", "test")};
  _timeperiods.push_front(tp);
  return tp.get();
}
#else
/**
 *  Create a new timeperiod.
 *
 *  @return The newly created timeperiod.
 */
timeperiod* timeperiod_creator::new_timeperiod() {
  configuration::Timeperiod conf_tp;
  configuration::timeperiod_helper tp_hlp(&conf_tp);
  conf_tp.set_timeperiod_name("test");
  conf_tp.set_alias("test");
  std::shared_ptr<timeperiod> tp = std::make_shared<timeperiod>(conf_tp);
  _timeperiods.push_front(tp);
  return tp.get();
}
#endif

/**
 *  Create a new exclusion on the timeperiod.
 *
 *  @param[in]  excluded  Excluded timeperiod.
 *  @param[out] target    Target timeperiod.
 */
void timeperiod_creator::new_exclusion(std::shared_ptr<timeperiod> excluded,
                                       timeperiod* target) {
  if (!target)
    target = _timeperiods.begin()->get();

  target->get_exclusions().insert({excluded->get_name(), excluded.get()});
}

/**
 *  Create a new calendar date range.
 *
 *  @param[in]  start_year   Start year.
 *  @param[in]  start_month  Start month.
 *  @param[in]  start_day    Start day.
 *  @param[in]  end_year     End year.
 *  @param[in]  end_month    End month.
 *  @param[in]  end_day      End day.
 *  @param[out] target       Target timeperiod.
 *
 *  @return The newly created daterange.
 */
daterange* timeperiod_creator::new_calendar_date(int start_year,
                                                 int start_month,
                                                 int start_day,
                                                 int end_year,
                                                 int end_month,
                                                 int end_day,
                                                 timeperiod* target) {
  if (!target)
    target = _timeperiods.begin()->get();

#ifdef LEGACY_CONF
  target->exceptions[daterange::calendar_date].emplace_back(
      daterange::calendar_date, start_year, start_month, start_day, 0, 0,
      end_year, end_month, end_day, 0, 0, 0,
      std::list<configuration::timerange>());
#else
  target->exceptions[daterange::calendar_date].emplace_back(
      daterange::calendar_date, start_year, start_month, start_day, 0, 0,
      end_year, end_month, end_day, 0, 0, 0,
      google::protobuf::RepeatedPtrField<configuration::Timerange>());
#endif
  return &*target->exceptions[daterange::calendar_date].rbegin();
}

/**
 *  Create a new specific month date range.
 *
 *  @param[in]  start_month  Start month.
 *  @param[in]  start_day    Start day.
 *  @param[in]  end_month    End month.
 *  @param[in]  end_day      End day.
 *  @param[out] target       Target timeperiod.
 *
 *  @return The newly created daterange.
 */
daterange* timeperiod_creator::new_specific_month_date(int start_month,
                                                       int start_day,
                                                       int end_month,
                                                       int end_day,
                                                       timeperiod* target) {
  if (!target)
    target = _timeperiods.begin()->get();

#ifdef LEGACY_CONF
  target->exceptions[daterange::month_date].emplace_back(
      daterange::month_date, 0, start_month, start_day, 0, 0, 0, end_month,
      end_day, 0, 0, 0, std::list<configuration::timerange>());
#else
  target->exceptions[daterange::month_date].emplace_back(
      daterange::month_date, 0, start_month, start_day, 0, 0, 0, end_month,
      end_day, 0, 0, 0,
      google::protobuf::RepeatedPtrField<configuration::Timerange>());
#endif
  return &*target->exceptions[daterange::month_date].rbegin();
}

/**
 *  Create a new generic month date.
 *
 *  @param[in]  start_day  Start day.
 *  @param[in]  end_day    End day.
 *  @param[out] target     Target timeperiod.
 *
 *  @return The newly created daterange.
 */
daterange* timeperiod_creator::new_generic_month_date(int start_day,
                                                      int end_day,
                                                      timeperiod* target) {
  if (!target)
    target = _timeperiods.begin()->get();

  std::shared_ptr<daterange> dr{new daterange(
      daterange::month_day, 0, 0, start_day, 0, 0, 0, 0, end_day, 0, 0, 0, {})};

#ifdef LEGACY_CONF
  target->exceptions[daterange::month_day].emplace_back(
      daterange::month_day, 0, 0, start_day, 0, 0, 0, 0, end_day, 0, 0, 0,
      std::list<configuration::timerange>());
#else
  target->exceptions[daterange::month_day].emplace_back(
      daterange::month_day, 0, 0, start_day, 0, 0, 0, 0, end_day, 0, 0, 0,
      google::protobuf::RepeatedPtrField<configuration::Timerange>());
#endif
  return &*target->exceptions[daterange::month_day].rbegin();
}

/**
 *  Create a new offset weekday daterange.
 *
 *  @param[in]  start_month   Start month.
 *  @param[in]  start_wday    Start week day.
 *  @param[in]  start_offset  Start offset.
 *  @param[in]  end_month     End month.
 *  @param[in]  end_wday      End week day.
 *  @param[in]  end_offset    End offset.
 *  @param[out] timeperiod    Target timeperiod.
 *
 *  @return The newly created daterange.
 */
daterange* timeperiod_creator::new_offset_weekday_of_specific_month(
    int start_month,
    int start_wday,
    int start_offset,
    int end_month,
    int end_wday,
    int end_offset,
    timeperiod* target) {
  if (!target)
    target = _timeperiods.begin()->get();

#ifdef LEGACY_CONF
  target->exceptions[daterange::month_week_day].emplace_back(
      daterange::month_week_day, 0, start_month, 0, start_wday, start_offset, 0,
      end_month, 0, end_wday, end_offset, 0,
      std::list<configuration::timerange>());
#else
  target->exceptions[daterange::month_week_day].emplace_back(
      daterange::month_week_day, 0, start_month, 0, start_wday, start_offset, 0,
      end_month, 0, end_wday, end_offset, 0,
      google::protobuf::RepeatedPtrField<configuration::Timerange>());
#endif
  return &*target->exceptions[daterange::month_week_day].rbegin();
}

/**
 *  Create a new offset weekday daterange.
 *
 *  @param[in]  start_wday    Start week day.
 *  @param[in]  start_offset  Start offset.
 *  @param[in]  end_wday      End week day.
 *  @param[in]  end_offset    End offset.
 *  @param[out] timeperiod    Target timeperiod.
 *
 *  @return The newly created daterange.
 */
daterange* timeperiod_creator::new_offset_weekday_of_generic_month(
    int start_wday,
    int start_offset,
    int end_wday,
    int end_offset,
    timeperiod* target) {
  if (!target)
    target = _timeperiods.begin()->get();

#ifdef LEGACY_CONF
  target->exceptions[daterange::week_day].emplace_back(
      daterange::week_day, 0, 0, 0, start_wday, start_offset, 0, 0, 0, end_wday,
      end_offset, 0, std::list<configuration::timerange>());
#else
  target->exceptions[daterange::week_day].emplace_back(
      daterange::week_day, 0, 0, 0, start_wday, start_offset, 0, 0, 0, end_wday,
      end_offset, 0,
      google::protobuf::RepeatedPtrField<configuration::Timerange>());
#endif
  return &*target->exceptions[daterange::week_day].rbegin();
}

/**
 *  Create a new timerange in a daterange.
 *
 *  @param[in]  start_hour    Start hour.
 *  @param[in]  start_minute  Start minute.
 *  @param[in]  end_hour      End hour.
 *  @param[in]  end_minute    End minute.
 *  @param[out] target        Target daterange.
 */
void timeperiod_creator::new_timerange(int start_hour,
                                       int start_minute,
                                       int end_hour,
                                       int end_minute,
                                       daterange* target) {
  if (!target)
    return;

  target->add_timerange(
      timerange(hmtos(start_hour, start_minute), hmtos(end_hour, end_minute)));
}

/**
 *  Create a new weekday timerange.
 *
 *  @param[in]  start_hour    Start hour.
 *  @param[in]  start_minute  Start minute.
 *  @param[in]  end_hour      End hour.
 *  @param[in]  end_minute    End minute.
 *  @param[in]  day           Day.
 *  @param[out] target        Target timeperiod.
 */
void timeperiod_creator::new_timerange(int start_hour,
                                       int start_minute,
                                       int end_hour,
                                       int end_minute,
                                       int day,
                                       timeperiod* target) {
  if (!target)
    target = _timeperiods.begin()->get();

  target->days[day].emplace_back(hmtos(start_hour, start_minute),
                                 hmtos(end_hour, end_minute));
}

/**
 *  Convert hour and minutes to a number of seconds.
 *
 *  @param[in] h  Hours.
 *  @param[in] m  Minutes.
 *
 *  @return The number of seconds.
 */
int hmtos(int h, int m) {
  return h * 60 * 60 + m * 60;
}

/**
 *  Set system time for testing purposes.
 *
 *  The real system time is not changed but time() returns the requested
 *  value.
 *
 *  @param now  New system time.
 */
void set_time(time_t now) {
  gl_now = now;
}

/**
 *  Convert a string to time_t.
 *
 *  @param str  String to convert.
 *
 *  @return The converted string.
 */
time_t strtotimet(std::string const& str) {
  tm t;
  memset(&t, 0, sizeof(t));
  if (!strptime(str.c_str(), "%Y-%m-%d %H:%M:%S", &t))
    throw(engine_error() << "invalid date format");
  t.tm_isdst = -1;
  return (mktime(&t));
}

/**
 *  Overload of libc time function.
 */

#ifndef __THROW
#define __THROW
#endif  // !__THROW

extern "C" time_t time(time_t* t) __THROW {
  if (t)
    *t = gl_now;
  return (gl_now);
}

#ifdef LEGACY_GETTIMEOFDAY
extern "C" int gettimeofday(struct timeval* tv, struct timezone*) __THROW {
#else
extern "C" int gettimeofday(struct timeval* tv, void*) __THROW {
#endif
  // extern "C" int gettimeofday(struct timeval* tv, struct timezone*) __THROW {
  if (tv) {
    tv->tv_sec = gl_now;
    tv->tv_usec = 0;
  }
  return 0;
}
/**
 *  Overload of libc clock_gettime function.
 */

// Flag to control time travel
static bool time_travel_enabled = false;

// time to add
static int time_to_add = 0;

// Original clock_gettime pointer
using clock_gettime_func = int (*)(clockid_t, struct timespec*);
static clock_gettime_func real_clock_gettime = nullptr;

// Override of clock_gettime
extern "C" int clock_gettime(clockid_t clk_id, struct timespec* tp) {
  if (!real_clock_gettime) {
    real_clock_gettime = (clock_gettime_func)dlsym(RTLD_NEXT, "clock_gettime");
  }

  int result = real_clock_gettime(clk_id, tp);  // Call the real function

  if (time_travel_enabled)
    tp->tv_sec += time_to_add;  // Add time if needed

  return result;
}

// Function to enable or disable time travel
extern "C" void enable_time_travel(bool enable, int added) {
  time_travel_enabled = enable;
  time_to_add = added;
}