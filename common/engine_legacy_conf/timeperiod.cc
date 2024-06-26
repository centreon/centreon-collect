/**
 * Copyright 2011-2013 Merethis
 * Copyright 2017-2024 Centreon
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
#include "timeperiod.hh"

#include "com/centreon/exceptions/msg_fmt.hh"
#include "daterange.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;
using com::centreon::exceptions::msg_fmt;

#define SETTER(type, method) \
  &object::setter<timeperiod, type, &timeperiod::method>::generic

std::unordered_map<std::string, timeperiod::setter_func> const
    timeperiod::_setters{
        {"alias", SETTER(std::string const&, _set_alias)},
        {"exclude", SETTER(std::string const&, _set_exclude)},
        {"timeperiod_name", SETTER(std::string const&, _set_timeperiod_name)}};

/**
 *  Constructor.
 *
 *  @param[in] key The object key.
 */
timeperiod::timeperiod(key_type const& key)
    : object(object::timeperiod), _timeperiod_name(key) {}

/**
 *  Copy constructor.
 *
 *  @param[in] right The timeperiod to copy.
 */
timeperiod::timeperiod(timeperiod const& right) : object(right) {
  operator=(right);
}

/**
 *  Destructor.
 */
timeperiod::~timeperiod() noexcept {}

/**
 *  Copy constructor.
 *
 *  @param[in] right The timeperiod to copy.
 *
 *  @return This timeperiod.
 */
timeperiod& timeperiod::operator=(timeperiod const& right) {
  if (this != &right) {
    object::operator=(right);
    _alias = right._alias;
    _exceptions = right._exceptions;
    _exclude = right._exclude;
    _timeperiod_name = right._timeperiod_name;
    _timeranges = right._timeranges;
  }
  return *this;
}

/**
 *  Equal operator.
 *
 *  @param[in] right The timeperiod to compare.
 *
 *  @return True if is the same timeperiod, otherwise false.
 */
bool timeperiod::operator==(timeperiod const& right) const {
  return object::operator==(right) && _alias == right._alias &&
         _exceptions == right._exceptions && _exclude == right._exclude &&
         _timeperiod_name == right._timeperiod_name &&
         _timeranges == right._timeranges;
}

/**
 *  Equal operator.
 *
 *  @param[in] right The timeperiod to compare.
 *
 *  @return True if is not the same timeperiod, otherwise false.
 */
bool timeperiod::operator!=(timeperiod const& right) const {
  return !operator==(right);
}

/**
 *  Less-than operator.
 *
 *  @param[in] right Object to compare to.
 *
 *  @return True if this object is less than right.
 */
bool timeperiod::operator<(timeperiod const& right) const {
  if (_timeperiod_name != right._timeperiod_name)
    return _timeperiod_name < right._timeperiod_name;
  else if (_alias != right._alias)
    return _alias < right._alias;
  else if (_exclude != right._exclude)
    return _exclude < right._exclude;
  else if (_timeranges != right._timeranges)
    return _timeranges < right._timeranges;
  return _exceptions < right._exceptions;
}

/**
 *  @brief Check if the object is valid.
 *
 *  If the object is not valid, an exception is thrown.
 */
void timeperiod::check_validity(error_cnt& err [[maybe_unused]]) const {
  if (_timeperiod_name.empty())
    throw msg_fmt("Time period has no name (property 'timeperiod_name')");
}

/**
 *  Get the time period key.
 *
 *  @return The time period name.
 */
timeperiod::key_type const& timeperiod::key() const noexcept {
  return _timeperiod_name;
}

/**
 *  Merge object.
 *
 *  @param[in] obj The object to merge.
 */
void timeperiod::merge(object const& obj) {
  if (obj.type() != _type)
    throw msg_fmt("Cannot merge time period with '{}'",
                  static_cast<uint32_t>(obj.type()));
  timeperiod const& tmpl(static_cast<timeperiod const&>(obj));

  MRG_DEFAULT(_alias);
  MRG_INHERIT(_exclude);
  MRG_DEFAULT(_timeperiod_name);
  MRG_TAB(_timeranges);

  // Merge exceptions.
  for (uint32_t i = 0; i < daterange::daterange_types; ++i) {
    for (std::list<daterange>::const_iterator it(tmpl._exceptions[i].begin()),
         end(tmpl._exceptions[i].end());
         it != end; ++it) {
      if (_has_similar_daterange(_exceptions[i], *it))
        continue;
      _exceptions[i].push_front(*it);
    }
  }
}

