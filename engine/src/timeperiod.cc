/**
 * Copyright 2011-2013 Merethis
 * Copyright 2014-2024 Centreon
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

#include "com/centreon/engine/timeperiod.hh"
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/daterange.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/shared.hh"
#include "com/centreon/engine/string.hh"
#include "com/centreon/engine/timerange.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration::applier;
using namespace com::centreon::engine::logging;
using namespace com::centreon::engine::string;

timeperiod_map timeperiod::timeperiods;

/**
 * @brief Constructor of a timeperiod from its configuration protobuf object.
 *
 * @param obj The configuration protobuf object.
 */
timeperiod::timeperiod(const configuration::Timeperiod& obj)
    : _name{obj.timeperiod_name()}, _alias{obj.alias()} {
  if (_name.empty() || _alias.empty()) {
    engine_logger(log_config_error, basic)
        << "Error: Name or alias for timeperiod is NULL";
    config_logger->error("Error: Name or alias for timeperiod is NULL");
    throw engine_error() << "Could not register time period '" << _name << "'";
  }

  // Check if the timeperiod already exist.
  timeperiod_map::const_iterator it{timeperiod::timeperiods.find(_name)};
  if (it != timeperiod::timeperiods.end()) {
    config_logger->error("Error: Timeperiod '{}' has already been defined",
                         _name);
    throw engine_error() << "Could not register time period '" << _name << "'";
  }

  // Fill time period structure.
  for (auto& r : obj.timeranges().sunday())
    days[0].emplace_back(r.range_start(), r.range_end());
  for (auto& r : obj.timeranges().monday())
    days[1].emplace_back(r.range_start(), r.range_end());
  for (auto& r : obj.timeranges().tuesday())
    days[2].emplace_back(r.range_start(), r.range_end());
  for (auto& r : obj.timeranges().wednesday())
    days[3].emplace_back(r.range_start(), r.range_end());
  for (auto& r : obj.timeranges().thursday())
    days[4].emplace_back(r.range_start(), r.range_end());
  for (auto& r : obj.timeranges().friday())
    days[5].emplace_back(r.range_start(), r.range_end());
  for (auto& r : obj.timeranges().saturday())
    days[6].emplace_back(r.range_start(), r.range_end());

  auto fill_exceptions = [this](const auto& obj_daterange, int idx) {
    for (auto& r : obj_daterange) {
      exceptions[idx].emplace_back(static_cast<daterange::type_range>(r.type()),
                                   r.syear(), r.smon(), r.smday(), r.swday(),
                                   r.swday_offset(), r.eyear(), r.emon(),
                                   r.emday(), r.ewday(), r.ewday_offset(),
                                   r.skip_interval(), r.timerange());
    }
  };

  fill_exceptions(obj.exceptions().calendar_date(), 0);
  fill_exceptions(obj.exceptions().month_date(), 1);
  fill_exceptions(obj.exceptions().month_day(), 2);
  fill_exceptions(obj.exceptions().month_week_day(), 3);
  fill_exceptions(obj.exceptions().week_day(), 4);

  set_exclusions(obj.exclude());
}

void timeperiod::set_exclusions(const configuration::StringSet& exclusions) {
  _exclusions.clear();
  for (auto& s : exclusions.data())
    _exclusions.emplace(s, nullptr);
}

void timeperiod::set_exceptions(const configuration::ExceptionArray& array) {
  for (auto& e : exceptions)
    e.clear();

  auto fill_exceptions = [this](const auto& obj_daterange, int idx) {
    for (auto& r : obj_daterange) {
      //      std::list<timerange> tr;
      //      for (auto& t : r.timerange())
      //        tr.emplace_back(t.range_start(), t.range_end());
      exceptions[idx].emplace_back(static_cast<daterange::type_range>(r.type()),
                                   r.syear(), r.smon(), r.smday(), r.swday(),
                                   r.swday_offset(), r.eyear(), r.emon(),
                                   r.emday(), r.ewday(), r.ewday_offset(),
                                   r.skip_interval(), r.timerange());
    }
  };

  fill_exceptions(array.calendar_date(), 0);
  fill_exceptions(array.month_date(), 1);
  fill_exceptions(array.month_day(), 2);
  fill_exceptions(array.month_week_day(), 3);
  fill_exceptions(array.week_day(), 4);
}

void timeperiod::set_name(std::string const& name) {
  _name = name;
}

void timeperiod::set_alias(std::string const& alias) {
  _alias = alias;
}

/**
 *  Equal operator.
 *
 *  @param[in] obj1 The first object to compare.
 *  @param[in] obj2 The second object to compare.
 *
 *  @return True if is the same object, otherwise false.
 */
bool timeperiod::operator==(timeperiod const& obj) noexcept {
  if (_name == obj._name && _alias == obj._alias &&
      (_exclusions.size() == obj._exclusions.size() &&
       std::equal(_exclusions.begin(), _exclusions.end(),
                  obj._exclusions.begin(), obj._exclusions.end()))) {
    for (uint32_t i{0}; i < exceptions.size(); ++i)
      if (exceptions[i] != obj.exceptions[i])
        return false;
    for (uint32_t i{0}; i < days.size(); ++i)
      if (days[i] != obj.days[i])
        return false;
    return true;
  }
  return false;
}

