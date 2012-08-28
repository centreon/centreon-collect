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
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "com/centreon/concurrency/locker.hh"
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/logging/logger.hh"
#include "com/centreon/process.hh"
#include "com/centreon/process_listener.hh"
#include "com/centreon/process_manager_posix.hh"

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
  // Check viability pointer.
  if (!p)
    throw (basic_error() << "invalid process: null pointer");

  concurrency::locker lock_process(&p->_lock_process);
  // Check if the process need to be manage.
  if (p->_process == static_cast<pid_t>(-1))
    throw (basic_error() << "invalid process: not running");

  concurrency::locker lock(&_lock_processes);
  // Add pid process to use waitpid.
  _processes_pid[p->_process] = p;

  // Monitor err/out output if necessary.
  if (p->_enable_stream[process::out])
    _processes_fd[p->_stream[process::out]] = p;
  if (p->_enable_stream[process::err])
    _processes_fd[p->_stream[process::err]] = p;

  // Add timeout to kill process if necessary.
  if (p->_timeout)
    _processes_timeout.insert(std::make_pair(p->_timeout, p));

  // Need to update file descriptor list.
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
  if (!_instance)
    _instance = new process_manager;
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
    _fds(new pollfd[64]),
    _fds_capacity(64),
    _fds_size(0),
    _update(true) {
  // Create pipe to notify ending to the process manager thread.
  if (::pipe(_fds_exit)) {
    char const* msg(strerror(errno));
    throw (basic_error() << "pipe creation failed: " << msg);
  }

  // Add exit fd to the file descriptor list.
  _processes_fd[_fds_exit[0]] = NULL;

  // Run process manager thread.
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
  // Kill all running process.
  {
    concurrency::locker lock(&_lock_processes);
    for (umap<pid_t, process*>::iterator
           it(_processes_pid.begin()), end(_processes_pid.end());
         it != end;
         ++it)
      it->second->kill();
  }

  // Exit process manager thread.
  _close(_fds_exit[1]);

  // Waiting the end of the process manager thread.
  wait();

  {
    concurrency::locker lock(&_lock_processes);

    // Release memory.
    delete[] _fds;

    // Release ressources.
    _close(_fds_exit[0]);

    // Waiting all process.
    int ret(0);
    int status(0);
    while ((ret = ::waitpid(-1, &status, 0)) > 0
           || (ret && errno == EINTR))
      ;
  }
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
 *  close syscall wrapper.
 *
 *  @param[in, out] fd The file descriptor to close.
 */
void process_manager::_close(int& fd) throw () {
  if (fd >= 0) {
    while (::close(fd) < 0 && errno == EINTR)
      ;
    fd = -1;
  }
  return;
}

/**
 *  Close stream.
 *
 *  @param[in] fd  The file descriptor to close.
 */
void process_manager::_close_stream(int fd) throw () {
  try {
    process* p(NULL);
    // Get process to link with fd and remove this
    // fd to the process manager.
    {
      concurrency::locker lock(&_lock_processes);
      _update = true;
      umap<int, process*>::iterator it(_processes_fd.find(fd));
      if (it == _processes_fd.end()) {
        _update = true;
        throw (basic_error() << "invalid fd: "
               "not found into processes fd list");
      }
      p = it->second;
      _processes_fd.erase(it);
    }

    // Update process informations.
    concurrency::locker lock(&p->_lock_process);
    if (p->_stream[process::out] == fd)
      p->_close(p->_stream[process::out]);
    else if (p->_stream[process::err] == fd)
      p->_close(p->_stream[process::err]);
    if (!p->_is_running()) {
      // Notify listener if necessary.
      if (p->_listener) {
        lock.unlock();
        (p->_listener->finished)(*p);
        lock.relock();
      }
      // Release condition variable.
      p->_cv_buffer_err.wake_one();
      p->_cv_buffer_out.wake_one();
      p->_cv_process.wake_one();
    }
  }
  catch (std::exception const& e) {
    logging::error(logging::high) << e.what();
  }
}

/**
 *  Remove process from list of processes timeout.
 *
 *  @param[in] p The process to remove.
 */
void process_manager::_erase_timeout(process* p) {
  // Check process viability.
  if (!p || !p->_timeout)
    return;
  concurrency::locker lock(&_lock_processes);
  umultimap<unsigned int, process*>::iterator
    it(_processes_timeout.find(p->_timeout));
  umultimap<unsigned int, process*>::iterator
    end(_processes_timeout.end());
  // Find and erase process from timeout list.
  while (it != end && it->first == p->_timeout) {
    if (it->second == p) {
      _processes_timeout.erase(it);
      break;
    }
    ++it;
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
 *  Kill process to reach the timeout.
 */
void process_manager::_kill_processes_timeout() throw () {
  concurrency::locker lock(&_lock_processes);
  // Get the current time.
  unsigned int now(time(NULL));
  umultimap<unsigned int, process*>::iterator
    it(_processes_timeout.begin());
  try {
    // Kill process who timeout and remove it from timeout list.
    while (it != _processes_timeout.end()
           && now >= it->first) {
      process* p(it->second);
      p->kill();
      p->_is_timeout = true;
      umultimap<unsigned int, process*>::iterator tmp(it++);
      _processes_timeout.erase(tmp);
    }
  }
  catch (std::exception const& e) {
    logging::error(logging::high) << e.what();
  }
}

/**
 *  Read stream.
 *
 *  @param[in] fd  The file descriptor to read.
 *
 *  @return Number of bytes read.
 */
unsigned int process_manager::_read_stream(int fd) throw () {
  unsigned int size(0);
  try {
    process* p(NULL);
    // Get process to link with fd.
    {
      concurrency::locker lock(&_lock_processes);
      umap<int, process*>::iterator it(_processes_fd.find(fd));
      if (it == _processes_fd.end()) {
        _update = true;
        throw (basic_error() << "invalid fd: "
               "not found into processes fd list");
      }
      p = it->second;
    }

    concurrency::locker lock(&p->_lock_process);
    // Read content of the stream and push it.
    char buffer[4096];
    size = p->_read(fd, buffer, sizeof(buffer));
    if (p->_stream[process::out] == fd) {
      p->_buffer_out.append(buffer, size);
      p->_cv_buffer_out.wake_one();
      // Notify listener if necessary.
      if (p->_listener) {
        lock.unlock();
        (p->_listener->data_is_available)(*p);
      }
    }
    else if (p->_stream[process::err] == fd) {
      p->_buffer_err.append(buffer, size);
      p->_cv_buffer_err.wake_one();
      // Notify listener if necessary.
      if (p->_listener) {
        lock.unlock();
        (p->_listener->data_is_available_err)(*p);
      }
    }
  }
  catch (std::exception const& e) {
    logging::error(logging::high) << e.what();
  }
  return (size);
}

/**
 *  Internal thread to monitor processes.
 */
void process_manager::_run() {
  try {
    while (true) {
      // Update the file descriptor list.
      _update_list();

      // Wait event on file descriptor.
      int ret(poll(_fds, _fds_size, DEFAULT_TIMEOUT));
      if (ret < 0 && errno == EINTR)
        ret = 0;
      else if (ret < 0) {
        char const* msg(strerror(errno));
        throw (basic_error() << "poll failed: " << msg);
      }
      for (unsigned int i(0), checked(0);
           checked < static_cast<unsigned int>(ret) && i < _fds_size;
           ++i) {
        // No event.
        if (!_fds[i].revents)
          continue;

        ++checked;

        // The process manager destructor was call,
        // it's time to quit the loop.
        if (_fds[i].fd == _fds_exit[0])
          return;

        // Data are available.
        unsigned int size(0);
        if (_fds[i].revents & (POLLIN | POLLPRI))
          size = _read_stream(_fds[i].fd);
        // File descriptor was close.
        if ((_fds[i].revents & POLLHUP) && !size)
          _close_stream(_fds[i].fd);
        //  Error!
        else if (_fds[i].revents & (POLLERR | POLLNVAL)) {
          _update = true;
          logging::error(logging::high)
            << "invalid fd " << _fds[i].fd << " from process manager";
        }
      }
      // Release finished process.
      _wait_processes();
      // Kill process in timeout.
      _kill_processes_timeout();
    }
  }
  catch (std::exception const& e) {
    logging::error(logging::high) << e.what();
  }
}

/**
 *  Update list of file descriptor to watch.
 */
void process_manager::_update_list() {
  concurrency::locker lock(&_lock_processes);
  // No need update.
  if (!_update)
    return;

  // Resize file descriptor list.
  if (_processes_fd.size() > _fds_capacity) {
    delete[] _fds;
    _fds_capacity = _processes_fd.size();
    _fds = new pollfd[_fds_capacity];
  }
  // Set file descriptor to wait event.
  _fds_size = 0;
  for (umap<int, process*>::const_iterator
         it(_processes_fd.begin()), end(_processes_fd.end());
       it != end;
       ++it) {
    _fds[_fds_size].fd = it->first;
    _fds[_fds_size].events = POLLIN | POLLPRI;
    _fds[_fds_size].revents = 0;
    ++_fds_size;
  }
  // Disable update.
  _update = false;
}

/**
 *  Waiting finished process.
 */
void process_manager::_wait_processes() throw () {
  try {
    while (true) {
      int status(0);
      pid_t pid(::waitpid(-1, &status, WNOHANG));
      // No process are finished.
      if (pid <= 0)
        break;

      process* p(NULL);
      // Get process to link with pid and remove this pid
      // to the process manager.
      {
        concurrency::locker lock(&_lock_processes);
        umap<pid_t, process*>::iterator it(_processes_pid.find(pid));
        if (it == _processes_pid.end())
          throw (basic_error() << "waiting process failed: " << pid
                 << " is not register");
        p = it->second;
        _processes_pid.erase(it);
      }

      // Update process informations.
      concurrency::locker lock(&p->_lock_process);
      p->_end_time = timestamp::now();
      p->_status = status;
      p->_process = static_cast<pid_t>(-1);
      p->_close(p->_stream[process::in]);
      _erase_timeout(p);
      if (!p->_is_running()) {
        // Notify listener if necessary.
        if (p->_listener) {
          lock.unlock();
          (p->_listener->finished)(*p);
          lock.relock();
        }
        // Release condition variable.
        p->_cv_buffer_err.wake_one();
        p->_cv_buffer_out.wake_one();
        p->_cv_process.wake_one();
      }
    }
  }
  catch (std::exception const& e) {
    logging::error(logging::high) << e.what();
  }
}
