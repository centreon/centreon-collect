/*
** Copyright 2011 Merethis
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

#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include "com/centreon/exception/basic.hh"
#include "com/centreon/handle_manager.hh"

using namespace com::centreon;

/**
 *  Default constructor.
 *
 *  @param[in] tm  The task manager.
 */
handle_manager::handle_manager(task_manager* tm)
  : _fds(NULL),
    _should_create_fds(false),
    _task_manager(tm) {

}

/**
 *  Default destructor.
 */
handle_manager::~handle_manager() throw () {
  for (std::map<native_handle, internal_task*>::const_iterator
         it(_handles.begin()), end(_handles.end());
       it != end;
       ++it)
    delete it->second;
  delete[] _fds;
}

/**
 *  Add a new handle into the handle manager.
 *
 *  @param[in] h            The handle to manage.
 *  @param[in] hl           The listener to recive notification.
 *  @param[in] is_runnable  True if the handle listener allow to run
 *                          simultaneously with other handle listener.
 */
void handle_manager::add(
                       handle* h,
                       handle_listener* hl,
                       bool is_runnable) {
  if (!h)
    throw (basic_error() << "try to add invalid handle "        \
           "in the handle manager:null pointer");
  if (!hl)
    throw (basic_error() << "try to add invalid handle "        \
           "listener in the handle manager:nullpointer");

  // Check if the handle was already checked by the handle manager.
  if (_handles.find(h->get_internal_handle()) == _handles.end()) {
    std::pair<native_handle, internal_task*>
      item(h->get_internal_handle(),
           new internal_task(h, hl, is_runnable));
    _handles.insert(item);
    _should_create_fds = true;
  }
}

/**
 *  Set a new task manager.
 *
 *  @param[in] tm  The task manager.
 */
void handle_manager::link(task_manager* tm) {
  _task_manager = tm;
}

/**
 *  Multiplex input/output and notify handle listner if necessary.
 *  Execute the task manager at the next execution time.
 */
void handle_manager::multiplex() {
  if (!_task_manager)
    throw (basic_error() << "impossible to run multiplexing because " \
           "we don't have any task manager:null pointer");

  // Create or update pollfd.
  _create_fds();

  // Determined the poll timeout with the next execution time.
  int timeout(0);
  timestamp now(timestamp::now());
  timestamp next(_task_manager->next_execution_time());
  if (next > now)
    timeout = next.to_msecond() - now.to_msecond();

  // Wait events.
  int ret = _poll(_fds, _handles.size(), timeout);
  if (ret == -1)
    throw (basic_error() << "the handle manager multiplexing failed:"
           << strerror(errno));

  // Dispatch events.
  int nb_check(0);
  for (unsigned int i(0), end(_handles.size());
       i < end && nb_check < ret;
       ++i) {
    if (!_fds[i].revents)
      continue;
    internal_task* task(_handles[_fds[i].fd]);
    if (_fds[i].revents & (POLLIN | POLLPRI))
      task->add_action(internal_task::read);
    if (_fds[i].revents & POLLOUT)
      task->add_action(internal_task::write);
    if (_fds[i].revents & POLLHUP)
      task->add_action(internal_task::close);
    if (_fds[i].revents & (POLLERR | POLLNVAL))
      task->add_action(internal_task::error);
    _task_manager->add(task, now, task->is_runnable());
    ++nb_check;
  }

  // Flush task needs to be execute at this time.
  _task_manager->execute(timestamp::now());
}

/**
 *  Remove a specific handle.
 *
 *  @param[in] h  The handle to remove.
 *
 *  @return True if the handle was remove, otherwise false.
 */
bool handle_manager::remove(handle* h) {
  if (!h)
    return (false);

  std::map<native_handle, internal_task*>::iterator
    it(_handles.find(h->get_internal_handle()));
  if (it == _handles.end())
    return (false);
  delete it->second;
  _handles.erase(it);

  _should_create_fds = true;
  return (true);
}

/**
 *  Remove all occurence of a specific handle listener.
 *
 *  @param[in] hl  The handle listener to remove.
 *
 *  @return The number of handle listener was remove.
 */
