/*
** Copyright 2011-2014 Merethis
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

#include <cstdio>
#include <cstring>
#include "com/centreon/misc/stringifier.hh"
#include "com/centreon/exceptions/basic.hh"

using namespace com::centreon::exceptions;

/**
 *  Default constructor.
 */
basic::basic() {}

/**
 *  Constructor with debugging informations.
 *
 *  @param[in] file      The file from calling this object.
 *  @param[in] function  The function from calling this object.
 *  @param[in] line      The line from calling this object.
 */
basic::basic(
         char const* file,
         char const* function,
         int line) {
  *this << "[" << file << ":" << line << "(" << function << ")] ";
}

/**
 *  Copy constructor.
 *
 *  @param[in] other  Object to copy.
 */
basic::basic(basic const& other)
  : std::exception(other) {
  _internal_copy(other);
}

/**
 *  Destructor.
 */
basic::~basic() throw () {}

/**
 *  Assignment operator.
 *
 *  @param[in] other  Object to copy.
 *
 *  @return This object.
 */
basic& basic::operator=(basic const& other) {
  if (this != &other) {
    std::exception::operator=(other);
    _internal_copy(other);
  }
  return (*this);
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
 *  @param[in] other  Object to copy.
 */
void basic::_internal_copy(basic const& other) {
  _buffer = other._buffer;
  return ;
}
