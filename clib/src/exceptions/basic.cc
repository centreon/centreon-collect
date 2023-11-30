/**
* Copyright 2011-2014 Centreon
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

#include "com/centreon/exceptions/basic.hh"
#include <cstdio>
#include <cstring>
#include "com/centreon/misc/stringifier.hh"

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
basic::basic(char const* file, char const* function, int line) {
  *this << "[" << file << ":" << line << "(" << function << ")] ";
}

/**
 *  Copy constructor.
 *
 *  @param[in] other  Object to copy.
 */
basic::basic(basic const& other) : std::exception(other) {
  _internal_copy(other);
}

/**
 *  Destructor.
 */
basic::~basic() throw() {}

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
char const* basic::what() const throw() {
  return (_buffer.data());
}

/**
 *  Internal copy method.
 *
 *  @param[in] other  Object to copy.
 */
void basic::_internal_copy(basic const& other) {
  _buffer = other._buffer;
  return;
}
