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

#include "com/centreon/handle_manager.hh"
#include <cerrno>
#include <cstring>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/handle_action.hh"
#include "com/centreon/handle_listener.hh"
#include "com/centreon/task_manager.hh"

using namespace com::centreon;

/**
 *  Constructor.
 *
 *  @param[in] tm Task manager.
 */
handle_manager::handle_manager(task_manager* tm)
    : _array(nullptr), _recreate_array{false}, _task_manager(tm) {}

/**
 *  Destructor.
 */
handle_manager::~handle_manager() noexcept {
  for (auto it = _handles.begin(), end = _handles.end(); it != end; ++it)
    try {
      if (_task_manager)
        _task_manager->remove(it->second);
      delete it->second;
    } catch (...) {
    }
  delete[] _array;
}

/**
 *  Add a new handle into the handle manager.
 *
 *  @param[in] h             The handle to listen.
 *  @param[in] hl            The listener that receives notifications.
 *  @param[in] is_threadable True if the handle listener if allowed to
 *                           run simultaneously with other listeners.
 */
void handle_manager::add(handle* h, handle_listener* hl, bool is_threadable) {
  // Check parameters.
  if (!h)
    throw(basic_error() << "attempt to add null handle in handle manager");
  if (!hl)
    throw(basic_error() << "attempt to add null listener in handle manager");

  // Check native handle.
  native_handle nh(h->get_native_handle());
  if (nh == native_handle_null)
    throw basic_error() << "attempt to add handle with invalid native "
                           "handle in the handle manager";

  // Check that handle isn't already registered.
  if (_handles.find(nh) == _handles.end()) {
    handle_action* ha = new handle_action(h, hl, is_threadable);
    std::pair<native_handle, handle_action*> item(nh, ha);
    _handles.insert(item);
    _recreate_array = true;
  } else
    throw basic_error() << "attempt to add handle "
                           "already monitored by handle manager";
}

/**
 *  Set a new task manager.
 *
 *  @param[in] tm Task manager.
 */
void handle_manager::link(task_manager* tm) {
  // Remove old tasks.
  if (_task_manager)
    for (auto it = _handles.begin(), end = _handles.end(); it != end; ++it)
      try {
        _task_manager->remove(it->second);
      } catch (...) {
      }

  // Set new task manager.
  _task_manager = tm;
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
    return false;

  // Search handle by native handle.
  std::map<native_handle, handle_action*>::iterator it(
      _handles.find(h->get_native_handle()));
  if ((it == _handles.end()) || it->second->get_handle() != h)
    return false;
  if (_task_manager)
    _task_manager->remove(it->second);
  delete it->second;
  _handles.erase(it);
  _recreate_array = true;
  return true;
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
    return 0;

  // Loop through map.
  unsigned int count_erase(0);
  for (std::map<native_handle, handle_action*>::iterator it(_handles.begin()),
       next(it), end(_handles.end());
       it != end; it = next) {
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
  return count_erase;
}

/**
 *  Multiplex input/output and notify handle listeners if necessary and
 *  execute the task manager.
 */
void handle_manager::multiplex() {
  // Check that task manager is present.
  if (!_task_manager)
    throw basic_error() << "cannot multiplex handles with no task manager";

  // Create or update pollfd.
  _setup_array();

  // Determined the poll timeout with the next execution time.
  int timeout(-1);
  timestamp now(timestamp::now());
  timestamp next(_task_manager->next_execution_time());
  if (!_handles.size() && next == timestamp::max_time())
    return;
  if (next <= now)
    timeout = 0;
  else if (next == timestamp::max_time())
    timeout = -1;
  else
    timeout = next.to_mseconds() - now.to_mseconds();

  // Wait events.
  int ret = _poll(_array, _handles.size(), timeout);
  if (ret == -1) {
    char const* msg(strerror(errno));
    throw basic_error() << "handle multiplexing failed: " << msg;
  }

  // Dispatch events.
  int nb_check(0);
  for (uint32_t i = 0, end = _handles.size(); i < end && nb_check < ret; ++i) {
    if (!_array[i].revents) {
      continue;
    }
    handle_action* task(_handles[_array[i].fd]);
    if (_array[i].revents & (POLLERR | POLLNVAL)) {
      task->set_action(handle_action::error);
    } else if (_array[i].revents & POLLOUT) {
      task->set_action(handle_action::write);
    } else if (_array[i].revents & (POLLHUP | POLLIN | POLLPRI)) {
      task->set_action(handle_action::read);
    }
    _task_manager->add(task, now, task->is_threadable());
    ++nb_check;
  }

  // Flush task needs to be execute at this time.
  _task_manager->execute(timestamp::now());
}

/**
 *  Wrapper for poll system function.
 *  @remark This function is static.
 *
 *  @param[in] fds      Set of file descriptors to be monitored.
 *  @param[in] nfds     The number of file descriptors to be monitored.
 *  @param[in] timeout  Specifies time limit for which poll will block,
 *                      in milliseconds.
 *
 *  @return A positive number on success, 0 if timeout, -1 on error and
 *          errno was set.
 */
int handle_manager::_poll(pollfd* fds, nfds_t nfds, int timeout) noexcept {
  int ret(0);
  do {
    ret = poll(fds, nfds, timeout);
  } while (ret == -1 && errno == EINTR);
  return ret;
}

/**
 *  Create or update internal pollfd array.
 */
void handle_manager::_setup_array() {
  // Should we reallocate array ?
  if (_recreate_array) {
    // Remove old array.
    delete[] _array;

    // Is there any handle ?
    if (_handles.empty())
      _array = nullptr;
    else {
      _array = new pollfd[_handles.size()];
      _recreate_array = false;
    }
  }

  // Update the pollfd.
  nfds_t nfds(0);
  for (auto it = _handles.begin(), end = _handles.end(); it != end; ++it) {
    _array[nfds].fd = it->first;
    _array[nfds].events = 0;
    _array[nfds].revents = 0;
    handle* h(it->second->get_handle());
    handle_listener* hl(it->second->get_handle_listener());
    if (hl->want_read(*h))
      _array[nfds].events |= POLLIN | POLLPRI;
    if (hl->want_write(*h))
      _array[nfds].events |= POLLOUT;
    ++nfds;
  }
}