/**
 *  Not equal operator.
 *
 *  @param[in] obj1 The first object to compare.
 *  @param[in] obj2 The second object to compare.
 *
 *  @return True if is not the same object, otherwise false.
 */
bool timeperiod::operator!=(timeperiod const& obj) noexcept {
  return !(*this == obj);
}

/**
 *  Add a round number of days (expressed in seconds) to a date.
 *
 *  @param[in] middnight  Midnight of base day.
 *  @param[in] skip       Number of days to skip (in seconds).
 *
 *  @return Midnight of the day in skip seconds.
 */
static time_t _add_round_days_to_midnight(time_t midnight, time_t skip) {
  // Compute expected time with no DST.
  time_t next_day_time(midnight + skip);
  struct tm next_day;
  localtime_r(&next_day_time, &next_day);

  // There was a DST shift in between.
  if (next_day.tm_hour || next_day.tm_min || next_day.tm_sec) {
    /*
    ** The trick here is to move from midnight to noon, add the skip
    ** seconds and break time down in a tm structure. We're now sure to
    ** be in the proper day (DST shift is +-1h) we only have to reset
    ** time to midnight, convert back and we're done.
    */
    next_day_time += 12 * 60 * 60;
    localtime_r(&next_day_time, &next_day);
    next_day.tm_hour = 0;
    next_day.tm_min = 0;
    next_day.tm_sec = 0;
    next_day.tm_isdst = -1;
    next_day_time = mktime(&next_day);
  }

  return next_day_time;
}

/**
 *  Returns a time (midnight) of particular (3rd, last) day in a given
 *  month.
 *
 *  @param[in] year      Year.
 *  @param[in] month     Month.
 *  @param[in] monthday  Day in month.
 *
 *  @return Requested timestamp, (time_t)-1 if conversion failed.
 */
static time_t calculate_time_from_day_of_month(int year,
                                               int month,
                                               int monthday) {
  time_t midnight;
  tm t;

  // Positive day (3rd day).
  if (monthday > 0) {
    t.tm_sec = 0;
    t.tm_min = 0;
    t.tm_hour = 0;
    t.tm_year = year;
    t.tm_mon = month;
    t.tm_mday = monthday;
    t.tm_isdst = -1;
    midnight = mktime(&t);

    // If we rolled over to the next month, time is invalid, assume the
    // user's intention is to keep it in the current month.
    if (t.tm_mon != month)
      midnight = (time_t)-1;
  }
  // Negative offset (last day, 3rd to last day).
  else {
    // Find last day in the month.
    int day(32);
    do {
      // Back up a day.
      --day;

      // Make the new time.
      t.tm_mon = month;
      t.tm_year = year;
      t.tm_mday = day;
      t.tm_isdst = -1;
      midnight = mktime(&t);
    } while ((midnight == (time_t)-1) || (t.tm_mon != month));

    // Now that we know the last day, back up more.
    t.tm_mon = month;
    t.tm_year = year;
    // Beware to roll over the whole month.
    if (-monthday >= t.tm_mday)
      t.tm_mday = 1;
    // -1 means last day of month, so add one to make this correct.
    else
      t.tm_mday += monthday + 1;
    t.tm_isdst = -1;
    midnight = mktime(&t);
  }

  return midnight;
}

/**
 *  Returns a time (midnight) of particular (3rd, last) weekday in a
 *  given month.
 *
 *  @param[in] year            Year.
 *  @param[in] month           Month.
 *  @param[in] weekday         Target weekday.
 *  @param[in] weekday_offset  Weekday offset (1 is first, 2 is second,
 *                             -1 is last).
 *
 *  @return Requested timestamp, (time_t)-1 if conversion failed.
 */
static time_t calculate_time_from_weekday_of_month(int year,
                                                   int month,
                                                   int weekday,
                                                   int weekday_offset) {
  // Compute first day of month (to get weekday).
  tm t;
  t.tm_sec = 0;
  t.tm_min = 0;
  t.tm_hour = 0;
  t.tm_year = year;
  t.tm_mon = month;
  t.tm_mday = 1;
  t.tm_isdst = -1;
  mktime(&t);
  time_t midnight;

  // How many days must we advance to reach the first instance of the
  // weekday this month ?
  int days(weekday - (t.tm_wday));
  if (days < 0)
    days += 7;

  // Positive offset (3rd thursday).
  if (weekday_offset > 0) {
    // How many weeks must we advance (no more than 5 possible).
    int weeks((weekday_offset > 5) ? 5 : weekday_offset);
    days += ((weeks - 1) * 7);

    // Make the new time.
    t.tm_mon = month;
    t.tm_year = year;
    t.tm_mday = days + 1;
    t.tm_isdst = -1;
    midnight = mktime(&t);

    // If we rolled over to the next month, time is invalid, assume the
    // user's intention is to keep it in the current month.
    if (t.tm_mon != month)
      midnight = (time_t)-1;
  }
  // Negative offset (last thursday, 3rd to last tuesday).
  else {
    // Find last instance of weekday in the month.
    days += (5 * 7);
    do {
      // Back up a week.
      days -= 7;

      // Make the new time.
      t.tm_mon = month;
      t.tm_year = year;
      t.tm_mday = days + 1;
      t.tm_isdst = -1;
      midnight = mktime(&t);
    } while ((midnight == (time_t)-1) || (t.tm_mon != month));

    // Now that we know the last instance of the weekday, back up more.
    days = ((weekday_offset + 1) * 7);
    t.tm_mon = month;
    t.tm_year = year;
    // Beware to roll over the whole month.
    if (-days >= t.tm_mday)
      t.tm_mday = t.tm_mday % 7;
    else
      t.tm_mday += days;
    t.tm_isdst = -1;
    midnight = mktime(&t);
  }

  return midnight;
}