/**
 *  Parse and set the timeperiod property.
 *
 *  @param[in] key   The property name.
 *  @param[in] value The property value.
 *
 *  @return True on success, otherwise false.
 */
bool timeperiod::parse(char const* key, char const* value) {
  std::unordered_map<std::string, timeperiod::setter_func>::const_iterator it{
      _setters.find(key)};
  if (it != _setters.end())
    return (it->second)(*this, value);
  return _add_week_day(key, value);
}

/**
 *  Parse and set the timeperiod property.
 *
 *  @param[in] line  The configuration line.
 *
 *  @return True on success, otherwise false.
 */
bool timeperiod::parse(std::string const& line) {
  std::size_t pos(line.find_first_of(" \t\r", 0));
  if (pos == std::string::npos)
    return false;
  std::string key(line.substr(0, pos));
  std::string value(absl::StripAsciiWhitespace(line.substr(pos + 1)));

  if (object::parse(key.c_str(), value.c_str()) ||
      parse(key.c_str(), value.c_str()) || _add_calendar_date(line) ||
      _add_other_date(line))
    return true;
  return false;
}

/**
 *  Get alias value.
 *
 *  @return The alias value.
 */
std::string const& timeperiod::alias() const noexcept {
  return _alias;
}

/**
 *  Get exceptions value.
 *
 *  @return The exceptions value.
 */
std::array<std::list<configuration::daterange>,
           configuration::daterange::daterange_types> const&
timeperiod::exceptions() const noexcept {
  return _exceptions;
}

/**
 *  Get exclude value.
 *
 *  @return The exclude value.
 */
set_string const& timeperiod::exclude() const noexcept {
  return *_exclude;
}

/**
 *  Get timeperiod_name value.
 *
 *  @return The timeperiod_name value.
 */
std::string const& timeperiod::timeperiod_name() const noexcept {
  return _timeperiod_name;
}

/**
 *  Get timeranges.
 *
 *  @return The timeranges list.
 */
const std::array<std::list<configuration::timerange>, 7>&
timeperiod::timeranges() const {
  return _timeranges;
}

/**
 *  Build timerange from new line.
 *
 *  @param[in]  line       The line to parse.
 *  @param[out] timeranges The list to fill.
 *
 *  @return True on success, otherwise false.
 */
