/*
** Copyright 2011 Merethis
**
** This file is part of Centreon Clib.
**
** Centreon Clib is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Clib is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Clib. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <sys/time.h>
#include "com/centreon/timestamp.hh"

using namespace com::centreon;

/**
 *  Default constructor.
 *
 *  @param[in] second   Set the second.
 *  @param[in] usecond  Set the microsecond.
 */
timestamp::timestamp(time_t second, long usecond)
  : _usecond(usecond), _second(second) {
  _transfer(&_second, &_usecond);
}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
timestamp::timestamp(timestamp const& right) {
  _internal_copy(right);
}

/**
 *  Default destructor.
 */
timestamp::~timestamp() throw () {

}

/**
 *  Default copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
timestamp& timestamp::operator=(timestamp const& right) {
  return (_internal_copy(right));
}

/**
 *  Compare if two object are equal.
 *
 *  @param[in] right  The object to compare.
 *
 *  @return True if equal, otherwise false.
 */
bool timestamp::operator==(timestamp const& right) const throw () {
  return (_second == right._second && _usecond == right._usecond);
}

/**
 *  Compare if two object are not equal.
 *
 *  @param[in] right  The object to compare.
 *
 *  @return True if not equal, otherwise false.
 */
bool timestamp::operator!=(timestamp const& right) const throw () {
  return (!operator==(right));
}

/**
 *  Compare if this is less than an object.
 *
 *  @param[in] right  The object to compare.
 *
 *  @return True if less, otherwise false.
 */
bool timestamp::operator<(timestamp const& right) const throw () {
  return (_second < right._second
          || (_second == right._second && _usecond < right._usecond));
}

/**
 *  Compare if this if less or equal than an object.
 *
 *  @param[in] right  The object to compare.
 *
 *  @return True if less or equal, otherwise false.
 */
bool timestamp::operator<=(timestamp const& right) const throw () {
  return (operator<(right) || operator==(right));
}

/**
 *  Compare if this if greater than an object.
 *
 *  @param[in] right  The object to compare.
 *
 *  @return True if greater, otherwise false.
 */
bool timestamp::operator>(timestamp const& right) const throw () {
  return (!operator<=(right));
}

/**
 *  Compare if this is greater or equal than an object.
 *
 *  @param[in] right  The object to compare.
 *
 *  @return True if greater or equal, otherwise false.
 */
bool timestamp::operator>=(timestamp const& right) const throw () {
  return (!operator<(right));
}

/**
 *  Add timestamp.
 *
 *  @param[in] right  The object to add.
 *
 *  @return The new timestamp.
 */
timestamp timestamp::operator+(timestamp const& right) const {
  timestamp ret(*this);
  ret += right;
  return (ret);
}

/**
 *  Substract timestamp.
 *
 *  @param[in] right  The object to substract.
 *
 *  @return The new timestamp.
 */
timestamp timestamp::operator-(timestamp const& right) const {
  timestamp ret(*this);
  ret -= right;
  return (ret);
}

/**
 *  Add timestamp.
 *
 *  @param[in] right  The object to add.
 *
 *  @return This object.
 */
timestamp& timestamp::operator+=(timestamp const& right) {
  _usecond += right._usecond;
  _second += right._second;
  _transfer(&_second, &_usecond);
  return (*this);
}

/**
 *  Substract timestamp.
 *
 *  @param[in] right  The object to substract.
 *
 *  @return This object.
 */
timestamp& timestamp::operator-=(timestamp const& right) {
  _transfer(&_second, &_usecond);
  _usecond -= right._usecond;
  _second -= right._second;
  _transfer(&_second, &_usecond);
  return (*this);
}

/**
 *  Add millisecond into this object.
 *
 *  @param[in] msecond  Time in milliseconds.
 */
void timestamp::add_msecond(long msecond) {
  add_usecond(msecond * 1000);
}

/**
 *  Add second into this object.
 *
 *  @param[in] second  Time in seconds.
 */
void timestamp::add_second(time_t second) {
  _second += second;
}

/**
 *  Add microsecond into this object.
 *
 *  @param[in] usecond  Time in microseconds.
 */
void timestamp::add_usecond(long usecond) {
  time_t second(0);
  _transfer(&second, &usecond);
  _usecond += usecond;
  _second += second;
  _transfer(&_second, &_usecond);
}

/**
 *  Get the current system time.
 *
 *  @return The current timestamp.
 */
timestamp timestamp::now() throw () {
  timeval tv;
  gettimeofday(&tv, NULL);
  return (timestamp(tv.tv_sec, tv.tv_usec));
}

/**
 *  Substract millisecond into this object.
 *
 *  @param[in] msecond  Time in milliseconds.
 */
void timestamp::sub_msecond(long msecond) {
  sub_usecond(msecond * 1000);
}

/**
 *  Substract second into this object.
 *
 *  @param[in] second  Time in seconds.
 */
void timestamp::sub_second(time_t second) {
  _second -= second;
}

/**
 *  Substract microsecond into this object.
 *
 *  @param[in] usecond  Time in microseconds.
 */
void timestamp::sub_usecond(long usecond) {
  time_t second(0);
  _transfer(&second, &usecond);
  _usecond -= usecond;
  _second -= second;
  _transfer(&_second, &_usecond);
}

/**
 *  Get the time in milliseconds.
 *
 *  @return The time in milliseconds.
 */
long timestamp::to_msecond() const throw () {
  return (_second * 1000 + _usecond / 1000);
}

/**
 *  Get the time in seconds.
 *
 *  @return The time in seconds.
 */
time_t timestamp::to_second() const throw () {
  return (_second);
}

/**
 *  Get the time in microseconds.
 *
 *  @return The time in microseconds.
 */
long timestamp::to_usecond() const throw () {
  return (_second * 1000000 + _usecond);
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
timestamp& timestamp::_internal_copy(timestamp const& right) {
  if (this != &right) {
    _usecond = right._usecond;
    _second = right._second;
  }
  return (*this);
}

/**
 *  Transfer microsecond in second if is possible.
 *  @remark This function is static.
 *
 *  @param[in,out] second   The second.
 *  @param[in]     usecond  The microsecond.
 */
void timestamp::_transfer(time_t* second, long* usecond) {
  // Transforms unnecessary microseconds into seconds.
  int nsec(*usecond / 1000000);
  *usecond -= nsec * 1000000;
  *second += nsec;

  if (*usecond < 0) {
    *usecond = 1000000 + *usecond;
    *second -= 1;
  }
}
