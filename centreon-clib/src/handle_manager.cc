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

#include <memory>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/handle_action.hh"
#include "com/centreon/handle_manager.hh"
#include "com/centreon/task_manager.hh"

using namespace com::centreon;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Constructor.
 *
 *  @param[in] tm Task manager.
 */
handle_manager::handle_manager(task_manager* tm)
  : _array(NULL),
    _recreate_array(false),
    _task_manager(tm) {}

/**
 *  Copy constructor.
 *
 *  @param[in] hm Object to copy.
 */
handle_manager::handle_manager(handle_manager const& hm) {
  _internal_copy(hm);
}

/**
 *  Destructor.
 */
handle_manager::~handle_manager() throw () {
  for (std::map<native_handle, handle_action*>::const_iterator
         it(_handles.begin()), end(_handles.end());
       it != end;
       ++it)
    try {
      if (_task_manager)
        _task_manager->remove(it->second);
      delete it->second;
    }
    catch (...) {}
  delete [] _array;
}

/**
 *  Assignment operator.
 *
 *  @param[in] hm Object to copy.
 *
 *  @return This object.
 */
handle_manager& handle_manager::operator=(handle_manager const& hm) {
  if (this != &hm)
    _internal_copy(hm);
  return (*this);
}

/**
 *  Add a new handle into the handle manager.
 *
 *  @param[in] h             The handle to listen.
 *  @param[in] hl            The listener that receives notifications.
 *  @param[in] is_threadable True if the handle listener if allowed to
 *                           run simultaneously with other listeners.
 */
void handle_manager::add(
                       handle* h,
                       handle_listener* hl,
                       bool is_threadable) {
  // Check parameters.
  if (!h)
    throw (basic_error()
           << "attempt to add null handle in handle manager");
  if (!hl)
    throw (basic_error()
           << "attempt to add null listener in handle manager");

  // Check native handle.
  native_handle nh(h->get_native_handle());
  if (nh == native_handle_null)
    throw (basic_error()
           << "attempt to add handle with invalid native " \
              "handle in the handle manager");

  // Check that handle isn't already registered.
  if (_handles.find(nh) == _handles.end()) {
    std::unique_ptr<handle_action>
      ha(new handle_action(h, hl, is_threadable));
    std::pair<native_handle, handle_action*> item(nh, ha.get());
    _handles.insert(item);
    ha.release();
    _recreate_array = true;
  }
  else
    throw (basic_error() << "attempt to add handle " \
           "already monitored by handle manager");

  return ;
}

/**
 *  Set a new task manager.
 *
 *  @param[in] tm Task manager.
 */
void handle_manager::link(task_manager* tm) {
  // Remove old tasks.
  if (_task_manager)
    for (std::map<native_handle, handle_action*>::iterator
           it(_handles.begin()), end(_handles.end());
         it != end;
         ++it)
      try {
        _task_manager->remove(it->second);
      }
      catch (...) {}

  // Set new task manager.
  _task_manager = tm;

  return ;
}

/**
 *  Remove a specific handle.
 *
 *  @param[in] h  The handle to remove.
 *
 *  @return True if the handle was removed, false otherwise.
 */
bool handle_manager::remove(handle* h) {
  // Beware null pointer.
  if (!h)
    return (false);

  // Search handle by native handle.
  std::map<native_handle, handle_action*>::iterator
    it(_handles.find(h->get_native_handle()));
  if ((it == _handles.end()) || it->second->get_handle() != h)
    return (false);
  if (_task_manager)
    _task_manager->remove(it->second);
  delete it->second;
  _handles.erase(it);
  _recreate_array = true;
  return (true);
}

/**
 *  Remove all occurence of a specific handle listener.
 *
 *  @param[in] hl  The handle listener to remove.
 *
 *  @return The number of items removed.
 */
unsigned int handle_manager::remove(handle_listener* hl) {
  // Beware null pointer.
  if (!hl)
    return (0);

  // Loop through map.
  unsigned int count_erase(0);
  for (std::map<native_handle, handle_action*>::iterator
         it(_handles.begin()), next(it), end(_handles.end());
       it != end;
       it = next) {
    ++(next = it);
    if (it->second->get_handle_listener() == hl) {
      if (_task_manager)
        _task_manager->remove(it->second);
      delete it->second;
      _handles.erase(it);
      ++count_erase;
    }
  }
  _recreate_array = true;
  return (count_erase);
}

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Copy internal data members.
 *
 *  @param[in] hm Object to copy.
 */
void handle_manager::_internal_copy(handle_manager const& hm) {
  link(hm._task_manager); // Will remove tasks only, not register.
  delete [] _array;
  _array = NULL;
  _recreate_array = true;
  _handles = hm._handles;
  return ;
}
