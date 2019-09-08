/*
** Copyright 2017 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#include "com/centreon/broker/exceptions/interrupt.hh"

using namespace com::centreon::broker::exceptions;

/**
 *  Default constructor.
 */
interrupt::interrupt() {}

/**
 *  Copy constructor.
 *
 *  @param[in] other  Object to build from.
 */
interrupt::interrupt(interrupt const& other)
    : misc::stringifier(other), std::exception(other) {}

/**
 *  Destructor.
 */
interrupt::~interrupt() throw() {}

/**
 *  Assignment operator overload.
 *
 *  @param[in] other  Object to copy from.
 *
 *  @return This instance.
 */
interrupt& interrupt::operator=(interrupt const& other) {
  if (this != &other) {
    misc::stringifier::operator=(other);
    std::exception::operator=(other);
  }
  return (*this);
}

/**
 *  Get message associated with the exception.
 *
 *  @return Nul-terminated message.
 */
char const* interrupt::what() const throw() {
  return (misc::stringifier::data());
}