/**
 *  Internal struct time information.
 */
struct time_info {
  time_t preferred_time;
  tm preftime;
  time_t midnight;
};

/**
 *  Calculate start time and end time for date range calendar date.
 *
 *  @param[in]  r      Range to calculate start time and end time.
 *  @param[in]  ti     Time informations.
 *  @param[out] start  Variable to fill start time.
 *  @param[out] end    Variable to fill end time.
 *
 *  @return True on success, otherwise false.
 */
static bool _daterange_calendar_date_to_time_t(daterange const& r,
                                               time_info const& ti,
                                               time_t& start,
                                               time_t& end) {
  (void)ti;

  tm t;
  t.tm_sec = 0;
  t.tm_min = 0;
  t.tm_hour = 0;
  t.tm_isdst = -1;
  t.tm_mday = r.get_smday();
  t.tm_mon = r.get_smon();
  t.tm_year = r.get_syear() - 1900;
  if ((start = mktime(&t)) == (time_t)-1)
    return false;

  if (r.get_eyear()) {
    t.tm_hour = 0;
    t.tm_min = 0;
    t.tm_sec = 0;
    t.tm_isdst = -1;
    t.tm_mday = r.get_emday();
    t.tm_mon = r.get_emon();
    t.tm_year = r.get_eyear() - 1900;
    if ((end = mktime(&t)) == (time_t)-1)
      return false;
    end = _add_round_days_to_midnight(end, 24 * 60 * 60);
  } else
    end = (time_t)-1;

  return true;
}

/**
 *  Calculate start time and end time for date range month date.
 *
 *  @param[in]  r      Range to calculate start time and end time.
 *  @param[in]  ti     Time informations.
 *  @param[out] start  Variable to fill start time.
 *  @param[out] end    Variable to fill end time.
 *
 *  @return True on success, otherwise false.
 */
static bool _daterange_month_date_to_time_t(daterange const& r,
                                            time_info const& ti,
                                            time_t& start,
                                            time_t& end) {
  // End before start ?
  bool end_before_start(
      (r.get_smon() > r.get_emon()) ||
      ((r.get_smon() == r.get_emon()) && (r.get_smday() > r.get_emday())));
  // At what year should we start ?
  int year(end_before_start ? ti.preftime.tm_year - 1 : ti.preftime.tm_year);
  bool found(false);
  for (int i(0); (i < 3) && !found; ++i, ++year) {
    start = calculate_time_from_day_of_month(year, r.get_smon(), r.get_smday());
    end = calculate_time_from_day_of_month(year + (end_before_start ? 1 : 0),
                                           r.get_emon(), r.get_emday());
    if (end != (time_t)-1) {
      end = _add_round_days_to_midnight(end, 24 * 60 * 60);
      if (ti.preferred_time < end)
        found = true;
    }
  }

  return start < end;
}

/**
 *  Calculate start time and end time for date range month day.
 *
 *  @param[in]  r      Range to calculate start time and end time.
 *  @param[in]  ti     Time informations.
 *  @param[out] start  Variable to fill start time.
 *  @param[out] end    Variable to fill end time.
 *
 *  @return True on success, otherwise false.
 */
static bool _daterange_month_day_to_time_t(daterange const& r,
                                           time_info const& ti,
                                           time_t& start,
                                           time_t& end) {
  // Check if there is a month decay between start and end.
  bool decay;
  if (r.get_smday() >= 0) {
    if (r.get_emday() >= 0)
      decay = (r.get_emday() < r.get_smday());
    else
      decay = false;
  } else {
    if (r.get_emday() >= 0)
      decay = (r.get_smday() > r.get_emday());
    else
      decay = true;
  }

  // To get an interval covering the preferred time, we need two check
  // three different cases. First if there is no month decay, the we
  // check the current month only. If there is a month decay then we
  // need to check last month -> current month and current month -> next
  // month intervals.

  // No decay, current month only.
  if (!decay) {
    start = calculate_time_from_day_of_month(ti.preftime.tm_year,
                                             ti.preftime.tm_mon, r.get_smday());
    end = calculate_time_from_day_of_month(ti.preftime.tm_year,
                                           ti.preftime.tm_mon, r.get_emday());
    if ((start == (time_t)-1) || (end == (time_t)-1))
      return false;
    end = _add_round_days_to_midnight(end, 24 * 60 * 60);
  }
  // Decay.
  else {
    // Check previous month -> current month.
    int year(ti.preftime.tm_year);
    int month(ti.preftime.tm_mon);
    if (month == 0) {
      --year;
      month = 11;
    } else
      --month;
    start = calculate_time_from_day_of_month(year, month, r.get_smday());
    end = calculate_time_from_day_of_month(ti.preftime.tm_year,
                                           ti.preftime.tm_mon, r.get_emday());
    if ((start == (time_t)-1) || (end == (time_t)-1))
      return false;
    end = _add_round_days_to_midnight(end, 24 * 60 * 60);

    // If interval is invalid, we need to check
    // current month -> next month.
    if (ti.preferred_time >= end) {
      year = ti.preftime.tm_year;
      month = ti.preftime.tm_mon;
      if (month == 11) {
        ++year;
        month = 0;
      } else
        ++month;
      start = calculate_time_from_day_of_month(
          ti.preftime.tm_year, ti.preftime.tm_mon, r.get_smday());
      end = calculate_time_from_day_of_month(year, month, r.get_emday());
      if ((start == (time_t)-1) || (end == (time_t)-1))
        return false;
      end = _add_round_days_to_midnight(end, 24 * 60 * 60);
    }
  }

  return start < end;
}

