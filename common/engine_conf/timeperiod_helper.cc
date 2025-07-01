/**
 * Copyright 2022-2024 Centreon (https://www.centreon.com/)
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
#include "common/engine_conf/timeperiod_helper.hh"

#include <absl/strings/str_format.h>
#include <fmt/ranges.h>

#include "com/centreon/exceptions/msg_fmt.hh"

using com::centreon::exceptions::msg_fmt;

namespace com::centreon::engine::configuration {

/**
 * @brief Constructor from a Timeperiod object.
 *
 * @param obj The Timeperiod object on which this helper works. The helper is
 * not the owner of this object.
 */
timeperiod_helper::timeperiod_helper(Timeperiod* obj)
    : message_helper(object_type::timeperiod,
                     obj,
                     {},
                     Timeperiod::descriptor()->field_count()) {
  _init();
}

/**
 * @brief For several keys, the parser of Timeperiod objects has a particular
 *        behavior. These behaviors are handled here.
 * @param key The key to parse.
 * @param value The value corresponding to the key
 */
bool timeperiod_helper::hook(std::string_view key, std::string_view value) {
  Timeperiod* obj = static_cast<Timeperiod*>(mut_obj());
  /* Since we use key to get back the good key value, it is faster to give key
   * by copy to the method. We avoid one key allocation... */
  key = validate_key(key);
  auto get_timerange = [](const std::string_view& value, auto* day) -> bool {
    auto arr = absl::StrSplit(value, ',');
    for (auto& d : arr) {
      std::pair<std::string_view, std::string_view> v = absl::StrSplit(d, '-');
      Timerange tr;
      std::pair<std::string_view, std::string_view> p =
          absl::StrSplit(v.first, ':');
      uint32_t h, m;
      if (!absl::SimpleAtoi(p.first, &h) || !absl::SimpleAtoi(p.second, &m))
        return false;
      tr.set_range_start(h * 3600 + m * 60);
      p = absl::StrSplit(v.second, ':');
      if (!absl::SimpleAtoi(p.first, &h) || !absl::SimpleAtoi(p.second, &m))
        return false;
      tr.set_range_end(h * 3600 + m * 60);
      day->Add(std::move(tr));
    }
    return true;
  };

  bool retval = false;
  if (key == "exclude") {
    fill_string_group(obj->mutable_exclude(), value);
    return true;
  } else {
    if (key == "sunday")
      retval =
          get_timerange(value, obj->mutable_timeranges()->mutable_sunday());
    else if (key == "monday")
      retval =
          get_timerange(value, obj->mutable_timeranges()->mutable_monday());
    else if (key == "tuesday")
      retval =
          get_timerange(value, obj->mutable_timeranges()->mutable_tuesday());
    else if (key == "wednesday")
      retval =
          get_timerange(value, obj->mutable_timeranges()->mutable_wednesday());
    else if (key == "thursday")
      retval =
          get_timerange(value, obj->mutable_timeranges()->mutable_thursday());
    else if (key == "friday")
      retval =
          get_timerange(value, obj->mutable_timeranges()->mutable_friday());
    else if (key == "saturday")
      retval =
          get_timerange(value, obj->mutable_timeranges()->mutable_saturday());
    if (!retval) {
      std::string line{absl::StrFormat("%s %s", key, value)};
      retval = _add_week_day(key, value) || _add_calendar_date(line) ||
               _add_other_date(line);
    }
  }
  return retval;
}

/**
 *  Add a week day.
 *
 *  @param[in] key   The week day.
 *  @param[in] value The range.
 *
 *  @return True on success, otherwise false.
 */
bool timeperiod_helper::_add_week_day(std::string_view key,
                                      std::string_view value) {
  Timeperiod* obj = static_cast<Timeperiod*>(mut_obj());
  unsigned int day_id;
  if (!_get_day_id(key, day_id))
    return false;

  google::protobuf::RepeatedPtrField<configuration::Timerange>* d;
  switch (day_id) {
    case 0:
      d = obj->mutable_timeranges()->mutable_sunday();
      break;
    case 1:
      d = obj->mutable_timeranges()->mutable_sunday();
      break;
    case 2:
      d = obj->mutable_timeranges()->mutable_sunday();
      break;
    case 3:
      d = obj->mutable_timeranges()->mutable_sunday();
      break;
    case 4:
      d = obj->mutable_timeranges()->mutable_sunday();
      break;
    case 5:
      d = obj->mutable_timeranges()->mutable_sunday();
      break;
    case 6:
      d = obj->mutable_timeranges()->mutable_sunday();
      break;
  }
  if (!_build_timeranges(value, *d))
    return false;

  return true;
}

