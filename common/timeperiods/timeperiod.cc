/**
 * Copyright 2011-2013 Merethis
 * Copyright 2014-2026 Centreon
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

#include <ctime>

#include "absl/time/civil_time.h"
#include "absl/time/time.h"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/timeperiods/timeperiod.hh"
#include "common/timeperiods/timeperiod_detail.hh"
#include "common/timeperiods/timeperiod_manager.hh"

using namespace com::centreon;
using com::centreon::exceptions::msg_fmt;

namespace com::centreon::common::timeperiods {

namespace {

/**
 *  Stateless replacement for localtime_r: break a time_t down into local civil
 *  fields expressed in @p tz, without touching any process-global TZ state.
 *
 *  @param[in]  when  Instant to convert.
 *  @param[in]  tz    Timezone the civil fields are expressed in.
 *  @param[out] out   Filled broken-down time.
 */
inline void tm_from_time(time_t when,
                         const absl::TimeZone& tz,
                         struct tm* out) {
  *out = absl::ToTM(absl::FromTimeT(when), tz);
}

/**
 *  Stateless replacement for mktime: interpret the broken-down fields of @p t
 *  in @p tz and return the matching time_t. Like mktime, @p t is normalised in
 *  place (out-of-range fields wrap, tm_wday / tm_yday / tm_isdst are filled),
 *  and tm_isdst is honoured the same way (-1 lets the zone decide, which maps
 *  to absl::FromTM's pre-transition instant).
 *
 *  @param[in,out] t   Broken-down time to convert and normalise.
 *  @param[in]     tz  Timezone the fields are expressed in.
 *
 *  @return The matching time_t.
 */
inline time_t time_from_tm(struct tm* t, const absl::TimeZone& tz) {
  absl::Time when = absl::FromTM(*t, tz);
  *t = absl::ToTM(when, tz);
  return absl::ToTimeT(when);
}

/**
 *  Midnight (00:00:00) of a civil day, expressed in @p tz, as a time_t. A
 *  nonexistent midnight (timezones whose DST transition falls on 00:00) is
 *  resolved to the pre-transition instant, matching time_from_tm / mktime with
 *  tm_isdst = -1.
 */
inline time_t civil_midnight(absl::CivilDay d, const absl::TimeZone& tz) {
  return absl::ToTimeT(
      tz.At(absl::CivilSecond(d.year(), d.month(), d.day(), 0, 0, 0)).pre);
}

// Maps a tm_wday (0 = Sunday) to the corresponding absl::Weekday.
constexpr absl::Weekday kWeekday[7] = {
    absl::Weekday::sunday,    absl::Weekday::monday,   absl::Weekday::tuesday,
    absl::Weekday::wednesday, absl::Weekday::thursday, absl::Weekday::friday,
    absl::Weekday::saturday};

}  // namespace

/**
 * @brief Constructor of a timeperiod from its configuration protobuf object.
 *
 * @param obj The configuration protobuf object.
 */
timeperiod::timeperiod(
    const com::centreon::engine::configuration::Timeperiod& obj)
    : _name{obj.timeperiod_name()}, _alias{obj.alias()} {
  if (_name.empty() || _alias.empty()) {
    timeperiod_manager::logger()->error(
        "Error: Name or alias for timeperiod is NULL");
    throw msg_fmt("Could not register time period '{}'", _name);
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
      timerange_list trs;
      for (const auto& t : r.timerange())
        trs.emplace_back(t.range_start(), t.range_end());
      exceptions[idx].emplace_back(
          static_cast<daterange::type_range>(r.type()), r.syear(), r.smon(),
          r.smday(), r.swday(), r.swday_offset(), r.eyear(), r.emon(),
          r.emday(), r.ewday(), r.ewday_offset(), r.skip_interval(), trs);
    }
  };

  fill_exceptions(obj.exceptions().calendar_date(), 0);
  fill_exceptions(obj.exceptions().month_date(), 1);
  fill_exceptions(obj.exceptions().month_day(), 2);
  fill_exceptions(obj.exceptions().month_week_day(), 3);
  fill_exceptions(obj.exceptions().week_day(), 4);

  set_exclusions(obj.exclude());
}