/**
 *  Calculate start time and end time for date range month week day.
 *
 *  @param[in]  r      Range to calculate start time and end time.
 *  @param[in]  ti     Time informations.
 *  @param[out] start  Variable to fill start time.
 *  @param[out] end    Variable to fill end time.
 *
 *  @return True on success, otherwise false.
 */
static bool _daterange_month_week_day_to_time_t(daterange const& r,
                                                time_info const& ti,
                                                time_t& start,
                                                time_t& end) {
  // Check if there is a year decay between start and end.
  bool decay(r.get_smon() > r.get_emon());

  // No decay, check current year only.
  if (!decay) {
    start = calculate_time_from_weekday_of_month(
        ti.preftime.tm_year, r.get_smon(), r.get_swday(), r.get_swday_offset());
    end = calculate_time_from_weekday_of_month(
        ti.preftime.tm_year, r.get_emon(), r.get_ewday(), r.get_ewday_offset());
    if ((start == (time_t)-1) || (end == (time_t)-1))
      return false;
    end = _add_round_days_to_midnight(end, 24 * 60 * 60);
  }
  // Decay, check previous year -> current year and
  // current year -> next year intervals.
  else {
    // Check previous year -> current year.
    start = calculate_time_from_weekday_of_month(ti.preftime.tm_year - 1,
                                                 r.get_smon(), r.get_swday(),
                                                 r.get_swday_offset());
    end = calculate_time_from_weekday_of_month(
        ti.preftime.tm_year, r.get_emon(), r.get_ewday(), r.get_ewday_offset());
    if ((start == (time_t)-1) || (end == (time_t)-1))
      return false;
    end = _add_round_days_to_midnight(end, 24 * 60 * 60);

    // If interval is invalid, we need to check
    // current year -> next year.
    if (ti.preferred_time >= end) {
      start = calculate_time_from_weekday_of_month(ti.preftime.tm_year,
                                                   r.get_smon(), r.get_swday(),
                                                   r.get_swday_offset());
      end = calculate_time_from_weekday_of_month(ti.preftime.tm_year + 1,
                                                 r.get_emon(), r.get_ewday(),
                                                 r.get_ewday_offset());
      if ((start == (time_t)-1) || (end == (time_t)-1))
        return false;
      end = _add_round_days_to_midnight(end, 24 * 60 * 60);
    }
  }

  return start < end;
}

/**
 *  Calculate start time and end time for date range week day.
 *
 *  @param[in]  r      Range to calculate start time and end time.
 *  @param[in]  ti     Time informations.
 *  @param[out] start  Variable to fill start time.
 *  @param[out] end    Variable to fill end time.
 *
 *  @return True on success, otherwise false.
 */
static bool _daterange_week_day_to_time_t(daterange const& r,
                                          time_info const& ti,
                                          time_t& start,
                                          time_t& end) {
  // What year/month should we use ?
  int year;
  int month;
  year = ti.preftime.tm_year;
  month = ti.preftime.tm_mon;

  while (true) {
    // Calculate time of specified weekday of month.
    start = calculate_time_from_weekday_of_month(year, month, r.get_swday(),
                                                 r.get_swday_offset());

    // Use same year and month as was calculated for start time above.
    end = calculate_time_from_weekday_of_month(year, month, r.get_ewday(),
                                               r.get_ewday_offset());
    if (end == (time_t)-1) {
      // End date can't be helped, so skip it.
      if (r.get_ewday_offset() < 0)
        return false;

      // Else end date slipped past end of month, so use last day
      // of month as end date.
      int end_month;
      int end_year;
      if (month != 11) {
        end_month = month + 1;
        end_year = year;
      } else {
        end_month = 0;
        end_year = year + 1;
      }
      end = calculate_time_from_day_of_month(end_year, end_month, 0);
    } else
      end = _add_round_days_to_midnight(end, 24 * 60 * 60);

    // Error checking.
    if (((time_t)-1 == start) || ((time_t)-1 == end) || (start > end))
      return false;

    // We should have an interval that includes or is above
    // preferred time.
    if (ti.preferred_time < end)
      break;
    // Advance to next month (or year) if we've passed this weekday of
    // this month already.
    else {
      month = ti.preftime.tm_mon;
      if (month != 11)
        ++month;
      else {
        month = 0;
        ++year;
      }
    }
  }

  return true;
}