/**
 *  Add a calendar date.
 *
 *  @param[in] line The line to parse.
 *
 *  @return True on success, otherwise false.
 */
bool timeperiod_helper::_add_calendar_date(const std::string& line) {
  int32_t ret = 0;
  int32_t pos = 0;
  bool fill_missing = false;
  uint32_t month_start = 0;
  uint32_t month_end = 0;
  uint32_t month_day_start = 0;
  uint32_t month_day_end = 0;
  uint32_t year_start = 0;
  uint32_t year_end = 0;
  uint32_t skip_interval = 0;

  if ((ret = sscanf(line.c_str(), "%4u-%2u-%2u - %4u-%2u-%2u / %u %n",
                    &year_start, &month_start, &month_day_start, &year_end,
                    &month_end, &month_day_end, &skip_interval, &pos)) == 7)
    fill_missing = false;
  else if ((ret = sscanf(line.c_str(), "%4u-%2u-%2u - %4u-%2u-%2u %n",
                         &year_start, &month_start, &month_day_start, &year_end,
                         &month_end, &month_day_end, &pos)) == 6)
    fill_missing = false;
  else if ((ret = sscanf(line.c_str(), "%4u-%2u-%2u / %u %n", &year_start,
                         &month_start, &month_day_start, &skip_interval,
                         &pos)) == 4)
    fill_missing = true;
  else if ((ret = sscanf(line.c_str(), "%4u-%2u-%2u %n", &year_start,
                         &month_start, &month_day_start, &pos)) == 3)
    fill_missing = true;

  if (ret) {
    if (fill_missing) {
      year_end = year_start;
      month_end = month_start;
      month_day_end = month_day_start;
    }

    Timeperiod* obj = static_cast<Timeperiod*>(mut_obj());
    auto* range = obj->mutable_exceptions()->add_calendar_date();
    range->set_type(Daterange_TypeRange_calendar_date);
    if (!_build_timeranges(line.substr(pos), *range->mutable_timerange()))
      return false;

    range->set_syear(year_start);
    range->set_smon(month_start - 1);
    range->set_smday(month_day_start);
    range->set_eyear(year_end);
    range->set_emon(month_end - 1);
    range->set_emday(month_day_end);
    range->set_skip_interval(skip_interval);

    return true;
  }
  return false;
}

/**
 *  Build timerange from new line.
 *
 *  @param[in]  line       The line to parse.
 *  @param[out] timeranges The list to fill.
 *
 *  @return True on success, otherwise false.
 */
bool timeperiod_helper::_build_timeranges(
    std::string_view line,
    google::protobuf::RepeatedPtrField<Timerange>& timeranges) {
  auto timeranges_str = absl::StrSplit(line, ',');
  for (auto tr : timeranges_str) {
    tr = absl::StripAsciiWhitespace(tr);
    std::size_t pos(tr.find('-'));
    if (pos == std::string::npos)
      return false;
    time_t start_time;
    if (!_build_time_t(tr.substr(0, pos), start_time))
      return false;
    time_t end_time;
    if (!_build_time_t(tr.substr(pos + 1), end_time))
      return false;
    Timerange* t = timeranges.Add();
    t->set_range_start(start_time);
    t->set_range_end(end_time);
  }
  return true;
}

/**
 *  Build time_t from timerange configuration.
 *
 *  @param[in]  time_str The time to parse (format 00:00-12:00).
 *  @param[out] ret      The value to fill.
 *
 *  @return True on success, otherwise false.
 */
bool timeperiod_helper::_build_time_t(std::string_view time_str, time_t& ret) {
  std::size_t pos(time_str.find(':'));
  if (pos == std::string::npos)
    return false;
  unsigned long hours;
  if (!absl::SimpleAtoi(time_str.substr(0, pos), &hours))
    return false;
  unsigned long minutes;
  if (!absl::SimpleAtoi(time_str.substr(pos + 1), &minutes))
    return false;
  ret = hours * 3600 + minutes * 60;
  return true;
}

/**
 * @brief Check the validity of the Timeperiod object.
 *
 * @param err An error counter.
 */
