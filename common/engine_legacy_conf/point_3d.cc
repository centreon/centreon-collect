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
#include "point_3d.hh"

using namespace com::centreon::engine::configuration;

/**
 *  Constructor.
 *
 *  @param[in] x The x coordinates.
 *  @param[in] y The y coordinates.
 *  @param[in] z The z coordinates.
 */
point_3d::point_3d(double x, double y, double z) : _x(x), _y(y), _z(z) {}

/**
 *  Copy constructor.
 *
 *  @param[in] right The object to copy.
 */
point_3d::point_3d(point_3d const& right) {
  operator=(right);
}

/**
 *  Destructor.
 */
point_3d::~point_3d() throw() {}

/**
 *  Copy operator.
 *
 *  @param[in] right The object to copy.
 *
 *  @return This object.
 */
point_3d& point_3d::operator=(point_3d const& right) {
  if (this != &right) {
    _x = right._x;
    _y = right._y;
    _z = right._z;
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
bool point_3d::operator==(point_3d const& right) const throw() {
  return (_x == right._x && _y == right._y && _z == right._z);
}

/**
 *  Not equal operator.
 *
 *  @param[in] right The opbject to compare.
 *
 *  @return True if is not the same object, otherwise false.
 */
bool point_3d::operator!=(point_3d const& right) const throw() {
  return (!operator==(right));
}

/**
 *  Less-than operator.
 *
 *  @param[in] right Object to compare to.
 *
 *  @return True if this object is less than right.
 */
bool point_3d::operator<(point_3d const& right) const throw() {
  if (_x != right._x)
    return (_x < right._x);
  else if (_y != right._y)
    return (_y < right._y);
  return (_z < right._z);
}

/**
 *  Get the x coordinates.
 *
 *  @return The x coordinates.
 */
double point_3d::x() const throw() {
  return (_x);
}

/**
 *  Get the y coordinates.
 *
 *  @return The y coordinates.
 */
double point_3d::y() const throw() {
  return (_y);
}

/**
 *  Get the z coordinates.
 *
 *  @return The z coordinates.
 */
double point_3d::z() const throw() {
  return (_z);
}