/**
 *  Calculate start time and end time for date range.
 *
 *  @param[in]  r      Range to calculate start time and end time.
 *  @param[in]  type   Date range type.
 *  @param[in]  ti     Time informations.
 *  @param[out] start  Variable to fill start time.
 *  @param[out] end    Variable to fill end time.
 *
 *  @return True on success, otherwise false.
 */
static bool _daterange_to_time_t(daterange const& r,
                                 unsigned int type,
                                 time_info const* ti,
                                 time_t& start,
                                 time_t& end) {
  typedef bool (*pfunc)(daterange const&, time_info const&, time_t&, time_t&);
  static pfunc tabfunc[] = {
      &_daterange_calendar_date_to_time_t,   // 2009-08-11
      &_daterange_month_date_to_time_t,      // january 1
      &_daterange_month_day_to_time_t,       // day 3
      &_daterange_month_week_day_to_time_t,  // thursday 2 april
      &_daterange_week_day_to_time_t         // wednesday 1
  };

  if (type >= sizeof(tabfunc) / sizeof(*tabfunc))
    return false;
  if (!(*tabfunc[type])(r, *ti, start, end))
    return false;

  // If skipping days...
  if (r.get_skip_interval() > 1) {
    // Advance to the next possible skip date
    if (start < ti->preferred_time) {
      // How many days have passed between skip start date
      // and preferred time ?
      unsigned long days((ti->midnight - (unsigned long)start) / (3600 * 24));

      // Advance start date to next skip day
      if (!(days % r.get_skip_interval()))
        start = _add_round_days_to_midnight(start, days * 24 * 60 * 60);
      else
        start = _add_round_days_to_midnight(
            start,
            ((days - (days % r.get_skip_interval()) + r.get_skip_interval()) *
             24 * 60 * 60));
    }
  }

  return true;
}

/**
 *  Get the earliest midnight of day that includes preferred time or
 *  occurs later.
 *
 *  @param[in] preferred_time     Preferred time.
 *  @param[in] drange             Date range.
 *  @param[in] drange_start_time  Date range start time.
 *  @param[in] drange_end_time    Date range end time.
 *
 *  @return Earliest midnight.
 */
static time_t _earliest_midnight_in_daterange(time_t preferred_time,
                                              const daterange& drange,
                                              time_t drange_start_time,
                                              time_t drange_end_time) {
  // XXX : handle full day skipping directly (from preferred_time to next
  // midnight)
  while ((drange_start_time < drange_end_time) ||
         (drange_end_time == (time_t)-1)) {
    // Next day at midnight.
    time_t next_day(
        _add_round_days_to_midnight(drange_start_time, 24 * 60 * 60));

    // Check range.
    if ((preferred_time < drange_start_time) ||
        ((preferred_time >= drange_start_time) && (preferred_time < next_day)))
      return drange_start_time;

    // Move to next day.
    if (drange.get_skip_interval() <= 1)
      drange_start_time = next_day;
    else
      drange_start_time = _add_round_days_to_midnight(
          drange_start_time, drange.get_skip_interval() * 24 * 60 * 60);
  }
  return (time_t)-1;
}

/**
 *  Get time range limits.
 *
 *  @param[in]  trange       Time range.
 *  @param[in]  midnight     Midnight of day.
 *  @param[out] range_start  Start of time range in this specific day.
 *  @param[out] range_end    End of time range in this specific day.
 *
 *  @return True upon successful conversion.
 */
static bool _timerange_to_time_t(const timerange& trange,
                                 struct tm const* midnight,
                                 time_t& range_start,
                                 time_t& range_end) {
  struct tm my_tm;
  memcpy(&my_tm, midnight, sizeof(my_tm));
  my_tm.tm_hour = trange.get_range_start() / 60 / 60;
  my_tm.tm_min = (trange.get_range_start() / 60) % 60;
  my_tm.tm_isdst = -1;
  range_start = mktime(&my_tm);
  my_tm.tm_hour = trange.get_range_end() / 60 / 60;
  my_tm.tm_min = (trange.get_range_end() / 60) % 60;
  my_tm.tm_isdst = -1;
  range_end = mktime(&my_tm);
  return range_start <= range_end;
}

/**
 *  See if the specified time falls into a valid time range in the given
 *  time period.
 *
 *  @param[in] test_time  Time to test.
 *  @param[in] tperiod    Target time period.
 *
 *  @return true on success, false on failure.
 */
bool check_time_against_period(time_t test_time, timeperiod* tperiod) {
  engine_logger(dbg_functions, basic) << "check_time_against_period()";
  functions_logger->trace("check_time_against_period()");

  // If no period was specified, assume the time is good.
  if (!tperiod)
    return true;

  // Faked next valid time must be tested time.
  time_t next_valid_time{(time_t)-1};
  tperiod->get_next_valid_time_per_timeperiod(test_time, &next_valid_time,
                                              false);
  functions_logger->trace("check_time_against_period {} ret={}",
                          tperiod->get_name(), next_valid_time == test_time);

  return next_valid_time == test_time;
}

/**
 *  See if the specified time falls into a valid time range in the given
 *  time period for the notification.
 *
 *  @param[in] test_time  Time to test.
 *  @param[in] tperiod    Target time period.
 *
 *  @return true on success, false on failure.
 */
