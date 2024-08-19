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
#include "point_2d.hh"

using namespace com::centreon::engine::configuration;

/**
 *  Constructor.
 *
 *  @param[in] x The x coordinates.
 *  @param[in] y The y coordinates.
 */
point_2d::point_2d(int x, int y) : _x(x), _y(y) {}

/**
 *  Copy constructor.
 *
 *  @param[in] right The object to copy.
 */
point_2d::point_2d(point_2d const& right) {
  operator=(right);
}

/**
 *  Destructor.
 */
point_2d::~point_2d() throw() {}

/**
 *  Copy operator.
 *
 *  @param[in] right The object to copy.
 *
 *  @return This object.
 */
point_2d& point_2d::operator=(point_2d const& right) {
  if (this != &right) {
    _x = right._x;
    _y = right._y;
  }
  return (*this);
}

/**
 *  Equal operator.
 *
 *  @param[in] right The opbject to compare.
 *
 *  @return True if is the same object, otherwise false.
 */
bool point_2d::operator==(point_2d const& right) const throw() {
  return (_x == right._x && _y == right._y);
}

/**
 *  Not equal operator.
 *
 *  @param[in] right The opbject to compare.
 *
 *  @return True if is not the same object, otherwise false.
 */
bool point_2d::operator!=(point_2d const& right) const throw() {
  return (!operator==(right));
}

/**
 *  Less-than operator.
 *
 *  @param[in] right Object to compare to.
 *
 *  @return True if this object is less than right.
 */
bool point_2d::operator<(point_2d const& right) const throw() {
  if (_x != right._x)
    return (_x < right._x);
  return (_y < right._y);
}

/**
 *  Get the x coordinates.
 *
 *  @return The x coordinates.
 */
int point_2d::x() const throw() {
  return (_x);
}

/**
 *  Get the y coordinates.
 *
 *  @return The y coordinates.
 */
int point_2d::y() const throw() {
  return (_y);
}
