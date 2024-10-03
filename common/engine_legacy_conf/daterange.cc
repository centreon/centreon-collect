/**
 * Copyright 2024 Centreon
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
#include "daterange.hh"

using namespace com::centreon::engine::configuration;

/**
 *  Create a new exception to a timeperiod.
 *
 *  @param[in] period        Base period.
 *  @param[in] type Type of the range (see header file).
 *  @param[in] syear The start year.
 *  @param[in] smon The start month.
 *  @param[in] smday The start day of month (may 3rd, last day in feb).
 *  @param[in] swday The start day of week (thursday).
 *  @param[in] swday_offset The start weekday offset (3rd thursday, last monday
 * in jan).
 *  @param[in] eyear The end year.
 *  @param[in] emon The end month.
 *  @param[in] emday The end day of month
 *  @param[in] ewday The end day of week.
 *  @param[in] ewday_offset The end weekday offset.
 *  @param[in] skip_interval The interval with the next day.
 */
daterange::daterange(type_range type,
                     int syear,
                     int smon,
                     int smday,
                     int swday,
                     int swday_offset,
                     int eyear,
                     int emon,
                     int emday,
                     int ewday,
                     int ewday_offset,
                     int skip_interval)
    : _type{type},
      _syear{syear},
      _smon{smon},
      _smday{smday},
      _swday{swday},
      _swday_offset{swday_offset},
      _eyear{eyear},
      _emon{emon},
      _emday{emday},
      _ewday{ewday},
      _ewday_offset{ewday_offset},
      _skip_interval{skip_interval} {}
daterange::type_range daterange::type() const {
  return _type;
}

daterange::daterange(type_range type)
    : _type{type},
      _syear{0},
      _smon{0},
      _smday{0},
      _swday{0},
      _swday_offset{0},
      _eyear{0},
      _emon{0},
      _emday{0},
      _ewday{0},
      _ewday_offset{0},
      _skip_interval{0} {}

/**
 * @brief Almost the same as the == operator but here timeranges are not
 * compared.
 *
 * @param obj Only date data are compared.
 *
 * @return A boolean that is true if they are "similar".
 */
bool daterange::is_date_data_equal(const configuration::daterange& obj) const {
  return _type == obj.type() && _syear == obj.get_syear() &&
         _smon == obj.get_smon() && _smday == obj.get_smday() &&
         _swday == obj.get_swday() && _swday_offset == obj.get_swday_offset() &&
         _eyear == obj.get_eyear() && _emon == obj.get_emon() &&
         _emday == obj.get_emday() && _ewday == obj.get_ewday() &&
         _ewday_offset == obj.get_ewday_offset() &&
         _skip_interval == obj.get_skip_interval();
}

void daterange::set_syear(int32_t syear) {
  _syear = syear;
}

void daterange::set_smon(int32_t smon) {
  _smon = smon;
}

void daterange::set_smday(int32_t smday) {
  _smday = smday;
}

void daterange::set_swday(int32_t smday) {
  _swday = smday;
}

void daterange::set_swday_offset(int32_t smday_offset) {
  _swday_offset = smday_offset;
}

void daterange::set_eyear(int32_t eyear) {
  _eyear = eyear;
}

void daterange::set_emon(int32_t emon) {
  _emon = emon;
}

void daterange::set_emday(int32_t emday) {
  _emday = emday;
}

void daterange::set_ewday(int32_t ewday) {
  _ewday = ewday;
}

void daterange::set_ewday_offset(int32_t ewday_offset) {
  _ewday_offset = ewday_offset;
}

void daterange::set_skip_interval(int32_t skip_interval) {
  _skip_interval = skip_interval;
}

void daterange::set_timerange(
    const std::list<configuration::timerange>& timerange) {
  _timerange = timerange;
}

const std::list<timerange>& daterange::get_timerange() const {
  return _timerange;
}

/**
 *  Equal operator.
 *
 *  @param[in] other Object to compare to.
 *
 *  @return True if this object is less than right.
 */