bool check_time_against_period_for_notif(time_t test_time,
                                         timeperiod* tperiod) {
  engine_logger(dbg_functions, basic)
      << "check_time_against_period_for_notif()";
  functions_logger->trace("check_time_against_period_for_notif()");

  // If no period was specified, assume the time is good.
  if (!tperiod)
    return true;

  // Faked next valid time must be tested time.
  time_t next_valid_time{(time_t)-1};
  tperiod->get_next_valid_time_per_timeperiod(test_time, &next_valid_time,
                                              true);
  return next_valid_time == test_time;
}

/**
 *  Get the next invalid time within a time period (used to compute
 *  exclusions).
 *
 *  @param[in]  preferred_time  The preferred time to check.
 *  @param[out] invalid_time    Variable to fill.
 *  @param[in]  notif_timeperiod    if called for the notification .
 */
void timeperiod::get_next_invalid_time_per_timeperiod(time_t preferred_time,
                                                      time_t* invalid_time,
                                                      bool notif_timeperiod) {
  engine_logger(dbg_functions, basic)
      << "get_next_invalid_time_per_timeperiod()";
  functions_logger->trace("get_next_invalid_time_per_timeperiod()");

  // If no time can be found, the original preferred time will be set
  // in invalid_time at the end of the loop.
  time_t original_preferred_time(preferred_time);

  // Do not compute more than one year ahead (we might compute forever).
  time_t earliest_time(preferred_time);
  time_t in_one_year(preferred_time + 366 * 24 * 60 * 60);
  while ((earliest_time != (time_t)-1) && (preferred_time < in_one_year)) {
    preferred_time = earliest_time;
    earliest_time = (time_t)-1;

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
    for (unsigned int daterange_type(0);
         (daterange_type < DATERANGE_TYPES) && ((time_t)-1 == earliest_time);
         ++daterange_type) {
      for (daterange_list::iterator
               it(this->exceptions[daterange_type].begin()),
           end(this->exceptions[daterange_type].end());
           it != end && ((time_t)-1 == earliest_time); ++it) {
        // Get range limits.
        time_t daterange_start_time((time_t)-1);
        time_t daterange_end_time((time_t)-1);
        if (_daterange_to_time_t(*it, daterange_type, &ti, daterange_start_time,
                                 daterange_end_time) &&
            ((preferred_time < daterange_end_time) ||
             ((time_t)-1 == daterange_end_time))) {
          // Check that date is within range.
          time_t earliest_midnight(_earliest_midnight_in_daterange(
              preferred_time, *it, daterange_start_time, daterange_end_time));
          if (earliest_midnight != (time_t)-1) {
            // Midnight.
            struct tm midnight;
            localtime_r(&earliest_midnight, &midnight);

            // Browse all time range of date range.
            for (timerange_list::const_iterator
                     it_tr(it->get_timerange().begin()),
                 end_tr(it->get_timerange().end());
                 it_tr != end_tr && ((time_t)-1 == earliest_time); ++it_tr) {
              // Get range limits.
              time_t range_start((time_t)-1);
              time_t range_end((time_t)-1);
              if (_timerange_to_time_t(*it_tr, &midnight, range_start,
                                       range_end) &&
                  (preferred_time >= range_start) &&
                  (preferred_time < range_end))
                earliest_time = range_end;
            }
          }
        }
      }
    }

    /*
    ** Find next available time from normal, weekly rotating schedule.
    ** We do not need to check more than 8 days (today plus 7 days
    ** ahead) because time ranges are recurring the same way every week.
    */
    for (int weekday(ti.preftime.tm_wday), days_into_the_future(0);
         (days_into_the_future <= 7) && ((time_t)-1 == earliest_time);
         ++weekday, ++days_into_the_future) {
      if (weekday >= 7)
        weekday -= 7;

      // Calculate start of this future weekday.
      time_t day_start(_add_round_days_to_midnight(
          ti.midnight, days_into_the_future * 24 * 60 * 60));
      struct tm day_midnight;
      localtime_r(&day_start, &day_midnight);

      // Check all time ranges for this day of the week.
      for (timerange_list::iterator it(this->days[weekday].begin()),
           end(this->days[weekday].end());
           it != end && ((time_t)-1 == earliest_time); ++it) {
        // Get range limits.
        time_t range_start((time_t)-1);
        time_t range_end((time_t)-1);
        if (_timerange_to_time_t(*it, &day_midnight, range_start, range_end) &&
            (preferred_time >= range_start) && (preferred_time < range_end))
          earliest_time = range_end;
      }
    }

    // Find next exclusion time.
    time_t next_exclusion((time_t)-1);
    timeperiodexclusion tpe = std::move(this->get_exclusions());
    for (timeperiodexclusion::iterator it(tpe.begin()), end(tpe.end());
         it != end; ++it) {
      time_t valid((time_t)-1);
      it->second->get_next_valid_time_per_timeperiod(preferred_time, &valid,
                                                     notif_timeperiod);
      if ((valid != (time_t)-1) &&
          (((time_t)-1 == next_exclusion) || (valid < next_exclusion)))
        next_exclusion = valid;
    }
    _exclusions = std::move(tpe);
    /*
    ** If we got an earliest_time this means that current preferred time
    ** is in a perfectly valid range. earliest_time holds the range end
    ** which might still be valid thanks to another date/time range and
    ** need to be checked. This is valid only if no exclusion occurs
    ** before.
    */

    if ((next_exclusion != (time_t)-1) &&
        (next_exclusion <
         _add_round_days_to_midnight(ti.midnight, 24 * 60 * 60)) &&
        (((time_t)-1 == earliest_time) || (next_exclusion <= earliest_time))) {
      earliest_time = (time_t)-1;
      preferred_time = next_exclusion;
      break;  // We have our time, no need to search anymore.
    }
    if (earliest_time != (time_t)-1) {
      preferred_time = earliest_time;
      earliest_time = (time_t)-1;
    }
  }

  // If we couldn't find a time period there must be none defined.
  if (earliest_time != (time_t)-1)
    *invalid_time = original_preferred_time;
  // Else use the calculated time.
  else
    *invalid_time = preferred_time;
}

