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
 *  Destructor.
 */
process_manager::~process_manager() throw () {
  _quit = true;
  wait();
  delete _fds;
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