unsigned int handle_manager::remove(handle_listener* hl) {
  if (!hl)
    return (0);

  unsigned int count_erase(0);
  for (std::map<native_handle, internal_task*>::iterator
         it(_handles.begin()), next(it), end(_handles.end());
       it != end;
       it = next) {
    ++(next = it);
    if (it->second->get_handle_listener() == hl) {
      delete it->second;
      _handles.erase(it);
      ++count_erase;
    }
  }

  if (count_erase)
    _should_create_fds = true;
  return (count_erase);
}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
handle_manager::handle_manager(handle_manager const& right) {
  _internal_copy(right);
}

/**
 *  Default copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
handle_manager& handle_manager::operator=(handle_manager const& right) {
  return (_internal_copy(right));
}

/**
 *  Internal create pollfd.
 */
void handle_manager::_create_fds() {
  if (_should_create_fds) {
    // Need to rebuild a new pollfd.
    delete[] _fds;
    _should_create_fds = false;

    if (!_handles.size()) {
      _fds = NULL;
      return;
    }
    _fds = new pollfd[_handles.size()];
  }

  // Update the pollfd.
  nfds_t nfds(0);
  for (std::map<native_handle, internal_task*>::iterator
         it(_handles.begin()), end(_handles.end());
       it != end;
       ++it) {
    _fds[nfds].fd = it->first;
    _fds[nfds].events = 0;
    _fds[nfds].revents = 0;
    handle* h(it->second->get_handle());
    handle_listener* hl(it->second->get_handle_listener());
    if (hl->want_read(*h))
      _fds[nfds].events |= POLLIN | POLLPRI;
    if (hl->want_write(*h))
      _fds[nfds].events |= POLLOUT;
  }
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
handle_manager& handle_manager::_internal_copy(handle_manager const& right) {
  (void)right;
  assert(!"impossible to copy handle_manager");
  abort();
  return (*this);
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
 *  Default constructor.
 *
 *  @param[in] h            A specific handle.
 *  @param[in] hl           A specific handle listener.
 *  @param[in] is_runnable  True if the task has the possibility to
 *                          run simultaneously.
 */
handle_manager::internal_task::internal_task(
                                 handle* h,
                                 handle_listener* hl,
                                 bool is_runnable)
  : task(),
    _action(0),
    _is_runnable(is_runnable),
    _h(h),
    _hl(hl) {

}

/**
 *  Default destructor.
 */
handle_manager::internal_task::~internal_task() throw () {

}

/**
 *  Add a new action to notify.
 *
 *  @param[in] a  The new action.
 */
void handle_manager::internal_task::add_action(action a) throw () {
  _action |= a;
}

/**
 *  Get if the task has the possibility to run simultaneously.
 *
 *  @return True is the task is thread safe, otherwise false.
 */
bool handle_manager::internal_task::is_runnable() const throw () {
  return (_is_runnable);
}

/**
 *  Get the specific handle.
 *
 *  @return The internal handle.
 */
handle* handle_manager::internal_task::get_handle() const throw () {
  return (_h);
}

/**
 *  Get the specific handle listener.
 *
 *  @return The internal handle listener.
 */
handle_listener* handle_manager::internal_task::get_handle_listener() const throw () {
  return (_hl);
}

/**
 *  Call the handle listener by action to be set.
 */
void handle_manager::internal_task::run() {
  if (_action & read)
    _hl->read(*_h);
  if (_action & write)
    _hl->write(*_h);
  if (_action & error)
    _hl->error(*_h);
  if (_action & close)
    _hl->close(*_h);
  _action = 0;
}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
handle_manager::internal_task::internal_task(internal_task const& right)
  : task() {
  _internal_copy(right);
}

/**
 *  Default copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
handle_manager::internal_task& handle_manager::internal_task::operator=(internal_task const& right) {
  return (_internal_copy(right));
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
handle_manager::internal_task& handle_manager::internal_task::_internal_copy(internal_task const& right) {
  (void)right;
  assert(!"impossible to copy handle_manager::internal_task");
  abort();
  return (*this);
}