/**
 *  Get the next valid time from timeranges.
 *
 *  @param[in] preferred_time  Preferred time.
 *  @param[in] timeranges      Time ranges.
 *
 *  @return The next valid time found within the day.
 */
static time_t _get_next_valid_time_in_timeranges(time_t preferred_time,
                                                 timerange_list timeranges) {
  time_t earliest_time((time_t)-1);
  struct tm midnight;
  localtime_r(&preferred_time, &midnight);
  midnight.tm_hour = 0;
  midnight.tm_min = 0;
  midnight.tm_sec = 0;
  midnight.tm_isdst = -1;
  for (timerange_list::iterator it(timeranges.begin()), end(timeranges.end());
       it != end; ++it) {
    time_t range_start((time_t)-1);
    time_t range_end((time_t)-1);
    if (_timerange_to_time_t(*it, &midnight, range_start, range_end)) {
      // Time range is in the future.
      if (range_start >= preferred_time) {
        if ((earliest_time == (time_t)-1) || (range_start < earliest_time))
          earliest_time = range_start;
      }
      // Preferred time is within the range.
      else if (preferred_time < range_end)
        earliest_time = preferred_time;
    }
  }
  return earliest_time;
}

/**
 *  Get the next valid time within a time period.
 *
 *  @param[in]  preferred_time      The preferred time to check.
 *  @param[out] valid_time          Variable to fill.
 *  @param[in]  notif_timeperiod    if called for the notification .
 */
void timeperiod::get_next_valid_time_per_timeperiod(time_t preferred_time,
                                                    time_t* valid_time,
                                                    bool notif_timeperiod) {
  engine_logger(dbg_functions, basic) << "get_next_valid_time_per_timeperiod()";
  functions_logger->trace("get_next_valid_time_per_timeperiod()");

  // If no time can be found, the original preferred time will be set
  // in valid_time at the end of the loop.
  time_t original_preferred_time(preferred_time);

  // Loop through the upcoming year a day at a time.
  time_t earliest_time((time_t)-1);
  time_info ti;
  ti.preferred_time = preferred_time;
  for (time_t in_one_year(ti.preferred_time + 366 * 24 * 60 * 60);
       (earliest_time == (time_t)-1) && (ti.preferred_time < in_one_year);) {
    // Compute time information.
    localtime_r(&ti.preferred_time, &ti.preftime);
    ti.preftime.tm_sec = 0;
    ti.preftime.tm_min = 0;
    ti.preftime.tm_hour = 0;
    ti.preftime.tm_isdst = -1;
    ti.midnight = mktime(&ti.preftime);

    // Browse all date range types in precedence order.
    bool skip_this_day(false);
    for (size_t daterange_type{0};
         daterange_type < DATERANGE_TYPES && earliest_time == (time_t)-1 &&
         !skip_this_day;
         ++daterange_type) {
      // Browse all date ranges of a given type. The earliest valid
      // time found in any date range will be valid.
      for (daterange_list::iterator it{exceptions[daterange_type].begin()},
           end{exceptions[daterange_type].end()};
           it != end; ++it) {
        // Get next range limits and check that we are within bounds.
        time_t daterange_start_time((time_t)-1);
        time_t daterange_end_time((time_t)-1);
        if (_daterange_to_time_t(*it, daterange_type, &ti, daterange_start_time,
                                 daterange_end_time) &&
            ((daterange_start_time == (time_t)-1) ||
             (daterange_start_time <= ti.midnight)) &&
            ((daterange_end_time == (time_t)-1) ||
             (ti.midnight < daterange_end_time))) {
          // Only test today. An higher precedence exception might have
          // been skipped because it was not valid on the current day
          // but could be valid tomorrow.
          time_t potential_time(_get_next_valid_time_in_timeranges(
              ti.preferred_time, it->get_timerange()));

          // Potential time found.
          if (potential_time != (time_t)-1) {
            if ((earliest_time == (time_t)-1) ||
                (potential_time < earliest_time))
              earliest_time = potential_time;
            skip_this_day = false;
          }
          // No valid potential time. As the date range is valid anyhow,
          // skip this day to handle exceptions precedence.
          else {
            skip_this_day = true;
            break;
          }
        }
      }
    }

    // Check if we should skip this day.
    if (!skip_this_day) {
      // Try the weekly schedule only if no valid time was found in
      // exceptions for this day.
      if (earliest_time == (time_t)-1) {
        time_t potential_time(_get_next_valid_time_in_timeranges(
            ti.preferred_time, this->days[ti.preftime.tm_wday]));
        if ((potential_time != (time_t)-1) &&
            ((earliest_time == (time_t)-1) || (potential_time < earliest_time)))
          earliest_time = potential_time;
      }
    }

    // Check exclusions.
    bool skipped(false);
    if (earliest_time != (time_t)-1) {
      time_t max_invalid((time_t)-1);
      timeperiodexclusion tpe = std::move(this->get_exclusions());

      for (timeperiodexclusion::iterator it(tpe.begin()), end(tpe.end());
           it != end; ++it) {
        time_t invalid((time_t)-1);
        it->second->get_next_invalid_time_per_timeperiod(
            earliest_time, &invalid, notif_timeperiod);
        if ((invalid != (time_t)-1) &&
            (((time_t)-1 == max_invalid) || (invalid > max_invalid)))
          max_invalid = invalid;
      }
      _exclusions = std::move(tpe);
      if ((max_invalid != (time_t)-1) && (max_invalid != earliest_time)) {
        earliest_time = (time_t)-1;
        ti.preferred_time = max_invalid;
        skipped = true;
      }
    }

    // Skip if not already done through exceptions.
    if (!skipped)
      ti.preferred_time =
          _add_round_days_to_midnight(ti.midnight, 24 * 60 * 60);
  }

  // If we couldn't find a time period there must be none defined.
  if ((earliest_time == (time_t)-1) && !notif_timeperiod)
    *valid_time = original_preferred_time;
  // Else use the calculated time.
  else
    *valid_time = earliest_time;
  functions_logger->trace("get_next_valid_time_per_timeperiod {} valid_time={}",
                          _name, *valid_time);
}