void timeperiod_helper::check_validity(error_cnt& err) const {
  const Timeperiod* o = static_cast<const Timeperiod*>(obj());

  if (o->timeperiod_name().empty()) {
    err.config_errors++;
    throw msg_fmt("Time period has no name (property 'timeperiod_name')");
  }
}

void timeperiod_helper::_init() {
  Timeperiod* obj = static_cast<Timeperiod*>(mut_obj());
  obj->mutable_obj()->set_register_(true);
}

/**
 *  Add other date.
 *
 *  @param[in] line The line to parse.
 *
 *  @return True on success, otherwise false.
 */
bool timeperiod_helper::_add_other_date(const std::string& line) {
  int pos = 0;
  Daterange::TypeRange type = Daterange_TypeRange_none;
  uint32_t month_start = 0;
  uint32_t month_end = 0;
  int32_t month_day_start = 0;
  int32_t month_day_end = 0;
  uint32_t skip_interval = 0;
  uint32_t week_day_start = 0;
  uint32_t week_day_end = 0;
  int32_t week_day_start_offset = 0;
  int32_t week_day_end_offset = 0;
  char buffer[4][4096];

  if (line.size() > 1024)
    return false;

  if (sscanf(line.c_str(), "%[a-z] %d %[a-z] - %[a-z] %d %[a-z] / %u %n",
             buffer[0], &week_day_start_offset, buffer[1], buffer[2],
             &week_day_end_offset, buffer[3], &skip_interval, &pos) == 7) {
    // wednesday 1 january - thursday 2 july / 3
    if (_get_day_id(buffer[0], week_day_start) &&
        _get_month_id(buffer[1], month_start) &&
        _get_day_id(buffer[2], week_day_end) &&
        _get_month_id(buffer[3], month_end))
      type = Daterange_TypeRange_month_week_day;
  } else if (sscanf(line.c_str(), "%[a-z] %d - %[a-z] %d / %u %n", buffer[0],
                    &month_day_start, buffer[1], &month_day_end, &skip_interval,
                    &pos) == 5) {
    // monday 2 - thursday 3 / 2
    if (_get_day_id(buffer[0], week_day_start) &&
        _get_day_id(buffer[1], week_day_end)) {
      week_day_start_offset = month_day_start;
      week_day_end_offset = month_day_end;
      type = Daterange_TypeRange_week_day;
    }
    // february 1 - march 15 / 3
    else if (_get_month_id(buffer[0], month_start) &&
             _get_month_id(buffer[1], month_end))
      type = Daterange_TypeRange_month_date;
    // day 4 - 6 / 2
    else if (!strcmp(buffer[0], "day") && !strcmp(buffer[1], "day"))
      type = Daterange_TypeRange_month_day;
  } else if (sscanf(line.c_str(), "%[a-z] %d - %d / %u %n", buffer[0],
                    &month_day_start, &month_day_end, &skip_interval,
                    &pos) == 4) {
    // thursday 2 - 4
    if (_get_day_id(buffer[0], week_day_start)) {
      week_day_start_offset = month_day_start;
      week_day_end = week_day_start;
      week_day_end_offset = month_day_end;
      type = Daterange_TypeRange_week_day;
    }
    // february 3 - 5
    else if (_get_month_id(buffer[0], month_start)) {
      month_end = month_start;
      type = Daterange_TypeRange_month_date;
    }
    // day 1 - 4
    else if (!strcmp(buffer[0], "day"))
      type = Daterange_TypeRange_month_day;
  } else if (sscanf(line.c_str(), "%[a-z] %d %[a-z] - %[a-z] %d %[a-z] %n",
                    buffer[0], &week_day_start_offset, buffer[1], buffer[2],
                    &week_day_end_offset, buffer[3], &pos) == 6) {
    // wednesday 1 january - thursday 2 july
    if (_get_day_id(buffer[0], week_day_start) &&
        _get_month_id(buffer[1], month_start) &&
        _get_day_id(buffer[2], week_day_end) &&
        _get_month_id(buffer[3], month_end))
      type = Daterange_TypeRange_month_week_day;
  } else if (sscanf(line.c_str(), "%[a-z] %d - %d %n", buffer[0],
                    &month_day_start, &month_day_end, &pos) == 3) {
    // thursday 2 - 4
    if (_get_day_id(buffer[0], week_day_start)) {
      week_day_start_offset = month_day_start;
      week_day_end = week_day_start;
      week_day_end_offset = month_day_end;
      type = Daterange_TypeRange_week_day;
    }
    // february 3 - 5
    else if (_get_month_id(buffer[0], month_start)) {
      month_end = month_start;
      type = Daterange_TypeRange_month_date;
    }
    // day 1 - 4
    else if (!strcmp(buffer[0], "day"))
      type = Daterange_TypeRange_month_day;
  } else if (sscanf(line.c_str(), "%[a-z] %d - %[a-z] %d %n", buffer[0],
                    &month_day_start, buffer[1], &month_day_end, &pos) == 4) {
    // monday 2 - thursday 3
    if (_get_day_id(buffer[0], week_day_start) &&
        _get_day_id(buffer[1], week_day_end)) {
      week_day_start_offset = month_day_start;
      week_day_end_offset = month_day_end;
      type = Daterange_TypeRange_week_day;
    }
    // february 1 - march 15
    else if (_get_month_id(buffer[0], month_start) &&
             _get_month_id(buffer[1], month_end))
      type = Daterange_TypeRange_month_date;
    // day 1 - day 5
    else if (!strcmp(buffer[0], "day") && !strcmp(buffer[1], "day"))
      type = Daterange_TypeRange_month_day;
  } else if (sscanf(line.c_str(), "%[a-z] %d %[a-z] %n", buffer[0],
                    &week_day_start_offset, buffer[1], &pos) == 3) {
    // thursday 3 february
    if (_get_day_id(buffer[0], week_day_start) &&
        _get_month_id(buffer[1], month_start)) {
      month_end = month_start;
      week_day_end = week_day_start;
      week_day_end_offset = week_day_start_offset;
      type = Daterange_TypeRange_month_week_day;
    }
  } else if (sscanf(line.c_str(), "%[a-z] %d %n", buffer[0], &month_day_start,
                    &pos) == 2) {
    // thursday 2
    if (_get_day_id(buffer[0], week_day_start)) {
      week_day_start_offset = month_day_start;
      week_day_end = week_day_start;
      week_day_end_offset = week_day_start_offset;
      type = Daterange_TypeRange_week_day;
    }
    // february 3
    else if (_get_month_id(buffer[0], month_start)) {
      month_end = month_start;
      month_day_end = month_day_start;
      type = Daterange_TypeRange_month_date;
    }
    // day 1
    else if (!strcmp(buffer[0], "day")) {
      month_day_end = month_day_start;
      type = Daterange_TypeRange_month_day;
    }
  }

  if (type != Daterange_TypeRange_none) {
    Timeperiod* obj = static_cast<Timeperiod*>(mut_obj());
    Daterange* range = nullptr;
    switch (type) {
      case Daterange_TypeRange_month_day: {
        range = obj->mutable_exceptions()->add_month_day();
        range->set_type(type);
        range->set_smday(month_day_start);
        range->set_emday(month_day_end);
      } break;
      case Daterange_TypeRange_month_week_day: {
        range = obj->mutable_exceptions()->add_month_week_day();
        range->set_type(type);
        range->set_smon(month_start);
        range->set_swday(week_day_start);
        range->set_swday_offset(week_day_start_offset);
        range->set_emon(month_end);
        range->set_ewday(week_day_end);
        range->set_ewday_offset(week_day_end_offset);
      } break;
      case Daterange_TypeRange_week_day: {
        range = obj->mutable_exceptions()->add_week_day();
        range->set_type(type);
        range->set_swday(week_day_start);
        range->set_swday_offset(week_day_start_offset);
        range->set_ewday(week_day_end);
        range->set_ewday_offset(week_day_end_offset);
      } break;
      case Daterange_TypeRange_month_date: {
        range = obj->mutable_exceptions()->add_month_date();
        range->set_type(type);
        range->set_smon(month_start);
        range->set_smday(month_day_start);
        range->set_emon(month_end);
        range->set_emday(month_day_end);
      } break;
      default:
        return false;
    }
    range->set_skip_interval(skip_interval);

    if (!_build_timeranges(line.substr(pos), *range->mutable_timerange()))
      return false;

    return true;
  }
  return false;
}

