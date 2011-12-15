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

#include "com/centreon/logging/verbosity.hh"

using namespace com::centreon::logging;

/**
 *  Default constrcutor.
 *
 *  @param[in] val  The verbosity level.
 */
verbosity::verbosity(unsigned int val)
  : _val(val) {

}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
verbosity::verbosity(verbosity const& right) {
  _internal_copy(right);
}

/**
 *  Default destructor.
 */
verbosity::~verbosity() throw () {

}

/**
 *  Default copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
verbosity& verbosity::operator=(verbosity const& right) {
  return (_internal_copy(right));
}

/**
 *  Default cast operator.
 */
verbosity::operator unsigned int() {
  return (_val);
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
verbosity& verbosity::_internal_copy(verbosity const& right) {
  if (this != &right) {
    _val = right._val;
  }
  return (*this);
}

/**
 *  Equal operator.
 *
 *  @param[in] right  The verbosity to compare.
 *
 *  @return True if the same object, otherwise false.
 */
bool verbosity::operator==(verbosity const& right) const throw () {
  return (_val == right._val);
}

/**
 *  Not equal operator.
 *
 *  @param[in] right  The verbosity to compare.
 *
 *  @return True if not the same object, otherwise false.
 */
bool verbosity::operator!=(verbosity const& right) const throw () {
  return (_val != right._val);
}

/**
 *  Less operator
 *
 *  @param[in] right  The verbosity to compare.
 *
 *  @return True if object is less than the other, otherwise false.
 */
bool verbosity::operator<(verbosity const& right) const throw () {
  return (_val < right._val);
}

/**
 *  Less or equal operator.
 *
 *  @param[in] right  The verbosity to compare.
 *
 *  @return True if object is less or equal than the other,
 *               otherwise false.
 */
bool verbosity::operator<=(verbosity const& right) const throw () {
  return (_val <= right._val);
}

/**
 *  Greater operator.
 *
 *  @param[in] right  The verbosity to compare.
 *
 *  @return True if object is greater than the other, otherwise false.
 */
bool verbosity::operator>(verbosity const& right) const throw () {
  return (_val > right._val);
}

/**
 *  Greater or equal operator.
 *
 *  @param[in] right  The verbosity to compare.
 *
 *  @return True if object is greater or equal than the other,
 *          otherwise false.
 */
bool verbosity::operator>=(verbosity const& right) const throw () {
  return (_val >= right._val);
}
