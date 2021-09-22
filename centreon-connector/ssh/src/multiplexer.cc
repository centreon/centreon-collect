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

#include <cassert>
#include "com/centreon/connector/ssh/multiplexer.hh"

using namespace com::centreon::connector::ssh;

// Class instance pointer.
static multiplexer* _instance = nullptr;

/**************************************
 *                                     *
 *           Public Methods            *
 *                                     *
 **************************************/

/**
 *  Get class instance.
 *
 *  @return multiplexer instance.
 */
multiplexer& multiplexer::instance() noexcept {
  assert(_instance);
  return *_instance;
}

/**
 * Load singleton.
 */
void multiplexer::load() {
  if (!_instance)
    _instance = new multiplexer;
}

/**
 * Unload singleton.
 */
void multiplexer::unload() {
  delete _instance;
  _instance = nullptr;
}

/**************************************
 *                                     *
 *           Private Methods           *
 *                                     *
 **************************************/

/**
 *  Default constructor.
 */
multiplexer::multiplexer() : com::centreon::handle_manager(this) {}
