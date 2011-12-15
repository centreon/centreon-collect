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

#include <stdio.h>
#include <string.h>
#include "com/centreon/misc/stringifier.hh"
#include "com/centreon/exception/basic.hh"

using namespace com::centreon::exception;

/**
 *  Default constructor.
 */
basic::basic() throw () {

}

/**
 *  Constructor with debugging informations.
 *
 *  @param[in] file      The file from calling this object.
 *  @param[in] function  The function from calling this object.
 *  @param[in] line      The line from calling this object.
 */
basic::basic(char const* file, char const* function, int line) throw () {
  *this << "[" << file << ":" << line << "(" << function << ")] ";
}

/**
 *  Copy constructor.
 *
 *  @param[in] right  Object to copy.
 */
basic::basic(basic const& right) throw ()
  : std::exception(right) {
  _internal_copy(right);
}

/**
 *  Destructor.
 */
basic::~basic() throw () {

}

/**
 *  Assignment operator.
 *
 *  @param[in] right  Object to copy.
 *
 *  @return This object.
 */
basic& basic::operator=(basic const& right) throw () {
  return (_internal_copy(right));
}

/**
 *  Get the basic message.
 *
 *  @return Basic message.
 */
char const* basic::what() const throw () {
  return (_buffer.data());
}

/**
 *  Internal copy method.
 *
 *  @param[in] right  Object to copy.
 *
 *  @return This object.
 */
basic& basic::_internal_copy(basic const& right) {
  if (this != &right) {
    std::exception::operator=(right);
    _buffer = right._buffer;
  }
  return (*this);
}
