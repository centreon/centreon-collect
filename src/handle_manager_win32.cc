/*
** Copyright 2012-2013 Centreon
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

#include <memory>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/handle_action.hh"
#include "com/centreon/handle_manager_win32.hh"

using namespace com::centreon;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Multiplex input/output and notify handle listeners if necessary and
 *  execute the task manager.
 */
void handle_manager::multiplex() {
  // Check that task manager is set.
  if (!_task_manager)
    throw(basic_error() << "cannot multiplex if handle manager "
                           "has no task manager");
}

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Create or update internal HANDLE array.
 */
void handle_manager::_setup_array() {
  // Should we reallocate the array ?
  if (_recreate_array) {
    // Remove old array.
    delete[] _array;

    // Is there any handle ?
    if (_handles.empty())
      _array = NULL;
    else {
      _array = new HANDLE[_handles.size()];
      _recreate_array = false;
    }

    // Set handles.
    unsigned int i(0);
    for (std::map<native_handle, handle_action*>::iterator it(_handles.begin()),
         end(_handles.end());
         it != end;
         ++it)
      _array[i++] = it->first;
  }

  return;
}