bool daterange::operator==(const daterange& other) const {
  return _type == other.type() && _syear == other.get_syear() &&
         _smon == other._smon && _smday == other._smday &&
         _swday == other.get_swday() && _swday_offset == other._swday_offset &&
         _eyear == other._eyear && _emon == other._emon &&
         _emday == other._emday && _ewday == other._ewday &&
         _ewday_offset == other._ewday_offset &&
         _skip_interval == other._skip_interval &&
         _timerange == other._timerange;
}

/**
 *  Less-than operator.
 *
 *  @param[in] other Object to compare to.
 *
 *  @return True if this object is less than right.
 */
bool daterange::operator<(daterange const& other) const {
  if (_emon != other._emon)
    return _emon < other._emon;
  else if (_smon != other._smon)
    return _smon < other._smon;
  else if (_emday != other._emday)
    return _emday < other._emday;
  else if (_smday != other._smday)
    return _smday < other._smday;
  else if (_skip_interval != other._skip_interval)
    return _skip_interval < other._skip_interval;
  else if (_type != other._type)
    return _type < other._type;
  else if (_ewday != other._ewday)
    return _ewday < other._ewday;
  else if (_swday != other._swday)
    return _swday < other._swday;
  else if (_ewday_offset != other._ewday_offset)
    return _ewday_offset < other._ewday_offset;
  else if (_swday_offset != other._swday_offset)
    return _swday_offset < other._swday_offset;
  else if (_eyear != other._eyear)
    return _eyear < other._eyear;
  else if (_syear != other._syear)
    return _syear < other._syear;
  return _timerange < other._timerange;
}