bool timeperiod::_build_timeranges(
    std::string const& line,
    std::list<configuration::timerange>& timeranges) {
  auto timeranges_str = absl::StrSplit(line, ',');
  for (auto tr : timeranges_str) {
    tr = absl::StripAsciiWhitespace(tr);
    std::size_t pos(tr.find('-'));
    if (pos == std::string::npos)
      return false;
    unsigned long start_time;
    if (!_build_time_t(tr.substr(0, pos), start_time))
      return false;
    unsigned long end_time;
    if (!_build_time_t(tr.substr(pos + 1), end_time))
      return false;
    timeranges.emplace_back(start_time, end_time);
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
bool timeperiod::_build_time_t(std::string_view time_str, unsigned long& ret) {
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
 *  Check if into the list we havea daterange similar
 *   to the argument.
 *
 *  @param[in] lst   The list to check.
 *  @param[in] range The date range to find.
 *
 *  @return True on success, otherwise false.
 */
bool timeperiod::_has_similar_daterange(
    const std::list<configuration::daterange>& lst,
    const configuration::daterange& range) noexcept {
  for (auto& l : lst)
    if (l.is_date_data_equal(range))
      return true;
  return false;
}

/**
 *  Add a calendar date.
 *
 *  @param[in] line The line to parse.
 *
 *  @return True on success, otherwise false.
 */
bool timeperiod::_add_calendar_date(std::string const& line) {
  int ret(0);
  int pos(0);
  bool fill_missing(false);
  unsigned int month_start(0);
  unsigned int month_end(0);
  unsigned int month_day_start(0);
  unsigned int month_day_end(0);
  unsigned int year_start(0);
  unsigned int year_end(0);
  unsigned int skip_interval(0);

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

    std::list<configuration::timerange> timeranges;
    if (!_build_timeranges(line.substr(pos), timeranges))
      return false;

    configuration::daterange range(daterange::calendar_date);
    range.set_syear(year_start);
    range.set_smon(month_start - 1);
    range.set_smday(month_day_start);
    range.set_eyear(year_end);
    range.set_emon(month_end - 1);
    range.set_emday(month_day_end);
    range.set_skip_interval(skip_interval);
    range.set_timerange(timeranges);

    _exceptions[daterange::calendar_date].push_front(range);
    return true;
  }
  return false;
}

/**
 *  Add other date.
 *
 *  @param[in] line The line to parse.
 *
 *  @return True on success, otherwise false.
 */
bool timeperiod::_add_other_date(std::string const& line) {
  int pos(0);
  daterange::type_range type(daterange::none);
  unsigned int month_start(0);
  unsigned int month_end(0);
  int month_day_start(0);
  int month_day_end(0);
  unsigned int skip_interval(0);
  unsigned int week_day_start(0);
  unsigned int week_day_end(0);
  int week_day_start_offset(0);
  int week_day_end_offset(0);
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
      type = daterange::month_week_day;
  } else if (sscanf(line.c_str(), "%[a-z] %d - %[a-z] %d / %u %n", buffer[0],
                    &month_day_start, buffer[1], &month_day_end, &skip_interval,
                    &pos) == 5) {
    // monday 2 - thursday 3 / 2
    if (_get_day_id(buffer[0], week_day_start) &&
        _get_day_id(buffer[1], week_day_end)) {
      week_day_start_offset = month_day_start;
      week_day_end_offset = month_day_end;
      type = daterange::week_day;
    }
    // february 1 - march 15 / 3
    else if (_get_month_id(buffer[0], month_start) &&
             _get_month_id(buffer[1], month_end))
      type = daterange::month_date;
    // day 4 - 6 / 2
    else if (!strcmp(buffer[0], "day") && !strcmp(buffer[1], "day"))
      type = daterange::month_day;
  } else if (sscanf(line.c_str(), "%[a-z] %d - %d / %u %n", buffer[0],
                    &month_day_start, &month_day_end, &skip_interval,
                    &pos) == 4) {
    // thursday 2 - 4
    if (_get_day_id(buffer[0], week_day_start)) {
      week_day_start_offset = month_day_start;
      week_day_end = week_day_start;
      week_day_end_offset = month_day_end;
      type = daterange::week_day;
    }
    // february 3 - 5
    else if (_get_month_id(buffer[0], month_start)) {
      month_end = month_start;
      type = daterange::month_date;
    }
    // day 1 - 4
    else if (!strcmp(buffer[0], "day"))
      type = daterange::month_day;
  } else if (sscanf(line.c_str(), "%[a-z] %d %[a-z] - %[a-z] %d %[a-z] %n",
                    buffer[0], &week_day_start_offset, buffer[1], buffer[2],
                    &week_day_end_offset, buffer[3], &pos) == 6) {
    // wednesday 1 january - thursday 2 july
    if (_get_day_id(buffer[0], week_day_start) &&
        _get_month_id(buffer[1], month_start) &&
        _get_day_id(buffer[2], week_day_end) &&
        _get_month_id(buffer[3], month_end))
      type = daterange::month_week_day;
  } else if (sscanf(line.c_str(), "%[a-z] %d - %d %n", buffer[0],
                    &month_day_start, &month_day_end, &pos) == 3) {
    // thursday 2 - 4
    if (_get_day_id(buffer[0], week_day_start)) {
      week_day_start_offset = month_day_start;
      week_day_end = week_day_start;
      week_day_end_offset = month_day_end;
      type = daterange::week_day;
    }
    // february 3 - 5
    else if (_get_month_id(buffer[0], month_start)) {
      month_end = month_start;
      type = daterange::month_date;
    }
    // day 1 - 4
    else if (!strcmp(buffer[0], "day"))
      type = daterange::month_day;
  } else if (sscanf(line.c_str(), "%[a-z] %d - %[a-z] %d %n", buffer[0],
                    &month_day_start, buffer[1], &month_day_end, &pos) == 4) {
    // monday 2 - thursday 3
    if (_get_day_id(buffer[0], week_day_start) &&
        _get_day_id(buffer[1], week_day_end)) {
      week_day_start_offset = month_day_start;
      week_day_end_offset = month_day_end;
      type = daterange::week_day;
    }
    // february 1 - march 15
    else if (_get_month_id(buffer[0], month_start) &&
             _get_month_id(buffer[1], month_end))
      type = daterange::month_date;
    // day 1 - day 5
    else if (!strcmp(buffer[0], "day") && !strcmp(buffer[1], "day"))
      type = daterange::month_day;
  } else if (sscanf(line.c_str(), "%[a-z] %d %[a-z] %n", buffer[0],
                    &week_day_start_offset, buffer[1], &pos) == 3) {
    // thursday 3 february
    if (_get_day_id(buffer[0], week_day_start) &&
        _get_month_id(buffer[1], month_start)) {
      month_end = month_start;
      week_day_end = week_day_start;
      week_day_end_offset = week_day_start_offset;
      type = daterange::month_week_day;
    }
  } else if (sscanf(line.c_str(), "%[a-z] %d %n", buffer[0], &month_day_start,
                    &pos) == 2) {
    // thursday 2
    if (_get_day_id(buffer[0], week_day_start)) {
      week_day_start_offset = month_day_start;
      week_day_end = week_day_start;
      week_day_end_offset = week_day_start_offset;
      type = daterange::week_day;
    }
    // february 3
    else if (_get_month_id(buffer[0], month_start)) {
      month_end = month_start;
      month_day_end = month_day_start;
      type = daterange::month_date;
    }
    // day 1
    else if (!strcmp(buffer[0], "day")) {
      month_day_end = month_day_start;
      type = daterange::month_day;
    }
  }

  if (type != daterange::none) {
    daterange range(type);
    if (type == daterange::month_day) {
      range.set_smday(month_day_start);
      range.set_emday(month_day_end);
    } else if (type == daterange::month_week_day) {
      range.set_smon(month_start);
      range.set_swday(week_day_start);
      range.set_swday_offset(week_day_start_offset);
      range.set_emon(month_end);
      range.set_ewday(week_day_end);
      range.set_ewday_offset(week_day_end_offset);
    } else if (type == daterange::week_day) {
      range.set_swday(week_day_start);
      range.set_swday_offset(week_day_start_offset);
      range.set_ewday(week_day_end);
      range.set_ewday_offset(week_day_end_offset);
    } else if (type == daterange::month_date) {
      range.set_smon(month_start);
      range.set_smday(month_day_start);
      range.set_emon(month_end);
      range.set_emday(month_day_end);
    }
    range.set_skip_interval(skip_interval);

    std::list<configuration::timerange> timeranges;
    if (!_build_timeranges(line.substr(pos), timeranges))
      return false;

    range.set_timerange(timeranges);
    _exceptions[type].push_front(range);
    return true;
  }

  return false;
}

/**
 *  Add a week day.
 *
 *  @param[in] key   The week day.
 *  @param[in] value The range.
 *
 *  @return True on success, otherwise false.
 */
bool timeperiod::_add_week_day(std::string const& key,
                               std::string const& value) {
  unsigned int day_id;
  if (!_get_day_id(key, day_id))
    return false;

  if (!_build_timeranges(value, _timeranges[day_id]))
    return false;

  return true;
}

/**
 *  Get the month id.
 *
 *  @param[in]  name The month name.
 *  @param[out] id   The id to fill.
 *
 *  @return True on success, otherwise false.
 */
bool timeperiod::_get_month_id(std::string const& name, unsigned int& id) {
  static std::string_view const months[] = {
      "january", "february", "march",     "april",   "may",      "june",
      "july",    "august",   "september", "october", "november", "december"};
  for (id = 0; id < sizeof(months) / sizeof(months[0]); ++id)
    if (name == months[id])
      return true;
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
bool timeperiod::_get_day_id(std::string const& name, unsigned int& id) {
  static std::string const days[] = {"sunday",    "monday",   "tuesday",
                                     "wednesday", "thursday", "friday",
                                     "saturday"};
  for (id = 0; id < sizeof(days) / sizeof(days[0]); ++id)
    if (name == days[id])
      return true;
  return false;
}

/**
 *  Set alias value.
 *
 *  @param[in] value The new alias value.
 *
 *  @return True on success, otherwise false.
 */
bool timeperiod::_set_alias(std::string const& value) {
  _alias = value;
  return true;
}

/**
 *  Set exclude value.
 *
 *  @param[in] value The new exclude value.
 *
 *  @return True on success, otherwise false.
 */
bool timeperiod::_set_exclude(std::string const& value) {
  _exclude = value;
  return true;
}

/**
 *  Set timeperiod_name value.
 *
 *  @param[in] value The new timeperiod_name value.
 *
 *  @return True on success, otherwise false.
 */
bool timeperiod::_set_timeperiod_name(std::string const& value) {
  _timeperiod_name = value;
  return true;
}
