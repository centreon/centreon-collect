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
#include "com/centreon/common/timeperiod_utils.hh"

namespace com::centreon::common {
namespace detail {

/**
 *  Add a round number of days (expressed in seconds) to a date.
 *
 *  @param[in] middnight  Midnight of base day.
 *  @param[in] skip       Number of days to skip (in seconds).
 *
 *  @return Midnight of the day in skip seconds.
 */
time_t add_round_days_to_midnight(time_t midnight, time_t skip) {
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
time_t calculate_time_from_day_of_month(int year, int month, int monthday) {
  struct tm t {};
  t.tm_isdst = -1;
  if (monthday > 0) {
    t.tm_year = year;
    t.tm_mon = month;
    t.tm_mday = monthday;
    time_t r = mktime(&t);

    // If we rolled over to the next month, time is invalid, assume the
    // user's intention is to keep it in the current month.
    if (t.tm_mon != month)
      return (time_t)-1;
    return r;
  }
  int day = 32;
  time_t r;
  do {
    // Back up a day.
    --day;

    // Make the new time.
    t.tm_mon = month;
    t.tm_year = year;
    t.tm_mday = day;
    t.tm_isdst = -1;
    r = mktime(&t);
  } while (r == (time_t)-1 || t.tm_mon != month);

  // Now that we know the last day, back up more.
  // Beware to roll over the whole month.
  if (-monthday >= t.tm_mday)
    t.tm_mday = 1;
  // -1 means last day of month, so add one to make this correct.
  else
    t.tm_mday += monthday + 1;
  t.tm_isdst = -1;

  return mktime(&t);
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
time_t calculate_time_from_weekday_of_month(int year,
                                            int month,
                                            int weekday,
                                            int weekday_offset) {
  // Compute first day of month (to get weekday).
  struct tm t {};
  t.tm_sec = 0;
  t.tm_min = 0;
  t.tm_hour = 0;
  t.tm_year = year;
  t.tm_mon = month;
  t.tm_mday = 1;
  t.tm_isdst = -1;
  mktime(&t);

  // How many days must we advance to reach the first instance of the
  // weekday this month ?
  int days = weekday - t.tm_wday;
  if (days < 0)
    days += 7;

  time_t r;

  // Positive offset (3rd thursday).
  if (weekday_offset > 0) {
    // How many weeks must we advance (no more than 5 possible).
    int weeks = (weekday_offset > 5) ? 5 : weekday_offset;
    days += (weeks - 1) * 7;

    // Make the new time.
    t.tm_mon = month;
    t.tm_year = year;
    t.tm_mday = days + 1;
    t.tm_isdst = -1;
    r = mktime(&t);
    // If we rolled over to the next month, time is invalid, assume the
    // user's intention is to keep it in the current month.
    if (t.tm_mon != month)
      return (time_t)-1;
    return r;
  }

  // Negative offset (last thursday, 3rd to last tuesday).
  // Find last instance of weekday in the month.
  days += 5 * 7;
  do {
    // Back up a week.
    days -= 7;

    // Make the new time.
    t.tm_mon = month;
    t.tm_year = year;
    t.tm_mday = days + 1;
    t.tm_isdst = -1;
    r = mktime(&t);
  } while (r == (time_t)-1 || t.tm_mon != month);

  // Now that we know the last instance of the weekday, back up more.
  days = (weekday_offset + 1) * 7;
  t.tm_mon = month;
  t.tm_year = year;
  // Beware to roll over the whole month.
  if (-days >= t.tm_mday)
    t.tm_mday = t.tm_mday % 7;
  else
    t.tm_mday += days;
  t.tm_isdst = -1;
  return mktime(&t);
}

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
bool daterange_calendar_date_to_time_t(const Daterange& r,
                                       const time_info& ti [[maybe_unused]],
                                       time_t& start,
                                       time_t& end) {
  struct tm t {};
  t.tm_isdst = -1;
  t.tm_mday = r.smday();
  t.tm_mon = r.smon();
  t.tm_year = r.syear() - 1900;
  if ((start = mktime(&t)) == (time_t)-1)
    return false;

  if (r.eyear()) {
    t.tm_hour = 0;
    t.tm_min = 0;
    t.tm_sec = 0;
    t.tm_isdst = -1;
    t.tm_mday = r.emday();
    t.tm_mon = r.emon();
    t.tm_year = r.eyear() - 1900;
    if ((end = mktime(&t)) == (time_t)-1)
      return false;

    end = add_round_days_to_midnight(end, 24 * 60 * 60);
  } else {
    end = (time_t)-1;
  }
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
bool daterange_month_date_to_time_t(const Daterange& r,
                                    const time_info& ti,
                                    time_t& start,
                                    time_t& end) {
  // End before start ?
  bool end_before_start =
      (r.smon() > r.emon()) || (r.smon() == r.emon() && r.smday() > r.emday());
  // At what year should we start ?
  int year = end_before_start ? ti.preftime.tm_year - 1 : ti.preftime.tm_year;
  bool found = false;
  for (int i = 0; i < 3 && !found; ++i, ++year) {
    start = calculate_time_from_day_of_month(year, r.smon(), r.smday());
    end = calculate_time_from_day_of_month(year + (end_before_start ? 1 : 0),
                                           r.emon(), r.emday());
    if (end != (time_t)-1) {
      end = add_round_days_to_midnight(end, 24 * 60 * 60);
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
bool daterange_month_day_to_time_t(const Daterange& r,
                                   const time_info& ti,
                                   time_t& start,
                                   time_t& end) {
  // Check if there is a month decay between start and end.
  bool decay;
  if (r.smday() >= 0) {
    if (r.emday() >= 0)
      decay = (r.emday() < r.smday());
    else
      decay = false;
  } else {
    if (r.emday() >= 0)
      decay = (r.smday() > r.emday());
    else
      decay = true;
  }

  // To get an interval covering the preferred time, we need two check
  // three different cases. First if there is no month decay, then we
  // check the current month only. If there is a month decay then we
  // need to check last month -> current month and current month -> next
  // month intervals.

  // No decay, current month only.
  if (!decay) {
    start = calculate_time_from_day_of_month(ti.preftime.tm_year,
                                             ti.preftime.tm_mon, r.smday());
    end = calculate_time_from_day_of_month(ti.preftime.tm_year,
                                           ti.preftime.tm_mon, r.emday());
    if (start == (time_t)-1 || end == (time_t)-1)
      return false;
    end = add_round_days_to_midnight(end, 24 * 60 * 60);
  }
  // Decay.
  else {
    // Check previous month -> current month.
    int year = ti.preftime.tm_year;
    int month = ti.preftime.tm_mon;
    if (month == 0) {
      --year;
      month = 11;
    } else {
      --month;
    }
    start = calculate_time_from_day_of_month(year, month, r.smday());
    end = calculate_time_from_day_of_month(ti.preftime.tm_year,
                                           ti.preftime.tm_mon, r.emday());
    if (start == (time_t)-1 || end == (time_t)-1)
      return false;
    end = add_round_days_to_midnight(end, 24 * 60 * 60);

    // If interval is invalid, we need to check
    // current month -> next month.
    if (ti.preferred_time >= end) {
      year = ti.preftime.tm_year;
      month = ti.preftime.tm_mon;
      if (month == 11) {
        ++year;
        month = 0;
      } else {
        ++month;
      }
      start = calculate_time_from_day_of_month(ti.preftime.tm_year,
                                               ti.preftime.tm_mon, r.smday());
      end = calculate_time_from_day_of_month(year, month, r.emday());
      if (start == (time_t)-1 || end == (time_t)-1)
        return false;
      end = add_round_days_to_midnight(end, 24 * 60 * 60);
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
bool daterange_month_week_day_to_time_t(const Daterange& r,
                                        const time_info& ti,
                                        time_t& start,
                                        time_t& end) {
  // Check if there is a year decay between start and end.
  bool decay = r.smon() > r.emon();

  // No decay, check current year only.
  if (!decay) {
    start = calculate_time_from_weekday_of_month(ti.preftime.tm_year, r.smon(),
                                                 r.swday(), r.swday_offset());
    end = calculate_time_from_weekday_of_month(ti.preftime.tm_year, r.emon(),
                                               r.ewday(), r.ewday_offset());
    if (start == (time_t)-1 || end == (time_t)-1)
      return false;
    end = add_round_days_to_midnight(end, 24 * 60 * 60);
  }
  // Decay, check previous year -> current year and
  // current year -> next year intervals.
  else {
    // Check previous year -> current year.
    start = calculate_time_from_weekday_of_month(
        ti.preftime.tm_year - 1, r.smon(), r.swday(), r.swday_offset());
    end = calculate_time_from_weekday_of_month(ti.preftime.tm_year, r.emon(),
                                               r.ewday(), r.ewday_offset());
    if (start == (time_t)-1 || end == (time_t)-1)
      return false;
    end = add_round_days_to_midnight(end, 24 * 60 * 60);

    // If interval is invalid, we need to check
    // current year -> next year.
    if (ti.preferred_time >= end) {
      start = calculate_time_from_weekday_of_month(
          ti.preftime.tm_year, r.smon(), r.swday(), r.swday_offset());
      end = calculate_time_from_weekday_of_month(
          ti.preftime.tm_year + 1, r.emon(), r.ewday(), r.ewday_offset());
      if (start == (time_t)-1 || end == (time_t)-1)
        return false;
      end = add_round_days_to_midnight(end, 24 * 60 * 60);
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
bool daterange_week_day_to_time_t(const Daterange& r,
                                  const time_info& ti,
                                  time_t& start,
                                  time_t& end) {
  // What year/month should we use ?
  int year = ti.preftime.tm_year;
  int month = ti.preftime.tm_mon;
  while (true) {
    // Calculate time of specified weekday of month.
    start = calculate_time_from_weekday_of_month(year, month, r.swday(),
                                                 r.swday_offset());

    // Use same year and month as was calculated for start time above.
    end = calculate_time_from_weekday_of_month(year, month, r.ewday(),
                                               r.ewday_offset());
    if (end == (time_t)-1) {
      // End date can't be helped, so skip it.
      if (r.ewday_offset() < 0)
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
    } else {
      end = add_round_days_to_midnight(end, 24 * 60 * 60);
    }

    // Error checking.
    if (start == (time_t)-1 || end == (time_t)-1 || start > end)
      return false;

    // We should have an interval that includes or is above
    // preferred time.
    if (ti.preferred_time < end)
      break;

    // Advance to next month (or year) if we've passed this weekday of
    // this month already.
    month = ti.preftime.tm_mon;
    if (month != 11)
      ++month;
    else {
      month = 0;
      ++year;
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
bool daterange_to_time_t(const Daterange& r,
                         int type,
                         const time_info& ti,
                         time_t& start,
                         time_t& end) {
  using pfunc = bool (*)(const Daterange&, const time_info&, time_t&, time_t&);
  static pfunc tabfunc[] = {
      daterange_calendar_date_to_time_t, daterange_month_date_to_time_t,
      daterange_month_day_to_time_t,     daterange_month_week_day_to_time_t,
      daterange_week_day_to_time_t,
  };

  if (type < 0 || static_cast<std::size_t>(type) >= std::size(tabfunc))
    return false;

  if (!tabfunc[type](r, ti, start, end))
    return false;

  // If skipping days...
  // Advance to the next possible skip date
  if (r.skip_interval() > 1 && start < ti.preferred_time) {
    // How many days have passed between skip start date
    // and preferred time ?
    unsigned long days = (ti.midnight - (unsigned long)start) / (3600 * 24);

    // Advance start date to next skip day
    if (!(days % r.skip_interval()))
      start = add_round_days_to_midnight(start, days * 24 * 60 * 60);
    else
      start = add_round_days_to_midnight(
          start, ((days - (days % r.skip_interval()) + r.skip_interval()) * 24 *
                  60 * 60));
  }

  return true;
}

/**
 *  Get the earliest midnight of day that includes preferred time or
 *  occurs later.
 *
 *  @param[in] pref_time      Preferred time.
 *  @param[in] dr             Date range.
 *  @param[in] dr_start       Date range start time.
 *  @param[in] dr_end         Date range end time.
 *
 *  @return Earliest midnight.
 */
time_t earliest_midnight_in_daterange(time_t pref,
                                      const Daterange& dr,
                                      time_t dr_start,
                                      time_t dr_end) {
  // XXX : handle full day skipping directly (from preferred_time to next
  // midnight)
  while (dr_start < dr_end || dr_end == (time_t)-1) {
    // Next day at midnight.
    time_t next_day = add_round_days_to_midnight(dr_start, 24 * 60 * 60);

    // Check range.
    if (pref < dr_start || (pref >= dr_start && pref < next_day))
      return dr_start;

    // Move to next day.
    if (dr.skip_interval() <= 1)
      dr_start = next_day;
    else
      dr_start = add_round_days_to_midnight(dr_start,
                                            dr.skip_interval() * 24 * 60 * 60);
  }
  return (time_t)-1;
}

/**
 *  Get time range limits.
 *
 *  @param[in]  tr           Time range.
 *  @param[in]  midnight     Midnight of day.
 *  @param[out] range_start  Start of time range in this specific day.
 *  @param[out] range_end    End of time range in this specific day.
 *
 *  @return True upon successful conversion.
 */
bool timerange_to_time_t(const Timerange& tr,
                         const struct tm* midnight,
                         time_t& range_start,
                         time_t& range_end) {
  struct tm my_tm;
  memcpy(&my_tm, midnight, sizeof(my_tm));
  my_tm.tm_hour = tr.range_start() / 3600;
  my_tm.tm_min = (tr.range_start() / 60) % 60;
  my_tm.tm_sec = 0;
  my_tm.tm_isdst = -1;
  range_start = mktime(&my_tm);
  my_tm.tm_hour = tr.range_end() / 3600;
  my_tm.tm_min = (tr.range_end() / 60) % 60;
  my_tm.tm_sec = 0;
  my_tm.tm_isdst = -1;
  range_end = mktime(&my_tm);
  return range_start <= range_end;
}

const google::protobuf::RepeatedPtrField<Timerange>& days_array_for_wday(
    const com::centreon::engine::configuration::DaysArray& days,
    int wday) {
  switch (wday) {
    case 0:
      return days.sunday();
    case 1:
      return days.monday();
    case 2:
      return days.tuesday();
    case 3:
      return days.wednesday();
    case 4:
      return days.thursday();
    case 5:
      return days.friday();
    default:
      return days.saturday();
  }
}

}  // namespace detail
}  // namespace com::centreon::common