/**
 *  Given a preferred time, get the next valid time within a time
 *  period.
 *
 *  @param[in]  preferred_time  The preferred time to check.
 *  @param[out] valid_time      Variable to fill.
 *  @param[in]  tperiod         The time period to use.
 */
void get_next_valid_time(time_t pref_time,
                         time_t* valid_time,
                         timeperiod* tperiod) {
  engine_logger(dbg_functions, basic) << "get_next_valid_time()";
  functions_logger->trace("get_next_valid_time()");

  // Preferred time must be now or in the future.
  time_t preferred_time(std::max(pref_time, time(NULL)));

  // If no timeperiod, go with the preferred time.
  if (!tperiod) {
    *valid_time = preferred_time;
    return;
  }
  // First check for possible timeperiod exclusions
  // before getting a valid_time.
  else {
    *valid_time = 0;
    tperiod->get_next_valid_time_per_timeperiod(preferred_time, valid_time,
                                                false);
  }
}

/**
 * Resolve the timeperiod object by checking all its contained pointers and
 * assigning them.
 *
 * @param w[out] Number of warnings produced during this resolution.
 * @param e[out] Number of errors produced during this resolution.
 *
 */
void timeperiod::resolve(uint32_t& w __attribute__((unused)), uint32_t& e) {
  uint32_t errors = 0;

  // Check for illegal characters in timeperiod name.
  if (contains_illegal_object_chars(_name.c_str())) {
    engine_logger(log_verification_error, basic)
        << "Error: The name of time period '" << _name
        << "' contains one or more illegal characters.";
    config_logger->error(
        "Error: The name of time period '{}' contains one or more illegal "
        "characters.",
        _name);
    errors++;
  }

  // Check for valid timeperiod names in exclusion list.
  for (timeperiodexclusion::iterator it{_exclusions.begin()},
       end{_exclusions.end()};
       it != end; ++it) {
    timeperiod_map::const_iterator found{
        timeperiod::timeperiods.find(it->first)};

    if (found == timeperiod::timeperiods.end()) {
      engine_logger(log_verification_error, basic)
          << "Error: Excluded time period '" << it->first
          << "' specified in timeperiod '" << _name
          << "' is not defined anywhere!";
      config_logger->error(
          "Error: Excluded time period '{}' specified in timeperiod '{}' is "
          "not defined anywhere!",
          it->first, _name);
      errors++;
    } else {
      // Save the timeperiod pointer for later.
      it->second = found->second.get();
    }
  }

  // Add errors.
  if (errors) {
    e += errors;
    throw engine_error() << "Cannot resolve time period '" << _name << "'";
  }
}

void timeperiod::set_days(const configuration::DaysArray& array) {
  for (auto& d : days)
    d.clear();

  for (auto& r : array.sunday())
    days[0].emplace_back(r.range_start(), r.range_end());
  for (auto& r : array.monday())
    days[1].emplace_back(r.range_start(), r.range_end());
  for (auto& r : array.tuesday())
    days[2].emplace_back(r.range_start(), r.range_end());
  for (auto& r : array.wednesday())
    days[3].emplace_back(r.range_start(), r.range_end());
  for (auto& r : array.thursday())
    days[4].emplace_back(r.range_start(), r.range_end());
  for (auto& r : array.friday())
    days[5].emplace_back(r.range_start(), r.range_end());
  for (auto& r : array.saturday())
    days[6].emplace_back(r.range_start(), r.range_end());
}