void timeperiod::set_exclusions(
    const com::centreon::engine::configuration::StringSet& exclusions) {
  _exclusions.clear();
  for (auto& s : exclusions.data())
    _exclusions.emplace(s, nullptr);
}

void timeperiod::set_exceptions(
    const com::centreon::engine::configuration::ExceptionArray& array) {
  for (auto& e : exceptions)
    e.clear();

  auto fill_exceptions = [this](const auto& obj_daterange, int idx) {
    for (auto& r : obj_daterange) {
      timerange_list trs;
      for (const auto& t : r.timerange())
        trs.emplace_back(t.range_start(), t.range_end());
      exceptions[idx].emplace_back(
          static_cast<daterange::type_range>(r.type()), r.syear(), r.smon(),
          r.smday(), r.swday(), r.swday_offset(), r.eyear(), r.emon(),
          r.emday(), r.ewday(), r.ewday_offset(), r.skip_interval(), trs);
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
 *  Add a round number of days to a midnight.
 *
 *  @param[in] midnight  Midnight of base day.
 *  @param[in] days      Number of days to add.
 *  @param[in] tz        Timezone the dates are expressed in.
 *
 *  @return Midnight of the day @p days later.
 */
static time_t _add_round_days_to_midnight(time_t midnight,
                                          int days,
                                          const absl::TimeZone& tz) {
  // `midnight` is a real midnight; return the midnight `days` civil days later.
  // Civil-day arithmetic is DST-immune, so the old "+12h then snap to midnight"
  // trick is no longer needed.
  absl::CivilDay base = absl::ToCivilDay(absl::FromTimeT(midnight), tz);
  return civil_midnight(base + days, tz);
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
time_t calculate_time_from_day_of_month(int year,
                                        int month,
                                        int monthday,
                                        const absl::TimeZone& tz) {
  const int y = year + 1900;
  const int m = month + 1;  // absl::CivilDay months are 1-based.

  // Positive day: that day, unless it overflows into the next month.
  if (monthday > 0) {
    absl::CivilDay d(y, m, monthday);
    if (d.month() != m)
      return (time_t)-1;
    return civil_midnight(d, tz);
  }

  // Negative day: count from the end. Last day = first of next month minus one
  // (CivilDay normalises month 13 to January of the next year). A magnitude
  // reaching or exceeding the month length collapses to the 1st.
  const int last_day = (absl::CivilDay(y, m + 1, 1) - 1).day();
  const int mday = (-monthday >= last_day) ? 1 : (last_day + monthday + 1);
  return civil_midnight(absl::CivilDay(y, m, mday), tz);
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
                                            int weekday_offset,
                                            const absl::TimeZone& tz) {
  const int y = year + 1900;
  const int m = month + 1;
  const absl::Weekday wd = kWeekday[weekday];

  // Positive offset (3rd thursday): first occurrence on/after the 1st, then
  // advance whole weeks. No more than 5 weekly occurrences are possible.
  if (weekday_offset > 0) {
    absl::CivilDay first(y, m, 1);
    absl::CivilDay occ =
        (absl::GetWeekday(first) == wd) ? first : absl::NextWeekday(first, wd);
    const int weeks = (weekday_offset > 5) ? 5 : weekday_offset;
    occ = occ + (weeks - 1) * 7;
    // If we rolled past the month, the occurrence does not exist.
    if (occ.month() != m)
      return (time_t)-1;
    return civil_midnight(occ, tz);
  }

  // Negative offset (last thursday, 3rd to last tuesday): last occurrence
  // on/before the last day, then step back. The arithmetic (including the %7
  // clamp for magnitudes beyond the number of occurrences) matches the former
  // struct tm implementation.
  absl::CivilDay last = absl::CivilDay(y, m + 1, 1) - 1;
  absl::CivilDay last_occ =
      (absl::GetWeekday(last) == wd) ? last : absl::PrevWeekday(last, wd);
  const int last_occ_day = last_occ.day();
  const int days = (weekday_offset + 1) * 7;  // <= 0
  const int mday =
      (-days >= last_occ_day) ? (last_occ_day % 7) : (last_occ_day + days);
  return civil_midnight(absl::CivilDay(y, m, mday), tz);
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
                                               time_t& end,
                                               const absl::TimeZone& tz) {
  (void)ti;

  // smon/emon are tm_mon (0-based); CivilDay months are 1-based.
  start = civil_midnight(
      absl::CivilDay(r.get_syear(), r.get_smon() + 1, r.get_smday()), tz);

  if (r.get_eyear()) {
    // The range is inclusive of its last day, so the exclusive end is the
    // midnight of the day after.
    absl::CivilDay end_day(r.get_eyear(), r.get_emon() + 1, r.get_emday());
    end = civil_midnight(end_day + 1, tz);
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
                                            time_t& end,
                                            const absl::TimeZone& tz) {
  // End before start ?
  bool end_before_start =
      r.get_smon() > r.get_emon() ||
      (r.get_smon() == r.get_emon() && r.get_smday() > r.get_emday());
  // At what year should we start ?
  int year = end_before_start ? ti.preftime.tm_year - 1 : ti.preftime.tm_year;
  bool found = false;
  for (int i = 0; i < 3 && !found; ++i, ++year) {
    start =
        calculate_time_from_day_of_month(year, r.get_smon(), r.get_smday(), tz);
    end = calculate_time_from_day_of_month(year + (end_before_start ? 1 : 0),
                                           r.get_emon(), r.get_emday(), tz);
    if (end != (time_t)-1) {
      end = _add_round_days_to_midnight(end, 1, tz);
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
                                           time_t& end,
                                           const absl::TimeZone& tz) {
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
    start = calculate_time_from_day_of_month(
        ti.preftime.tm_year, ti.preftime.tm_mon, r.get_smday(), tz);
    end = calculate_time_from_day_of_month(
        ti.preftime.tm_year, ti.preftime.tm_mon, r.get_emday(), tz);
    if ((start == (time_t)-1) || (end == (time_t)-1))
      return false;
    end = _add_round_days_to_midnight(end, 1, tz);
  }
  // Decay.
  else {
    // Check previous month -> current month.
    int year = ti.preftime.tm_year;
    int month = ti.preftime.tm_mon;
    if (month == 0) {
      --year;
      month = 11;
    } else
      --month;
    start = calculate_time_from_day_of_month(year, month, r.get_smday(), tz);
    end = calculate_time_from_day_of_month(
        ti.preftime.tm_year, ti.preftime.tm_mon, r.get_emday(), tz);
    if ((start == (time_t)-1) || (end == (time_t)-1))
      return false;
    end = _add_round_days_to_midnight(end, 1, tz);

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
          ti.preftime.tm_year, ti.preftime.tm_mon, r.get_smday(), tz);
      end = calculate_time_from_day_of_month(year, month, r.get_emday(), tz);
      if ((start == (time_t)-1) || (end == (time_t)-1))
        return false;
      end = _add_round_days_to_midnight(end, 1, tz);
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
                                                time_t& end,
                                                const absl::TimeZone& tz) {
  // Check if there is a year decay between start and end.
  bool decay = r.get_smon() > r.get_emon();

  // No decay, check current year only.
  if (!decay) {
    start = calculate_time_from_weekday_of_month(ti.preftime.tm_year,
                                                 r.get_smon(), r.get_swday(),
                                                 r.get_swday_offset(), tz);
    end = calculate_time_from_weekday_of_month(ti.preftime.tm_year,
                                               r.get_emon(), r.get_ewday(),
                                               r.get_ewday_offset(), tz);
    if ((start == (time_t)-1) || (end == (time_t)-1))
      return false;
    end = _add_round_days_to_midnight(end, 1, tz);
  }
  // Decay, check previous year -> current year and
  // current year -> next year intervals.
  else {
    // Check previous year -> current year.
    start = calculate_time_from_weekday_of_month(ti.preftime.tm_year - 1,
                                                 r.get_smon(), r.get_swday(),
                                                 r.get_swday_offset(), tz);
    end = calculate_time_from_weekday_of_month(ti.preftime.tm_year,
                                               r.get_emon(), r.get_ewday(),
                                               r.get_ewday_offset(), tz);
    if ((start == (time_t)-1) || (end == (time_t)-1))
      return false;
    end = _add_round_days_to_midnight(end, 1, tz);

    // If interval is invalid, we need to check
    // current year -> next year.
    if (ti.preferred_time >= end) {
      start = calculate_time_from_weekday_of_month(ti.preftime.tm_year,
                                                   r.get_smon(), r.get_swday(),
                                                   r.get_swday_offset(), tz);
      end = calculate_time_from_weekday_of_month(ti.preftime.tm_year + 1,
                                                 r.get_emon(), r.get_ewday(),
                                                 r.get_ewday_offset(), tz);
      if ((start == (time_t)-1) || (end == (time_t)-1))
        return false;
      end = _add_round_days_to_midnight(end, 1, tz);
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
                                          time_t& end,
                                          const absl::TimeZone& tz) {
  // What year/month should we use ?
  int year;
  int month;
  year = ti.preftime.tm_year;
  month = ti.preftime.tm_mon;

  while (true) {
    // Calculate time of specified weekday of month.
    start = calculate_time_from_weekday_of_month(year, month, r.get_swday(),
                                                 r.get_swday_offset(), tz);

    // Use same year and month as was calculated for start time above.
    end = calculate_time_from_weekday_of_month(year, month, r.get_ewday(),
                                               r.get_ewday_offset(), tz);
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
      end = calculate_time_from_day_of_month(end_year, end_month, 0, tz);
    } else
      end = _add_round_days_to_midnight(end, 1, tz);

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
                                 uint32_t type,
                                 time_info const* ti,
                                 time_t& start,
                                 time_t& end,
                                 const absl::TimeZone& tz) {
  typedef bool (*pfunc)(daterange const&, time_info const&, time_t&, time_t&,
                        const absl::TimeZone&);
  static pfunc tabfunc[] = {
      &_daterange_calendar_date_to_time_t,   // 2009-08-11
      &_daterange_month_date_to_time_t,      // january 1
      &_daterange_month_day_to_time_t,       // day 3
      &_daterange_month_week_day_to_time_t,  // thursday 2 april
      &_daterange_week_day_to_time_t         // wednesday 1
  };

  if (type >= sizeof(tabfunc) / sizeof(*tabfunc))
    return false;
  if (!(*tabfunc[type])(r, *ti, start, end, tz))
    return false;

  // If skipping days...
  if (r.get_skip_interval() > 1) {
    // Advance to the next possible skip date
    if (start < ti->preferred_time) {
      // How many days have passed between skip start date
      // and preferred time ?
      uint64_t days =
          (ti->midnight - static_cast<uint64_t>(start)) / (3600 * 24);

      // Advance start date to next skip day
      if (!(days % r.get_skip_interval()))
        start = _add_round_days_to_midnight(start, days, tz);
      else
        start = _add_round_days_to_midnight(
            start,
            days - (days % r.get_skip_interval()) + r.get_skip_interval(), tz);
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
                                              time_t drange_end_time,
                                              const absl::TimeZone& tz) {
  // XXX : handle full day skipping directly (from preferred_time to next
  // midnight)
  while ((drange_start_time < drange_end_time) ||
         (drange_end_time == (time_t)-1)) {
    // Next day at midnight.
    time_t next_day = _add_round_days_to_midnight(drange_start_time, 1, tz);

    // Check range.
    if ((preferred_time < drange_start_time) ||
        ((preferred_time >= drange_start_time) && (preferred_time < next_day)))
      return drange_start_time;

    // Move to next day.
    if (drange.get_skip_interval() <= 1)
      drange_start_time = next_day;
    else
      drange_start_time = _add_round_days_to_midnight(
          drange_start_time, drange.get_skip_interval(), tz);
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
                                 time_t& range_end,
                                 const absl::TimeZone& tz) {
  // The instant at a seconds-since-midnight offset on `midnight`'s day, built
  // directly in civil time. Like the former mktime path, only the hour and
  // minute of the offset are used (the seconds come from `midnight`), and an
  // offset of 24:00 normalises to the next day's midnight.
  auto at = [&](int offset) {
    return absl::ToTimeT(
        tz.At(absl::CivilSecond(midnight->tm_year + 1900, midnight->tm_mon + 1,
                                midnight->tm_mday, offset / 3600,
                                (offset / 60) % 60, midnight->tm_sec))
            .pre);
  };
  range_start = at(trange.get_range_start());
  range_end = at(trange.get_range_end());
  return range_start <= range_end;
}


/**
 *  See if the specified time falls into a valid time range in this time period.
 *
 *  @param[in] test_time  Time to test.
 *  @param[in] tz         Timezone the period is evaluated in.
 *
 *  @return true if test_time is within the period.
 */
bool timeperiod::check_time_against_period(time_t test_time,
                                           const absl::TimeZone& tz) {
  timeperiod_manager::logger()->trace("check_time_against_period()");

  // Faked next valid time must be tested time.
  time_t next_valid_time{(time_t)-1};
  get_next_valid_time_per_timeperiod(test_time, &next_valid_time, false, tz);
  timeperiod_manager::logger()->trace("check_time_against_period {} ret={}",
                                      get_name(), next_valid_time == test_time);

  return next_valid_time == test_time;
}

/**
 *  See if the specified time falls into a valid time range in this time period,
 *  for the notification logic (a period with no valid time is restrictive here,
 *  unlike the regular check above).
 *
 *  @param[in] test_time  Time to test.
 *  @param[in] tz         Timezone the period is evaluated in.
 *
 *  @return true if test_time is within the period.
 */
bool timeperiod::check_time_against_period_for_notif(time_t test_time,
                                                     const absl::TimeZone& tz) {
  timeperiod_manager::logger()->trace("check_time_against_period_for_notif()");

  // Faked next valid time must be tested time.
  time_t next_valid_time{(time_t)-1};
  get_next_valid_time_per_timeperiod(test_time, &next_valid_time, true, tz);
  return next_valid_time == test_time;
}

/**
 *  Get the next invalid time within this time period (used to compute
 *  exclusions): the instant at which the period stops being valid.
 *
 *  Returns the end of the timerange that contains preferred_time, the start of
 *  an exclusion that cuts the current window short, or preferred_time itself
 *  when it is already outside the period. Only the day containing
 *  preferred_time is examined: a fixed instant cannot fall inside a future
 *  day's range.
 *
 *  @param[in] preferred_time   The preferred time to check.
 *  @param[in] notif_timeperiod If called for the notification logic.
 *  @param[in] tz               Timezone the period is evaluated in.
 *
 *  @return The next invalid time.
 */
time_t timeperiod::get_next_invalid_time_per_timeperiod(
    time_t preferred_time,
    bool notif_timeperiod,
    const absl::TimeZone& tz) {
  timeperiod_manager::logger()->trace("get_next_invalid_time_per_timeperiod()");

  // Compute time information for preferred_time.
  time_info ti;
  ti.preferred_time = preferred_time;
  tm_from_time(preferred_time, tz, &ti.preftime);
  ti.preftime.tm_sec = 0;
  ti.preftime.tm_min = 0;
  ti.preftime.tm_hour = 0;
  ti.preftime.tm_isdst = -1;
  ti.midnight = time_from_tm(&ti.preftime, tz);

  time_t earliest_time = (time_t)-1;

  // Browse date range exceptions: the end of the timerange covering
  // preferred_time is when the period stops being valid.
  for (uint32_t daterange_type = 0;
       (daterange_type < DATERANGE_TYPES) && ((time_t)-1 == earliest_time);
       ++daterange_type) {
    for (auto& dr : exceptions[daterange_type]) {
      if (earliest_time != (time_t)-1)
        break;
      time_t daterange_start_time = (time_t)-1;
      time_t daterange_end_time = (time_t)-1;
      if (_daterange_to_time_t(dr, daterange_type, &ti, daterange_start_time,
                               daterange_end_time, tz) &&
          ((preferred_time < daterange_end_time) ||
           ((time_t)-1 == daterange_end_time))) {
        time_t earliest_midnight = _earliest_midnight_in_daterange(
            preferred_time, dr, daterange_start_time, daterange_end_time, tz);
        if (earliest_midnight != (time_t)-1) {
          struct tm midnight;
          tm_from_time(earliest_midnight, tz, &midnight);
          for (const auto& tr : dr.get_timerange()) {
            time_t range_start = (time_t)-1;
            time_t range_end = (time_t)-1;
            if (_timerange_to_time_t(tr, &midnight, range_start, range_end,
                                     tz) &&
                (preferred_time >= range_start) &&
                (preferred_time < range_end)) {
              earliest_time = range_end;
              break;
            }
          }
        }
      }
    }
  }

  // Normal weekly schedule: only the day containing preferred_time can match a
  // fixed instant. ti.preftime is already that day's midnight.
  if (earliest_time == (time_t)-1) {
    for (auto& tr : days[ti.preftime.tm_wday]) {
      time_t range_start = (time_t)-1;
      time_t range_end = (time_t)-1;
      if (_timerange_to_time_t(tr, &ti.preftime, range_start, range_end, tz) &&
          (preferred_time >= range_start) && (preferred_time < range_end)) {
        earliest_time = range_end;
        break;
      }
    }
  }

  // An exclusion that becomes valid within the current day and before the end
  // of the current window cuts it short.
  time_t next_exclusion = (time_t)-1;
  timeperiodexclusion tpe = std::move(this->get_exclusions());
  for (auto& [name, excluded] : tpe) {
    time_t valid = (time_t)-1;
    excluded->get_next_valid_time_per_timeperiod(preferred_time, &valid,
                                                 notif_timeperiod, tz);
    if ((valid != (time_t)-1) &&
        (((time_t)-1 == next_exclusion) || (valid < next_exclusion)))
      next_exclusion = valid;
  }
  _exclusions = std::move(tpe);

  if ((next_exclusion != (time_t)-1) &&
      (next_exclusion < _add_round_days_to_midnight(ti.midnight, 1, tz)) &&
      (((time_t)-1 == earliest_time) || (next_exclusion <= earliest_time)))
    return next_exclusion;
  if (earliest_time != (time_t)-1)
    return earliest_time;
  return preferred_time;
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
                                                 timerange_list timeranges,
                                                 const absl::TimeZone& tz) {
  time_t earliest_time = (time_t)-1;
  struct tm midnight;
  tm_from_time(preferred_time, tz, &midnight);
  midnight.tm_hour = 0;
  midnight.tm_min = 0;
  midnight.tm_sec = 0;
  midnight.tm_isdst = -1;
  for (auto& tr : timeranges) {
    time_t range_start = (time_t)-1;
    time_t range_end = (time_t)-1;
    if (_timerange_to_time_t(tr, &midnight, range_start, range_end, tz)) {
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
                                                    bool notif_timeperiod,
                                                    const absl::TimeZone& tz) {
  timeperiod_manager::logger()->trace("get_next_valid_time_per_timeperiod()");

  // If no time can be found, the original preferred time will be set
  // in valid_time at the end of the loop.
  time_t original_preferred_time = preferred_time;

  // Loop through the upcoming year a day at a time.
  time_t earliest_time = (time_t)-1;
  time_info ti;
  ti.preferred_time = preferred_time;
  for (time_t in_one_year = ti.preferred_time + 366 * 24 * 60 * 60;
       (earliest_time == (time_t)-1) && (ti.preferred_time < in_one_year);) {
    // Compute time information.
    tm_from_time(ti.preferred_time, tz, &ti.preftime);
    ti.preftime.tm_sec = 0;
    ti.preftime.tm_min = 0;
    ti.preftime.tm_hour = 0;
    ti.preftime.tm_isdst = -1;
    ti.midnight = time_from_tm(&ti.preftime, tz);

    // Browse all date range types in precedence order.
    bool skip_this_day = false;
    for (uint32_t daterange_type = 0;
         daterange_type < DATERANGE_TYPES && earliest_time == (time_t)-1 &&
         !skip_this_day;
         ++daterange_type) {
      // Browse all date ranges of a given type. The earliest valid
      // time found in any date range will be valid.
      for (auto& dr : exceptions[daterange_type]) {
        // Get next range limits and check that we are within bounds.
        time_t daterange_start_time = (time_t)-1;
        time_t daterange_end_time = (time_t)-1;
        if (_daterange_to_time_t(dr, daterange_type, &ti, daterange_start_time,
                                 daterange_end_time, tz) &&
            ((daterange_start_time == (time_t)-1) ||
             (daterange_start_time <= ti.midnight)) &&
            ((daterange_end_time == (time_t)-1) ||
             (ti.midnight < daterange_end_time))) {
          // Only test today. An higher precedence exception might have
          // been skipped because it was not valid on the current day
          // but could be valid tomorrow.
          time_t potential_time = _get_next_valid_time_in_timeranges(
              ti.preferred_time, dr.get_timerange(), tz);

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
        time_t potential_time = _get_next_valid_time_in_timeranges(
            ti.preferred_time, this->days[ti.preftime.tm_wday], tz);
        if ((potential_time != (time_t)-1) &&
            ((earliest_time == (time_t)-1) || (potential_time < earliest_time)))
          earliest_time = potential_time;
      }
    }

    // Check exclusions.
    bool skipped = false;
    if (earliest_time != (time_t)-1) {
      time_t max_invalid = (time_t)-1;
      timeperiodexclusion tpe = std::move(this->get_exclusions());

      for (auto& [name, excluded] : tpe) {
        time_t invalid = excluded->get_next_invalid_time_per_timeperiod(
            earliest_time, notif_timeperiod, tz);
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
      ti.preferred_time = _add_round_days_to_midnight(ti.midnight, 1, tz);
  }

  // If we couldn't find a time period there must be none defined.
  if ((earliest_time == (time_t)-1) && !notif_timeperiod)
    *valid_time = original_preferred_time;
  // Else use the calculated time.
  else
    *valid_time = earliest_time;
  timeperiod_manager::logger()->trace(
      "get_next_valid_time_per_timeperiod {} valid_time={}", _name,
      *valid_time);
}

/**
 *  Given a preferred time, get the next valid time within this time period.
 *
 *  @param[in] pref_time  The preferred time to check.
 *  @param[in] tz         Timezone the period is evaluated in.
 *
 *  @return The next valid time (at or after now).
 */
time_t timeperiod::get_next_valid_time(time_t pref_time,
                                       const absl::TimeZone& tz) {
  timeperiod_manager::logger()->trace("get_next_valid_time()");

  // Preferred time must be now or in the future.
  time_t preferred_time = std::max(pref_time, time(NULL));

  // First check for possible timeperiod exclusions before getting a
  // valid_time.
  time_t valid_time = 0;
  get_next_valid_time_per_timeperiod(preferred_time, &valid_time, false, tz);
  return valid_time;
}

/**
 * Resolve the timeperiod object by checking all its contained pointers and
 * assigning them.
 *
 * @param w[out] Number of warnings produced during this resolution.
 * @param e[out] Number of errors produced during this resolution.
 *
 */
void timeperiod::resolve(const timeperiod_map& all,
                         uint32_t& w __attribute__((unused)),
                         uint32_t& e) {
  uint32_t errors = 0;

  // Check for illegal characters in timeperiod name.
  if (timeperiod_manager::contains_illegal_chars(_name)) {
    timeperiod_manager::logger()->error(
        "Error: The name of time period '{}' contains one or more illegal "
        "characters.",
        _name);
    errors++;
  }

  // Check for valid timeperiod names in exclusion list.
  for (auto& [name, excluded] : _exclusions) {
    auto found = all.find(name);

    if (found == all.end()) {
      timeperiod_manager::logger()->error(
          "Error: Excluded time period '{}' specified in timeperiod '{}' is "
          "not defined anywhere!",
          name, _name);
      errors++;
    } else {
      // Save the timeperiod pointer for later.
      excluded = found->second.get();
    }
  }

  // Add errors.
  if (errors) {
    e += errors;
    throw msg_fmt("Cannot resolve time period '{}'", _name);
  }
}

void timeperiod::set_days(
    const com::centreon::engine::configuration::DaysArray& array) {
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

}  // namespace com::centreon::common::timeperiods
