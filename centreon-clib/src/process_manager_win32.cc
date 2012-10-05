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

#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include "com/centreon/concurrency/locker.hh"
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/logging/logger.hh"
#include "com/centreon/process.hh"
#include "com/centreon/process_listener.hh"
#include "com/centreon/process_manager_win32.hh"

using namespace com::centreon;

// Default varibale.
static int const DEFAULT_TIMEOUT = 200;

// Class instance.
static process_manager* _instance = NULL;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Add process to the process manager.
 *
 *  @param[in] p    The process to manage.
 *  @param[in] obj  The object to notify.
 */
void process_manager::add(process* p) {
  if (!p)
    throw (basic_error() << "invalid process: null pointer");

  concurrency::locker lock_process(&p->_lock_process);
  if (p->_process == static_cast<pid_t>(-1))
    throw (basic_error() << "invalid process: not running");

  concurrency::locker lock(&_lock_processes);
  _processes_pid[p->_process] = p;
  if (p->_enable_stream[process::out])
    _processes_fd[p->_stream[process::out]] = p;
  if (p->_enable_stream[process::err])
    _processes_fd[p->_stream[process::err]] = p;
  _update = true;
  return;
}

/**
 *  Get instance of the process manager.
 *
 *  @return the process manager.
 */
process_manager& process_manager::instance() {
  return (*_instance);
}

/**
 *  Load the process manager.
 */
void process_manager::load() {
  delete _instance;
  _instance = new process_manager();
  return;
}

/**
 *  Unload the process manager.
 */
void process_manager::unload() {
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
 *  Default constructor.
 */
process_manager::process_manager()
  : concurrency::thread(),
    _fds(NULL),
    _fds_capacity(64),
    _fds_size(0),
    _quit(false),
    _update(false) {
  exec();
}

/**
 *  Copy constructor.
 *
 *  @param[in] right Object to copy.
 */
process_manager::process_manager(process_manager const& right)
  : concurrency::thread() {
  _internal_copy(right);
}

/**
 *  Destructor.
 */
process_manager::~process_manager() throw () {
  _quit = true;
  wait();
  delete _fds;
}

/**
 *  Assignment operator.
 *
 *  @param[in] right Object to copy.
 *
 *  @return This object.
 */
process_manager& process_manager::operator=(process_manager const& right) {
  _internal_copy(right);
  return (*this);
}

/**
 *  Close stream.
 *
 *  @param[in] fd  The file descriptor to close.
 */
void process_manager::_close_stream(HANDLE fd) throw () {
  try {
    process* p(NULL);
    {
      concurrency::locker lock(&_lock_processes);
      _update = true;
      htable<int, process*>::iterator it(_processes_fd.find(fd));
      if (it == _processes_fd.end()) {
        _update = true;
        throw (basic_error() << "invalid fd: "
               "not found into processes fd list");
      }
      p = it->second;
      _processes_fd.erase(it);
    }

    concurrency::locker lock(&p->_lock_process);
    if (p->_stream[process::out] == fd)
      p->_close(p->_stream[process::out]);
    else if (p->_stream[process::err] == fd)
      p->_close(p->_stream[process::err]);
    if (!p->_is_running()) {
      p->_cv_process.wake_one();
      if (p->_listener)
        (p->_listener->finished)(*p);
    }
  }
  catch (std::exception const& e) {
    log_error(logging::high) << e.what();
  }
}


/**
 *  Copy internal data members.
 *
 *  @param[in] p Object to copy.
 */
void process_manager::_internal_copy(process_manager const& right) {
  (void)right;
  assert(!"process_manager is not copyable");
  abort();
  return;
}

/**
 *  Read stream.
 *
 *  @param[in] fd  The file descriptor to read.
 */
void process_manager::_read_stream(HANDLE fd) throw () {
  try {
    process* p(NULL);
    {
      concurrency::locker lock(&_lock_processes);
      htable<int, process*>::iterator it(_processes_fd.find(fd));
      if (it == _processes_fd.end()) {
        _update = true;
        throw (basic_error() << "invalid fd: "
               "not found into processes fd list");
      }
      p = it->second;
    }

    concurrency::locker lock(&p->_lock_process);
    char buffer[4096];
    unsigned int size(p->_read(fd, buffer, sizeof(buffer)));
    if (p->_stream[process::out] == fd)
      p->_buffer_out.append(buffer, size);
    else if (p->_stream[process::err] == fd)
      p->_buffer_err.append(buffer, size);
    p->_cv_process.wake_one();
  }
  catch (std::exception const& e) {
    logerror(logging::high) << e.what();
  }
}

/**
 *  Internal thread to monitor processes.
 */
void process_manager::_run() {
  try {
    while (!_quit) {
      _update_list();
      if (!_fds_size) {
        concurrency::thread::msleep(DEFAULT_TIMEOUT);
        continue;
      }
      // XXX: todo manage handle (WaitForMultipleObjects).
      throw (basic_error() << "process_manager dosn't work on windows");
      _wait_processes();
    }
  }
  catch (std::exception const& e) {
    log_error(logging::high) << e.what();
  }
}

/**
 *  Update list of file descriptor to watch.
 */
void process_manager::_update_list() {
  concurrency::locker lock(&_lock_processes);
  if (!_update)
    return;

  if (_processes_fd.size() > _fds_capacity) {
    delete[] _fds;
    _fds = new pollfd[_processes_fd.size()];
  }
  _fds_size = 0;
  for (htable<int, process*>::const_iterator
         it(_processes_fd.begin()), end(_processes_fd.end());
       it != end;
       ++it) {
    // XXX: todo make handle information for manage these.
    ++_fds_size;
  }
  _update = false;
}

/**
 *  Waiting finished process.
 */
void process_manager::_wait_processes() throw () {
  try {
    // XXX: catch ending process.
  }
  catch (std::exception const& e) {
    log_error(logging::high) << e.what();
  }
}
