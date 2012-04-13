/*
** Copyright 2011-2012 Merethis
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

#include <errno.h>
#include <memory>
#include <string.h>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/handle_action.hh"
#include "com/centreon/handle_listener.hh"
#include "com/centreon/handle_manager_posix.hh"
#include "com/centreon/task_manager.hh"
#include "com/centreon/timestamp.hh"

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
  // Check that task manager is present.
  if (!_task_manager)
    throw (basic_error()
           << "cannot multiplex handles with no task manager");

  // Create or update pollfd.
  _setup_array();

  // Determined the poll timeout with the next execution time.
  int timeout(-1);
  timestamp now(timestamp::now());
  timestamp next(_task_manager->next_execution_time());
  if (!_handles.size() && next == timestamp::max())
    return ;
  if (next <= now)
    timeout = 0;
  else if (next == timestamp::max())
    timeout = -1;
  else
    timeout = next.to_mseconds() - now.to_mseconds();

  // Wait events.
  int ret = _poll(_array, _handles.size(), timeout);
  if (ret == -1) {
    char const* msg(strerror(errno));
    throw (basic_error() << "handle multiplexing failed: " << msg);
  }

  // Dispatch events.
  int nb_check(0);
  for (unsigned int i(0), end(_handles.size());
       i < end && nb_check < ret;
       ++i) {
    if (!_array[i].revents)
      continue;
    handle_action* task(_handles[_array[i].fd]);
    if (_array[i].revents & (POLLERR | POLLNVAL))
      task->set_action(handle_action::error);
    else if (_array[i].revents & POLLOUT)
      task->set_action(handle_action::write);
    else if (_array[i].revents & (POLLHUP | POLLIN | POLLPRI))
      task->set_action(handle_action::read);
    _task_manager->add(task, now, task->is_threadable());
    ++nb_check;
  }

  // Flush task needs to be execute at this time.
  _task_manager->execute(timestamp::now());
}

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

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
int handle_manager::_poll(
                      pollfd *fds,
                      nfds_t nfds,
                      int timeout) throw () {
  int ret(0);
  do {
    ret = poll(fds, nfds, timeout);
  } while (ret == -1 && errno == EINTR);
  return (ret);
}

/**
 *  Create or update internal pollfd array.
 */
void handle_manager::_setup_array() {
  // Should we reallocate array ?
  if (_recreate_array) {
    // Remove old array.
    delete [] _array;

    // Is there any handle ?
    if (_handles.empty())
      _array = NULL;
    else {
      _array = new pollfd[_handles.size()];
      _recreate_array = false;
    }
  }

  // Update the pollfd.
  nfds_t nfds(0);
  for (std::map<native_handle, handle_action*>::iterator
         it(_handles.begin()), end(_handles.end());
       it != end;
       ++it) {
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

  return ;
}
