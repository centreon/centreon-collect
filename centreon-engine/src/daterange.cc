/*
** Copyright 2011-2013 Merethis
**
** This file is part of Centreon Engine.
**
** Centreon Engine is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Engine is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Engine. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include "com/centreon/engine/daterange.hh"
#include <array>
#include <iomanip>
#include "com/centreon/engine/common.hh"
#include "com/centreon/engine/string.hh"
#include "com/centreon/engine/timeperiod.hh"
#include "com/centreon/engine/timerange.hh"

using namespace com::centreon::engine;

/**
 *  Create a new exception to a timeperiod.
 *
 *  @param[in] period        Base period.
 *  @param[in] type
 *  @param[in] syear
 *  @param[in] smon
 *  @param[in] smday
 *  @param[in] swday
 *  @param[in] swday_offset
 *  @param[in] eyear
 *  @param[in] emon
 *  @param[in] emday
 *  @param[in] ewday
 *  @param[in] ewday_offset
 *  @param[in] skip_interval
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

daterange::daterange(type_range type)
    : _type(type),
      _syear(0),
      _smon(0),
      _smday(0),
      _swday(0),
      _swday_offset(0),
      _eyear(0),
      _emon(0),
      _emday(0),
      _ewday(0),
      _ewday_offset(0),
      _skip_interval(0) {}

/**
 *  Equal operator.
 *
 *  @param[in] obj1 The first object to compare.
 *  @param[in] obj2 The second object to compare.
 *
 *  @return True if is the same object, otherwise false.
 */
bool daterange::operator==(daterange const& obj) const {
  return _type == obj.get_type() && _syear == obj.get_syear() &&
         _smon == obj.get_smon() && _smday == obj.get_smday() &&
         _swday == obj.get_swday() && _swday_offset == obj.get_swday_offset() &&
         _eyear == obj.get_eyear() && _emon == obj.get_emon() &&
         _emday == obj.get_emday() && _ewday == obj.get_ewday() &&
         _ewday_offset == obj.get_ewday_offset() &&
         _skip_interval == obj.get_skip_interval() &&
         _timerange == obj._timerange;
}

bool daterange::is_date_data_equal(daterange const& obj) const {
  return _type == obj.get_type() && _syear == obj.get_syear() &&
         _smon == obj.get_smon() && _smday == obj.get_smday() &&
         _swday == obj.get_swday() && _swday_offset == obj.get_swday_offset() &&
         _eyear == obj.get_eyear() && _emon == obj.get_emon() &&
         _emday == obj.get_emday() && _ewday == obj.get_ewday() &&
         _ewday_offset == obj.get_ewday_offset() &&
         _skip_interval == obj.get_skip_interval();
}

/**
 *  Not equal operator.
 *
 *  @param[in] obj1 The first object to compare.
 *  @param[in] obj2 The second object to compare.
 *
 *  @return True if is not the same object, otherwise false.
 */
bool daterange::operator!=(daterange const& obj) const {
  return !(*this == obj);
}

/**
 *  Less-than operator.
 *
 *  @param[in] right Object to compare to.
 *
 *  @return True if this object is less than right.
 */
