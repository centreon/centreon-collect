/*
** Copyright 2011 Merethis
** This file is part of Centreon Connector SSH.
**
** Centreon Connector SSH is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector SSH is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector SSH. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <string.h>
#include "com/centreon/connector/ssh/exception.hh"

using namespace com::centreon::connector::ssh;

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Convert a numeric value to a string.
 *
 *  @param[in] format printf format associated with t.
 *  @param[in] t      Numeric value to append to the buffer.
 *
 *  @return Current instance.
 */
template <typename T>
exception& exception::_conversion(char const* format, T t) throw () {
  int length(sizeof(_buffer) - _current);
  int retval(snprintf(_buffer + _current, length, format, t));
  if (retval > 0)
    _current += ((retval > length) ? length : retval);
  _buffer[_current] = '\0';
  return (*this);
}

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Default constructor.
 */
exception::exception() : _current(0) {
  _buffer[_current] = '\0';
}

/**
 *  Copy constructor.
 *
 *  @param[in] e Object to copy.
 */
exception::exception(exception const& e)
  : std::exception(), _current(e._current) {
  memcpy(_buffer, e._buffer, (_current + 1) * sizeof(*_buffer));
}

/**
 *  Destructor.
 */
exception::~exception() throw () {}

/**
 *  Assignment operator.
 *
 *  @param[in] e Object to copy.
 *
 *  @return This object.
 */
exception& exception::operator=(exception const& e) {
  _current = e._current;
  memcpy(_buffer, e._buffer, (_current + 1) * sizeof(*_buffer));
  return (*this);
}

/**
 *  Append a string to the error message.
 *
 *  @param[in] str String to append.
 *
 *  @return This object.
 */
exception& exception::operator<<(char const* str) throw () {
  // Avoid NULL pointer.
  if (!str)
    str = "(NULL)";

  // Size to copy.
  unsigned int size(strlen(str));

  // Check against buffer limit.
  {
    unsigned int limit((sizeof(_buffer) / sizeof(*_buffer))
      - 1
      - _current);
    if (size > limit)
      size = limit;
  }

  // Copy data.
  memcpy(_buffer + _current, str, size);
  _current += size;
  _buffer[_current] = '\0';

  return (*this);
}

/**
 *  Append a string to the error message.
 *
 *  @param[in] str String to append.
 *
 *  @return This object.
 */
exception& exception::operator<<(std::string const& str) throw () {
  // Get string pointer.
  char const* ptr;
  try {
    ptr = str.c_str();
  }
  catch (...) {
    ptr = "(not enough memory)";
  }

  // Append string.
  this->operator<<(ptr);

  return (*this);
}

/**
 *  Append an integer to the error message.
 *
 *  @param[in] i Integer to append.
 *
 *  @return This object.
 */
exception& exception::operator<<(int i) throw () {
  return (_conversion("%d", i));
}

/**
 *  Get exception message.
 *
 *  @return Exception message.
 */
char const* exception::what() const throw () {
  return (_buffer);
}