/**
 *  Get the week day id.
 *
 *  @param[in]  name The week day name.
 *  @param[out] id   The id to fill.
 *
 *  @return True on success, otherwise false.
 */
bool timeperiod_helper::_get_day_id(std::string_view name, uint32_t& id) {
  static const absl::flat_hash_map<std::string_view, uint32_t> days = {
      {"sunday", 0},   {"monday", 1}, {"tuesday", 2},  {"wednesday", 3},
      {"thursday", 4}, {"friday", 5}, {"saturday", 6},
  };
  auto found = days.find(name);
  if (found != days.end()) {
    id = found->second;
    return true;
  } else
    return false;
}

/**
 *  Get the month id.
 *
 *  @param[in]  name The month name.
 *  @param[out] id   The id to fill.
 *
 *  @return True on success, otherwise false.
 */
bool timeperiod_helper::_get_month_id(std::string_view name, uint32_t& id) {
  static const absl::flat_hash_map<std::string_view, uint32_t> months = {
      {"january", 0},   {"february", 1}, {"march", 2},     {"april", 3},
      {"may", 4},       {"june", 5},     {"july", 6},      {"august", 7},
      {"september", 8}, {"october", 9},  {"november", 10}, {"december", 11},
  };
  auto found = months.find(name);
  if (found != months.end()) {
    id = found->second;
    return true;
  } else
    return false;
}

