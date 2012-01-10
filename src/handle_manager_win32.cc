/*
** Copyright 2012 Merethis
**
** This file is part of Centreon Clib.
**
** Centreon Clib is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Clib is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Clib. If not, see
** <http://www.gnu.org/licenses/>.
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
    throw (basic_error() << "cannot multiplex if handle manager " \
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
    delete [] _array;

    // Is there any handle ?
    if (_handles.empty())
      _array = NULL;
    else {
      _array = new HANDLE[_handles.size()];
      _recreate_array = false;
    }

    // Set handles.
    unsigned int i(0);
    for (std::map<native_handle, handle_action*>::iterator
           it(_handles.begin()), end(_handles.end());
         it != end;
         ++it)
      _array[i++] = it->first;
  }

  return ;
}