namespace com::centreon::engine::configuration {
/**
 *  Dump timerange content into the stream.
 *
 *  @param[out] os  The output stream.
 *  @param[in]  obj The timerange to dump.
 *
 *  @return The output stream.
 */
std::ostream& operator<<(std::ostream& os, const timerange& obj) {
  uint32_t start_hours(obj.range_start() / 3600);
  uint32_t start_minutes((obj.range_start() % 3600) / 60);
  uint32_t end_hours(obj.range_end() / 3600);
  uint32_t end_minutes((obj.range_end() % 3600) / 60);
  os << fmt::format("{:02}:{:02}-{:02}:{:02}", start_hours, start_minutes,
                    end_hours, end_minutes);
  return os;
}

/**
 *  Dump timerange_list content into the stream.
 *
 *  @param[out] os  The output stream.
 *  @param[in]  obj The timerange_list to dump.
 *
 *  @return The output stream.
 */
std::ostream& operator<<(std::ostream& os, const std::list<timerange>& obj) {
  for (auto it = obj.begin(), end = obj.end(); it != end; ++it)
    os << *it << ((next(it) == obj.end()) ? "" : ", ");
  return os;
}

/**
 *  Dump the daterange value into the calendar date format.
 *
 *  @param[out] os  The output stream.
 *  @param[in]  obj The daterange to dump.
 *
 *  @return The output stream.
 */
static std::ostream& _dump_calendar_date(std::ostream& os,
                                         daterange const& obj) {
  os << fmt::format("{:02}-{:02}-{:02}", obj.get_syear(), obj.get_smon() + 1,
                    obj.get_smday());
  if (obj.get_syear() != obj.get_eyear() || obj.get_smon() != obj.get_emon() ||
      obj.get_smday() != obj.get_emday())
    os << fmt::format(" - {:02}-{:02}-{:02}", obj.get_eyear(),
                      obj.get_emon() + 1, obj.get_emday());
  if (obj.get_skip_interval())
    os << " / " << obj.get_skip_interval();
  return os;
}

static const std::string_view& month_name(uint32_t index) {
  static constexpr std::array<std::string_view, 12> month_name{
      "january", "february", "march",     "april",   "may",      "june",
      "july",    "august",   "september", "october", "november", "december"};
  return month_name[index];
}

static const std::string_view& weekday_name(uint32_t index) {
  static constexpr std::array<std::string_view, 7> day_name{
      "sunday",   "monday", "tuesday", "wednesday",
      "thursday", "friday", "saturday"};
  return day_name[index];
}

/**
 *  Dump the daterange value into the month date format.
 *
 *  @param[out] os  The output stream.
 *  @param[in]  obj The daterange to stringify.
 *
 *  @return The output stream.
 */
static std::ostream& _dump_month_date(std::ostream& os, daterange const& obj) {
  const std::string_view& smon = month_name(obj.get_smon());
  const std::string_view& emon = month_name(obj.get_emon());
  os << smon << " " << obj.get_smday();
  if (smon != emon)
    os << " - " << emon << " " << obj.get_emday();
  else if (obj.get_smday() != obj.get_emday())
    os << " - " << obj.get_emday();
  if (obj.get_skip_interval())
    os << " / " << obj.get_skip_interval();
  return os;
}

/**
 *  Dump the daterange value into the month day format.
 *
 *  @param[out] os  The output stream.
 *  @param[in]  obj The daterange to stringify.
 *
 *  @return The output stream.
 */
static std::ostream& _dump_month_day(std::ostream& os, daterange const& obj) {
  os << "day " << obj.get_smday();
  if (obj.get_smday() != obj.get_emday())
    os << " - " << obj.get_emday();
  if (obj.get_skip_interval())
    os << " / " << obj.get_skip_interval();
  return os;
}

/**
 *  Dump the daterange value into the month week day
 *  format.
 *
 *  @param[out] os  The output stream.
 *  @param[in]  obj The daterange to stringify.
 *
 *  @return The output stream.
 */
static std::ostream& _dump_month_week_day(std::ostream& os,
                                          daterange const& obj) {
  os << weekday_name(obj.get_swday()) << " " << obj.get_swday_offset() << " "
     << month_name(obj.get_smon());
  if (obj.get_swday() != obj.get_ewday() ||
      obj.get_swday_offset() != obj.get_ewday_offset() ||
      obj.get_smon() != obj.get_emon())
    os << " - " << weekday_name(obj.get_ewday()) << " "
       << obj.get_ewday_offset() << " " << month_name(obj.get_emon());
  if (obj.get_skip_interval())
    os << " / " << obj.get_skip_interval();
  return os;
}

/**
 *  Dump the daterange value into the week day format.
 *
 *  @param[out] os  The output stream.
 *  @param[in]  obj The daterange to stringify.
 *
 *  @return The output stream.
 */
static std::ostream& _dump_week_day(std::ostream& os, daterange const& obj) {
  os << weekday_name(obj.get_swday()) << " " << obj.get_swday_offset();
  if (obj.get_swday() != obj.get_ewday() ||
      obj.get_swday_offset() != obj.get_ewday_offset())
    os << " - " << weekday_name(obj.get_ewday()) << " "
       << obj.get_ewday_offset();
  if (obj.get_skip_interval())
    os << " / " << obj.get_skip_interval();
  return os;
}

/**
 *  Dump daterange content into the stream.
 *
 *  @param[out] os  The output stream.
 *  @param[in]  obj The daterange to dump.
 *
 *  @return The output stream.
 */
std::ostream& operator<<(std::ostream& os, const daterange& obj) {
  typedef std::ostream& (*func)(std::ostream&, daterange const&);
  static func tab[] = {
      &_dump_calendar_date,  &_dump_month_date, &_dump_month_day,
      &_dump_month_week_day, &_dump_week_day,
  };

  if (obj.type() < 0 || obj.type() >= daterange::daterange_types)
    os << "unknown type " << obj.type();
  else {
    (*(tab[obj.type()]))(os, obj);
    os << " " << obj.get_timerange();
  }
  return os;
}

std::ostream& operator<<(
    std::ostream& os,
    const std::array<std::list<daterange>, daterange::daterange_types>& obj) {
  os << '{';
  for (unsigned ii = 0; ii < obj.size(); ++ii) {
    switch (ii) {
      case daterange::calendar_date:
        os << "calendar_date:";
        break;
      case daterange::month_date:
        os << "month_date:";
        break;
      case daterange::month_day:
        os << "month_day:";
        break;
      case daterange::month_week_day:
        os << "month_week_day:";
        break;
      case daterange::week_day:
        os << "week_day:";
        break;
    }
    for (const daterange& dr : obj[ii]) {
      os << '{' << dr << "},";
    }
    os << '[';
    os << "],";
  }
  os << '}';
  return os;
}
}  // namespace com::centreon::engine::configuration