std::string daterange_to_str(const Daterange& dr) {
  static const std::array<std::string_view, 7> days{
      "sunday",   "monday", "tuesday", "wednesday",
      "thursday", "friday", "saturday"};
  static const std::array<std::string_view, 12> months{
      "january", "february", "march",     "april",   "may",      "june",
      "july",    "august",   "september", "october", "november", "december"};
  std::string retval;
  switch (dr.type()) {
    case Daterange_TypeRange_calendar_date: {
      std::string retval = fmt::format("{:02}-{:02}-{:02}", dr.syear(),
                                       dr.smon() + 1, dr.smday());
      if (dr.syear() != dr.eyear() || dr.smon() != dr.emon() ||
          dr.smday() != dr.emday())
        retval = fmt::format("{} - {:02}-{:02}-{:02} / {}", retval, dr.eyear(),
                             dr.emon() + 1, dr.emday(), dr.skip_interval());
    } break;
    case Daterange_TypeRange_month_date: {
      retval = fmt::format("{} {}", months[dr.smon()], dr.smday());
      if (dr.smon() != dr.emon())
        retval =
            fmt::format("{} - {} {}", retval, months[dr.emon()], dr.emday());
      else if (dr.smday() != dr.emday())
        retval = fmt::format("{} - {}", retval, dr.emday());
      if (dr.skip_interval())
        retval = fmt::format("{} / {}", retval, dr.skip_interval());
    } break;
    case Daterange_TypeRange_month_day: {
      retval = fmt::format("day {}", dr.smday());
      if (dr.smday() != dr.emday())
        retval = fmt::format("{} - {}", retval, dr.emday());
      if (dr.skip_interval())
        retval = fmt::format("{} / {}", retval, dr.skip_interval());
    } break;
    case Daterange_TypeRange_month_week_day: {
      retval = fmt::format("{} {} {}", days[dr.swday()], dr.swday_offset(),
                           months[dr.smon()]);
      if (dr.swday() != dr.ewday() || dr.swday_offset() != dr.ewday_offset() ||
          dr.smon() != dr.emon())
        retval = fmt::format("{} - {} {} {}", retval, days[dr.ewday()],
                             dr.ewday_offset(), months[dr.emon()]);
      if (dr.skip_interval())
        retval = fmt::format("{} / {}", dr.skip_interval());
    } break;
    case Daterange_TypeRange_week_day: {
      retval = fmt::format("{} {}", days[dr.swday()], dr.swday_offset());
      if (dr.swday() != dr.ewday() || dr.swday_offset() != dr.ewday_offset())
        retval = fmt::format("{} - {} {}", retval, days[dr.ewday()],
                             dr.ewday_offset());
      if (dr.skip_interval())
        retval = fmt::format("{} / {}", retval, dr.skip_interval());
    } break;
    default:
      assert("should not arrive" == nullptr);
  }
  std::vector<std::string> timeranges_str;
  for (auto& t : dr.timerange()) {
    uint32_t start_hours(t.range_start() / 3600);
    uint32_t start_minutes((t.range_start() % 3600) / 60);
    uint32_t end_hours(t.range_end() / 3600);
    uint32_t end_minutes((t.range_end() % 3600) / 60);
    timeranges_str.emplace_back(fmt::format("{:02}:{:02}-{:02}:{:02}",
                                            start_hours, start_minutes,
                                            end_hours, end_minutes));
  }
  retval = fmt::format("{} {}", retval, fmt::join(timeranges_str, ", "));
  return retval;
}
}  // namespace com::centreon::engine::configuration