bool daterange::operator<(daterange const& right) const {
  if (_emon != right._emon)
    return (_emon < right._emon);
  else if (_smon != right._smon)
    return (_smon < right._smon);
  else if (_emday != right._emday)
    return (_emday < right._emday);
  else if (_smday != right._smday)
    return (_smday < right._smday);
  else if (_skip_interval != right._skip_interval)
    return (_skip_interval < right._skip_interval);
  else if (_type != right._type)
    return (_type < right._type);
  else if (_ewday != right._ewday)
    return (_ewday < right._ewday);
  else if (_swday != right._swday)
    return (_swday < right._swday);
  else if (_ewday_offset != right._ewday_offset)
    return (_ewday_offset < right._ewday_offset);
  else if (_swday_offset != right._swday_offset)
    return (_swday_offset < right._swday_offset);
  else if (_eyear != right._eyear)
    return (_eyear < right._eyear);
  else if (_syear != right._syear)
    return (_syear < right._syear);
  return (_timerange < right._timerange);
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
  os << std::setfill('0') << std::setw(2) << obj.get_syear() << "-"
     << std::setfill('0') << std::setw(2) << obj.get_smon() + 1 << "-"
     << std::setfill('0') << std::setw(2) << obj.get_smday();
  if (obj.get_syear() != obj.get_eyear() || obj.get_smon() != obj.get_emon() ||
      obj.get_smday() != obj.get_emday())
    os << " - " << std::setfill('0') << std::setw(2) << obj.get_eyear() << "-"
       << std::setfill('0') << std::setw(2) << obj.get_emon() + 1 << "-"
       << std::setfill('0') << std::setw(2) << obj.get_emday();
  if (obj.get_skip_interval())
    os << " / " << obj.get_skip_interval();
  return (os);
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
  std::string const& smon(daterange::get_month_name(obj.get_smon()));
  std::string const& emon(daterange::get_month_name(obj.get_emon()));
  os << smon << " " << obj.get_smday();
  if (smon != emon)
    os << " - " << emon << " " << obj.get_emday();
  else if (obj.get_smday() != obj.get_emday())
    os << " - " << obj.get_emday();
  if (obj.get_skip_interval())
    os << " / " << obj.get_skip_interval();
  return (os);
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
  return (os);
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
  os << daterange::get_weekday_name(obj.get_swday()) << " "
     << obj.get_swday_offset() << " "
     << daterange::get_month_name(obj.get_smon());
  if (obj.get_swday() != obj.get_ewday() ||
      obj.get_swday_offset() != obj.get_ewday_offset() ||
      obj.get_smon() != obj.get_emon())
    os << " - " << daterange::get_weekday_name(obj.get_ewday()) << " "
       << obj.get_ewday_offset() << " "
       << daterange::get_month_name(obj.get_emon());
  if (obj.get_skip_interval())
    os << " / " << obj.get_skip_interval();
  return (os);
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
  os << daterange::get_weekday_name(obj.get_swday()) << " "
     << obj.get_swday_offset();
  if (obj.get_swday() != obj.get_ewday() ||
      obj.get_swday_offset() != obj.get_ewday_offset())
    os << " - " << daterange::get_weekday_name(obj.get_ewday()) << " "
       << obj.get_ewday_offset();
  if (obj.get_skip_interval())
    os << " / " << obj.get_skip_interval();
  return (os);
}

CCE_BEGIN()
/**
 *  Dump daterange content into the stream.
 *
 *  @param[out] os  The output stream.
 *  @param[in]  obj The daterange to dump.
 *
 *  @return The output stream.
 */
std::ostream& operator<<(std::ostream& os, daterange const& obj) {
  typedef std::ostream& (*func)(std::ostream&, daterange const&);
  static func tab[] = {
      &_dump_calendar_date,  &_dump_month_date, &_dump_month_day,
      &_dump_month_week_day, &_dump_week_day,
  };

  if (obj.get_type() < 0 || obj.get_type() >= DATERANGE_TYPES)
    os << "unknown type " << obj.get_type();
  else {
    (*(tab[obj.get_type()]))(os, obj);
    os << " " << obj.get_timerange();
  }
  return (os);
}

std::ostream& operator<<(std::ostream& os, exception_array const& obj) {
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

CCE_END()

/**
 *  Get the month name.
 *
 *  @param[in] index  The month position.
 *
 *  @return The month name.
 */
std::string const& daterange::get_month_name(unsigned int index) {
  static std::string const unknown("unknown");
  static std::string const month[] = {
      "january", "february", "march",     "april",   "may",      "june",
      "july",    "august",   "september", "october", "november", "december"};
  if (index >= sizeof(month) / sizeof(*month))
    return (unknown);
  return (month[index]);
}

/**
 *  Get the weekday name.
 *
 *  @param[in] index  The weekday position.
 *
 *  @return The weekday name.
 */
std::string const& daterange::get_weekday_name(unsigned int index) {
  static std::string const unknown("unknown");
  static std::string const days[] = {"sunday",    "monday",   "tuesday",
                                     "wednesday", "thursday", "friday",
                                     "saturday"};
  if (index >= sizeof(days) / sizeof(*days))
    return (unknown);
  return (days[index]);
}
