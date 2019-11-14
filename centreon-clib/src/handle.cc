/*
** Copyright 2011-2013 Centreon
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

#include "com/centreon/handle.hh"

using namespace com::centreon;

/**************************************
 *                                     *
 *           Public Methods            *
 *                                     *
 **************************************/

/**
 *  Default constructor.
 */
handle::handle() {
  // add line.
}

/**
 *  Copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
handle::handle(handle const& right) {
  (void)right;
}

/**
 *  Destructor.
 */
handle::~handle() noexcept {}

/**
 *  Assignment operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
handle& handle::operator=(handle const& right) {
  (void)right;
  return *this;
}
