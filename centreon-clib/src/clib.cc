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

#include <cstdlib>
#include "com/centreon/clib.hh"
#include "com/centreon/logging/engine.hh"
#include "com/centreon/process_manager.hh"

using namespace com::centreon;

// Class instance.
static clib* _instance = NULL;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Load the clib singleton.
 *
 *  @param[in] flags Specify which elements should be loaded.
 */
void clib::load(unsigned int flags) {
  delete _instance;
  _instance = NULL;
  _instance = new clib(flags);
  return;
}

/**
 *  Unload the clib singleton.
 */
void clib::unload() {
  delete _instance;
  _instance = NULL;
  return;
}

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Constructor.
 *
 *  @param[in] flags Specify which elements to load.
 */
clib::clib(unsigned int flags) {
  if (flags & with_logging_engine)
    logging::engine::load();
  if (flags & with_process_manager)
    process_manager::load();
}

/**
 *  Destructor.
 */
clib::~clib() throw () {
  process_manager::unload();
  logging::engine::unload();
}
