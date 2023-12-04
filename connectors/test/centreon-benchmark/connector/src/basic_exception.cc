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

#include "com/centreon/benchmark/connector/basic_exception.hh"

using namespace com::centreon::benchmark::connector;

/**
 *  Default constructor.
 */
basic_exception::basic_exception(char const* message) : _message(message) {}

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
basic_exception::~basic_exception() throw() {}

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
char const* basic_exception::what() const throw() {
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
