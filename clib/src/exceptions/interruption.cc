/**
* Copyright 2014 Centreon
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

#include "com/centreon/exceptions/interruption.hh"

using namespace com::centreon::exceptions;

/**
 *  Default constructor.
 */
interruption::interruption() {}

/**
 *  Constructor with debugging informations.
 *
 *  @param[in] file      The file in which this object is created.
 *  @param[in] function  The function creating this object.
 *  @param[in] line      The line in the file creating this object.
 */
interruption::interruption(char const* file, char const* function, int line)
    : basic(file, function, line) {}

/**
 *  Copy constructor.
 *
 *  @param[in] other  Object to copy.
 */
interruption::interruption(interruption const& other) : basic(other) {}

/**
 *  Destructor.
 */
interruption::~interruption() throw() {}

/**
 *  Assignment operator.
 *
 *  @param[in] other  Object to copy.
 *
 *  @return This object.
 */
interruption& interruption::operator=(interruption const& other) {
  basic::operator=(other);
  return (*this);
}
