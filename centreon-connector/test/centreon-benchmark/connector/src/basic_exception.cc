/*
** Copyright 2011 Merethis
**
** This file is part of Centreon Connector ICMP.
**
** Centreon Connector ICMP is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector ICMP is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector ICMP. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include "com/centreon/benchmark/connector/basic_exception.hh"

using namespace com::centreon::benchmark::connector;

/**
 *  Default constructor.
 */
basic_exception::basic_exception(char const* message)
  : _message(message) {

}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
basic_exception::basic_exception(basic_exception const& right) {
  _internal_copy(right);
}

/**
 *  Default destructor.
 */
basic_exception::~basic_exception() throw () {

}

/**
 *  Default copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
basic_exception& basic_exception::operator=(basic_exception const& right) {
  return (_internal_copy(right));
}

/**
 *  The exception message.
 *
 *  @return The message.
 */
char const* basic_exception::what() const throw () {
  return (_message);
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
basic_exception& basic_exception::_internal_copy(basic_exception const& right) {
  if (this != &right) {
    _message = right._message;
  }
  return (*this);
}
