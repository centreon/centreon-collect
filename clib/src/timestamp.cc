/**
 * Copyright 2011-2013 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */

#include "com/centreon/timestamp.hh"

#include <sys/time.h>

#include <limits>

using namespace com::centreon;

/**
 *  Default constructor.
 *
 *  @param[in] secs   Set the seconds.
 *  @param[in] usecs  Set the microseconds.
 */
timestamp::timestamp(time_t secs, int32_t usecs) : _secs(secs), _usecs(0) {
  add_useconds(usecs);
}

/**
 *  Copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
timestamp::timestamp(const timestamp& right)
    : _secs{right._secs}, _usecs{right._usecs} {}

/**
 *  Assignment operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
timestamp& timestamp::operator=(const timestamp& right) {
  if (this != &right) {
    _secs = right._secs;
    _usecs = right._usecs;
  }
  return *this;
}

/**
 *  Compare if two object are equal.
 *
 *  @param[in] right  The object to compare.
 *
 *  @return True if equal, otherwise false.
 */
bool timestamp::operator==(const timestamp& right) const noexcept {
  return _secs == right._secs && _usecs == right._usecs;
}

/**
 *  Compare if two object are not equal.
 *
 *  @param[in] right  The object to compare.
 *
 *  @return True if not equal, otherwise false.
 */
bool timestamp::operator!=(const timestamp& right) const noexcept {
  return _secs != right._secs || _usecs != right._usecs;
}

/**
 *  Compare if this is less than an object.
 *
 *  @param[in] right  The object to compare.
 *
 *  @return True if less, otherwise false.
 */
bool timestamp::operator<(const timestamp& right) const noexcept {
  return _secs < right._secs || (_secs == right._secs && _usecs < right._usecs);
}

/**
 *  Compare if this if less or equal than an object.
 *
 *  @param[in] right  The object to compare.
 *
 *  @return True if less or equal, otherwise false.
 */
bool timestamp::operator<=(const timestamp& right) const noexcept {
  return _secs < right._secs ||
         (_secs == right._secs && _usecs <= right._usecs);
}

/**
 *  Compare if this if greater than an object.
 *
 *  @param[in] right  The object to compare.
 *
 *  @return True if greater, otherwise false.
 */
bool timestamp::operator>(const timestamp& right) const noexcept {
  return _secs > right._secs || (_secs == right._secs && _usecs > right._usecs);
}

/**
 *  Compare if this is greater or equal than an object.
 *
 *  @param[in] right  The object to compare.
 *
 *  @return True if greater or equal, otherwise false.
 */
bool timestamp::operator>=(const timestamp& right) const noexcept {
  return _secs > right._secs ||
         (_secs == right._secs && _usecs >= right._usecs);
}

/**
 *  Add timestamp.
 *
 *  @param[in] right  The object to add.
 *
 *  @return The new timestamp.
 */
timestamp timestamp::operator+(const timestamp& right) const {
  timestamp ret(*this);
  ret += right;
  return ret;
}

/**
 *  Substract timestamp.
 *
 *  @param[in] right  The object to substract.
 *
 *  @return The new timestamp.
 */
timestamp timestamp::operator-(const timestamp& right) const {
  timestamp ret(*this);
  ret -= right;
  return ret;
}

/**
 *  Add timestamp.
 *
 *  @param[in] right  The object to add.
 *
 *  @return This object.
 */
timestamp& timestamp::operator+=(const timestamp& right) {
  _secs += right._secs;
  _usecs += right._usecs;
  if (_usecs >= 1000000) {
    _usecs -= 1000000;
    _secs++;
  }
  return *this;
}

/**
 *  Substract timestamp.
 *
 *  @param[in] right  The object to substract.
 *
 *  @return This object.
 */
timestamp& timestamp::operator-=(const timestamp& right) {
  _secs -= right._secs;
  int32_t d = static_cast<int32_t>(_usecs) - static_cast<int32_t>(right._usecs);
  if (d < 0) {
    _usecs = d + 1000000;
    _secs--;
  }
  return *this;
}

/**
 *  Add milliseconds to this object.
 *
 *  @param[in] msecs  Time in milliseconds.
 */
void timestamp::add_mseconds(int32_t msecs) { add_useconds(msecs * 1000); }

/**
 *  Add microseconds to this object.
 *
 *  @param[in] usecs  Time in microseconds.
 */
void timestamp::add_useconds(int32_t usecs) {
  int64_t us(_usecs);
  us += usecs;
  if (us < 0) {
    _secs += us / 1000000;  // Will be negative.
    us %= 1000000;
    if (us) {  // Non zero means negative value.
      --_secs;
      us += 1000000;
    }
  } else if (us >= 1000000) {
    _secs += us / 1000000;
    us %= 1000000;
  }
  _usecs = static_cast<uint32_t>(us);
}

/**
 *  Reset timestamp.
 */
void timestamp::clear() noexcept {
  _secs = 0;
  _usecs = 0;
}

/**
 *  Get the maximum time.
 *
 *  @return Maximum time.
 */
timestamp timestamp::max_time() noexcept {
  timestamp t;
  t._secs = std::numeric_limits<time_t>::max();
  t._usecs = 999999;
  return t;
}

/**
 *  Get the minimum time.
 *
 *  @return Minimum time.
 */
timestamp timestamp::min_time() noexcept {
  timestamp t;
  t._secs = std::numeric_limits<time_t>::min();
  t._usecs = 0;
  return t;
}

/**
 *  Get the current system time.
 *
 *  @return The current timestamp.
 */
timestamp timestamp::now() noexcept {
  timeval tv;
  gettimeofday(&tv, NULL);
  return timestamp(tv.tv_sec, tv.tv_usec);
}

/**
 *  Substract milliseconds from this object.
 *
 *  @param[in] msecs  Time in milliseconds.
 */
void timestamp::sub_mseconds(int32_t msecs) { add_mseconds(-msecs); }

/**
 *  Substract seconds from this object.
 *
 *  @param[in] secs  Time in seconds.
 */
void timestamp::sub_seconds(time_t secs) noexcept { _secs -= secs; }

/**
 *  Add seconds from this object.
 *
 *  @param[in] secs  Time in seconds.
 */
void timestamp::add_seconds(time_t secs) noexcept { _secs += secs; }

/**
 *  Substract microseconds from this object.
 *
 *  @param[in] usecs  Time in microseconds.
 */
void timestamp::sub_useconds(int32_t usecs) { add_useconds(-usecs); }

/**
 *  Get the time in milliseconds.
 *
 *  @return The time in milliseconds.
 */
int64_t timestamp::to_mseconds() const noexcept {
  return _secs * 1000ll + _usecs / 1000;
}

/**
 *  Get the time in seconds.
 *
 *  @return The time in seconds.
 */
time_t timestamp::to_seconds() const noexcept { return _secs; }

/**
 *  Get the time in microseconds.
 *
 *  @return The time in microseconds.
 */
int64_t timestamp::to_useconds() const noexcept {
  return _secs * 1000000ll + _usecs;
}

namespace com::centreon {

std::ostream& operator<<(std::ostream& s, const timestamp& to_dump) {
  struct tm tmp;
  time_t seconds = to_dump.to_seconds();
  localtime_r(&seconds, &tmp);
  char buf[80];
  strftime(buf, sizeof(buf), "%c: ", &tmp);
  s << buf;

  return s;
}
}  // namespace com::centreon
