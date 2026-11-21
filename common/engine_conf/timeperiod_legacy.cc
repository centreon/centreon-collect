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
#include "common/engine_conf/timeperiod_legacy.hh"

#include <cstdio>
#include <cstring>

#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"

namespace com::centreon::engine::configuration {

namespace {

/**
 * @brief Convert an English month name to its 0-based index (january = 0).
 *
 * Switches on the first letter to narrow the search to that letter's group
 * (at most three candidates), then compares only within it — faster than a
 * full linear scan or a hash lookup at this size (benchmarked).
 *
 * @param[in]  name  Lowercase month name.
 * @param[out] id    The matched index; only meaningful when true is returned.
 *
 * @return true if @p name is a month name.
 */
bool month_id(std::string_view name, uint32_t& id) {
  if (name.empty())
    return false;
  switch (name[0]) {
    case 'j':
      if (name == "january") {
        id = 0;
        return true;
      }
      if (name == "june") {
        id = 5;
        return true;
      }
      if (name == "july") {
        id = 6;
        return true;
      }
      return false;
    case 'f':
      if (name == "february") {
        id = 1;
        return true;
      }
      return false;
    case 'm':
      if (name == "march") {
        id = 2;
        return true;
      }
      if (name == "may") {
        id = 4;
        return true;
      }
      return false;
    case 'a':
      if (name == "april") {
        id = 3;
        return true;
      }
      if (name == "august") {
        id = 7;
        return true;
      }
      return false;
    case 's':
      if (name == "september") {
        id = 8;
        return true;
      }
      return false;
    case 'o':
      if (name == "october") {
        id = 9;
        return true;
      }
      return false;
    case 'n':
      if (name == "november") {
        id = 10;
        return true;
      }
      return false;
    case 'd':
      if (name == "december") {
        id = 11;
        return true;
      }
      return false;
    default:
      return false;
  }
}

/**
 * @brief Convert an English weekday name to its index (sunday = 0 …
 *        saturday = 6).
 *
 * Same first-letter switch as month_id (at most two candidates per letter).
 *
 * @param[in]  name  Lowercase weekday name.
 * @param[out] id    The matched index; only meaningful when true is returned.
 *
 * @return true if @p name is a weekday name.
 */
bool weekday_id(std::string_view name, uint32_t& id) {
  if (name.empty())
    return false;
  switch (name[0]) {
    case 's':
      if (name == "sunday") {
        id = 0;
        return true;
      }
      if (name == "saturday") {
        id = 6;
        return true;
      }
      return false;
    case 'm':
      if (name == "monday") {
        id = 1;
        return true;
      }
      return false;
    case 't':
      if (name == "tuesday") {
        id = 2;
        return true;
      }
      if (name == "thursday") {
        id = 4;
        return true;
      }
      return false;
    case 'w':
      if (name == "wednesday") {
        id = 3;
        return true;
      }
      return false;
    case 'f':
      if (name == "friday") {
        id = 5;
        return true;
      }
      return false;
    default:
      return false;
  }
}

/**
 * @brief Convert "HH:MM" (with optional surrounding blanks) to seconds since
 *        midnight.
 *
 * @param[in]  text  The time string.
 * @param[out] ret   The resulting number of seconds.
 *
 * @return true on success, false if @p text is not a valid "HH:MM".
 */
bool time_to_seconds(std::string_view text, uint64_t& ret) {
  size_t colon = text.find(':');
  if (colon == std::string_view::npos)
    return false;
  std::string_view h = text.substr(0, colon);
  std::string_view m = text.substr(colon + 1);
  uint32_t hours = 0;
  uint32_t minutes = 0;
  if (!absl::SimpleAtoi(h, &hours) || !absl::SimpleAtoi(m, &minutes))
    return false;
  ret = hours * 3600ull + minutes * 60ull;
  return true;
}

/**
 * @brief Get the exceptions list of @p tp matching a date-range type.
 *
 * @param[in,out] tp    Timeperiod whose exceptions are targeted.
 * @param[in]     type  The date-range type.
 *
 * @return The repeated field to append to, or nullptr for an unhandled type.
 */
::google::protobuf::RepeatedPtrField<Daterange>* exception_dest(
    Timeperiod& tp,
    Daterange::TypeRange type) {
  ExceptionArray* ex = tp.mutable_exceptions();
  switch (type) {
    case Daterange::calendar_date:
      return ex->mutable_calendar_date();
    case Daterange::month_date:
      return ex->mutable_month_date();
    case Daterange::month_day:
      return ex->mutable_month_day();
    case Daterange::month_week_day:
      return ex->mutable_month_week_day();
    case Daterange::week_day:
      return ex->mutable_week_day();
    default:
      return nullptr;
  }
}

/**
 * @brief Get the time-range list of a weekday in a DaysArray.
 *
 * @param[in,out] days  The DaysArray holding the seven weekdays.
 * @param[in]     day   Day index, 0 = Sunday … 6 = Saturday.
 *
 * @return The repeated field for that day, or nullptr if @p day is out of
 * range.
 */
::google::protobuf::RepeatedPtrField<Timerange>* weekday_field(DaysArray* days,
                                                               int day) {
  switch (day) {
    case 0:
      return days->mutable_sunday();
    case 1:
      return days->mutable_monday();
    case 2:
      return days->mutable_tuesday();
    case 3:
      return days->mutable_wednesday();
    case 4:
      return days->mutable_thursday();
    case 5:
      return days->mutable_friday();
    case 6:
      return days->mutable_saturday();
    default:
      return nullptr;
  }
}

/**
 * @brief Parse a calendar-date exception line and append it to @p tp.
 *
 * Handles "2009-08-11", an optional end date ("… - 2009-08-20") and an optional
 * skip interval ("… / 2").
 *
 * @param[in]     line  The full exception line (date part + time-range part).
 * @param[in,out] tp    Timeperiod whose exceptions are appended to.
 *
 * @return true if @p line is a calendar-date exception, false otherwise.
 */
bool build_calendar_date(const std::string& line, Timeperiod& tp) {
  int ret = 0;
  int pos = 0;
  uint32_t year_start = 0, month_start = 0, month_day_start = 0;
  uint32_t year_end = 0, month_end = 0, month_day_end = 0;
  uint32_t skip_interval = 0;

  if ((ret = sscanf(line.c_str(), "%4u-%2u-%2u - %4u-%2u-%2u / %9u %n",
                    &year_start, &month_start, &month_day_start, &year_end,
                    &month_end, &month_day_end, &skip_interval, &pos)) == 7)
    ;
  else if ((ret = sscanf(line.c_str(), "%4u-%2u-%2u - %4u-%2u-%2u %n",
                         &year_start, &month_start, &month_day_start, &year_end,
                         &month_end, &month_day_end, &pos)) == 6)
    ;
  else if ((ret = sscanf(line.c_str(), "%4u-%2u-%2u / %9u %n", &year_start,
                         &month_start, &month_day_start, &skip_interval,
                         &pos)) == 4) {
    year_end = 0;
    month_end = 0;
    month_day_end = 0;
  } else if ((ret = sscanf(line.c_str(), "%4u-%2u-%2u %n", &year_start,
                           &month_start, &month_day_start, &pos)) == 3) {
    year_end = year_start;
    month_end = month_start;
    month_day_end = month_day_start;
  }

  if (!ret)
    return false;

  Daterange* range = exception_dest(tp, Daterange::calendar_date)->Add();
  range->set_type(Daterange::calendar_date);
  range->set_syear(year_start);
  range->set_smon(static_cast<int>(month_start) - 1);  // 0-based month.
  range->set_smday(month_day_start);
  range->set_eyear(year_end);
  range->set_emon(static_cast<int>(month_end) - 1);
  range->set_emday(month_day_end);
  range->set_skip_interval(skip_interval);
  return legacy_build_timeranges(line.substr(pos), range->mutable_timerange());
}

/**
 * @brief Parse a non-calendar exception line and append it to @p tp.
 *
 * Recognises the month_date, month_day, month_week_day and week_day grammars
 * (e.g. "monday 4 january", "day 3", "thursday 2 - 4", "february 1 - march
 * 15"), each with an optional skip interval.
 *
 * @param[in]     line  The full exception line (date part + time-range part).
 * @param[in,out] tp    Timeperiod whose exceptions are appended to.
 *
 * @return true if @p line matched one of the grammars, false otherwise.
 */
bool build_other_date(const std::string& line, Timeperiod& tp) {
  if (line.size() > 1024)
    return false;

  int pos = 0;
  Daterange::TypeRange type = Daterange::none;
  uint32_t month_start = 0, month_end = 0;
  int month_day_start = 0, month_day_end = 0;
  uint32_t skip_interval = 0;
  uint32_t week_day_start = 0, week_day_end = 0;
  int week_day_start_offset = 0, week_day_end_offset = 0;
  char buffer[4][1024];

  if (sscanf(line.c_str(),
             "%1023[a-z] %9d %1023[a-z] - %1023[a-z] %9d %1023[a-z] / %9u %n",
             buffer[0], &week_day_start_offset, buffer[1], buffer[2],
             &week_day_end_offset, buffer[3], &skip_interval, &pos) == 7) {
    // wednesday 1 january - thursday 2 july / 3
    if (weekday_id(buffer[0], week_day_start) &&
        month_id(buffer[1], month_start) &&
        weekday_id(buffer[2], week_day_end) && month_id(buffer[3], month_end))
      type = Daterange::month_week_day;
  } else if (sscanf(line.c_str(), "%1023[a-z] %9d - %1023[a-z] %9d / %9u %n",
                    buffer[0], &month_day_start, buffer[1], &month_day_end,
                    &skip_interval, &pos) == 5) {
    // monday 2 - thursday 3 / 2
    if (weekday_id(buffer[0], week_day_start) &&
        weekday_id(buffer[1], week_day_end)) {
      week_day_start_offset = month_day_start;
      week_day_end_offset = month_day_end;
      type = Daterange::week_day;
    }
    // february 1 - march 15 / 3
    else if (month_id(buffer[0], month_start) && month_id(buffer[1], month_end))
      type = Daterange::month_date;
    // day 4 - 6 / 2
    else if (!strcmp(buffer[0], "day") && !strcmp(buffer[1], "day"))
      type = Daterange::month_day;
  } else if (sscanf(line.c_str(), "%1023[a-z] %9d - %9d / %9u %n", buffer[0],
                    &month_day_start, &month_day_end, &skip_interval,
                    &pos) == 4) {
    // thursday 2 - 4 / 2
    if (weekday_id(buffer[0], week_day_start)) {
      week_day_start_offset = month_day_start;
      week_day_end = week_day_start;
      week_day_end_offset = month_day_end;
      type = Daterange::week_day;
    }
    // february 3 - 5 / 2
    else if (month_id(buffer[0], month_start)) {
      month_end = month_start;
      type = Daterange::month_date;
    }
    // day 1 - 4 / 2
    else if (!strcmp(buffer[0], "day"))
      type = Daterange::month_day;
  } else if (sscanf(line.c_str(),
                    "%1023[a-z] %9d %1023[a-z] - %1023[a-z] %9d %1023[a-z] %n",
                    buffer[0], &week_day_start_offset, buffer[1], buffer[2],
                    &week_day_end_offset, buffer[3], &pos) == 6) {
    // wednesday 1 january - thursday 2 july
    if (weekday_id(buffer[0], week_day_start) &&
        month_id(buffer[1], month_start) &&
        weekday_id(buffer[2], week_day_end) && month_id(buffer[3], month_end))
      type = Daterange::month_week_day;
  } else if (sscanf(line.c_str(), "%1023[a-z] %9d - %9d %n", buffer[0],
                    &month_day_start, &month_day_end, &pos) == 3) {
    // thursday 2 - 4
    if (weekday_id(buffer[0], week_day_start)) {
      week_day_start_offset = month_day_start;
      week_day_end = week_day_start;
      week_day_end_offset = month_day_end;
      type = Daterange::week_day;
    }
    // february 3 - 5
    else if (month_id(buffer[0], month_start)) {
      month_end = month_start;
      type = Daterange::month_date;
    }
    // day 1 - 4
    else if (!strcmp(buffer[0], "day"))
      type = Daterange::month_day;
  } else if (sscanf(line.c_str(), "%1023[a-z] %9d - %1023[a-z] %9d %n",
                    buffer[0], &month_day_start, buffer[1], &month_day_end,
                    &pos) == 4) {
    // monday 2 - thursday 3
    if (weekday_id(buffer[0], week_day_start) &&
        weekday_id(buffer[1], week_day_end)) {
      week_day_start_offset = month_day_start;
      week_day_end_offset = month_day_end;
      type = Daterange::week_day;
    }
    // february 1 - march 15
    else if (month_id(buffer[0], month_start) && month_id(buffer[1], month_end))
      type = Daterange::month_date;
    // day 1 - day 5
    else if (!strcmp(buffer[0], "day") && !strcmp(buffer[1], "day"))
      type = Daterange::month_day;
  } else if (sscanf(line.c_str(), "%1023[a-z] %9d %1023[a-z] %n", buffer[0],
                    &week_day_start_offset, buffer[1], &pos) == 3) {
    // thursday 3 february
    if (weekday_id(buffer[0], week_day_start) &&
        month_id(buffer[1], month_start)) {
      month_end = month_start;
      week_day_end = week_day_start;
      week_day_end_offset = week_day_start_offset;
      type = Daterange::month_week_day;
    }
  } else if (sscanf(line.c_str(), "%1023[a-z] %9d %n", buffer[0],
                    &month_day_start, &pos) == 2) {
    // thursday 2
    if (weekday_id(buffer[0], week_day_start)) {
      week_day_start_offset = month_day_start;
      week_day_end = week_day_start;
      week_day_end_offset = week_day_start_offset;
      type = Daterange::week_day;
    }
    // february 3
    else if (month_id(buffer[0], month_start)) {
      month_end = month_start;
      month_day_end = month_day_start;
      type = Daterange::month_date;
    }
    // day 1
    else if (!strcmp(buffer[0], "day")) {
      month_day_end = month_day_start;
      type = Daterange::month_day;
    }
  }

  if (type == Daterange::none)
    return false;

  Daterange* range = exception_dest(tp, type)->Add();
  range->set_type(type);
  if (type == Daterange::month_day) {
    range->set_smday(month_day_start);
    range->set_emday(month_day_end);
  } else if (type == Daterange::month_week_day) {
    range->set_smon(month_start);
    range->set_swday(week_day_start);
    range->set_swday_offset(week_day_start_offset);
    range->set_emon(month_end);
    range->set_ewday(week_day_end);
    range->set_ewday_offset(week_day_end_offset);
  } else if (type == Daterange::week_day) {
    range->set_swday(week_day_start);
    range->set_swday_offset(week_day_start_offset);
    range->set_ewday(week_day_end);
    range->set_ewday_offset(week_day_end_offset);
  } else if (type == Daterange::month_date) {
    range->set_smon(month_start);
    range->set_smday(month_day_start);
    range->set_emon(month_end);
    range->set_emday(month_day_end);
  }
  range->set_skip_interval(skip_interval);
  return legacy_build_timeranges(line.substr(pos), range->mutable_timerange());
}

}  // namespace

bool legacy_build_timeranges(
    const std::string& text,
    ::google::protobuf::RepeatedPtrField<Timerange>* out) {
  if (text.empty())
    return true;
  for (std::string_view t : absl::StrSplit(text, ',')) {
    size_t dash = t.find('-');
    if (dash == std::string_view::npos)
      return false;
    uint64_t start = 0;
    uint64_t end = 0;
    if (!time_to_seconds(t.substr(0, dash), start) ||
        !time_to_seconds(t.substr(dash + 1), end))
      return false;
    Timerange* tr = out->Add();
    tr->set_range_start(start);
    tr->set_range_end(end);
  }
  return true;
}

bool legacy_set_weekday(Timeperiod& tp, int day, const std::string& text) {
  ::google::protobuf::RepeatedPtrField<Timerange>* field =
      weekday_field(tp.mutable_timeranges(), day);
  if (!field)
    return false;
  return legacy_build_timeranges(text, field);
}

bool legacy_add_exception(Timeperiod& tp,
                          const std::string& daterange_text,
                          const std::string& timerange_text) {
  // Mirror broker: concatenate the two parts and parse them as one line.
  std::string line = daterange_text + " " + timerange_text;
  return build_calendar_date(line, tp) || build_other_date(line, tp);
}

}  // namespace com::centreon::engine::configuration
