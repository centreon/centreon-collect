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

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/logging/logger.hh"
#include "com/centreon/process.hh"
#include "com/centreon/process_listener.hh"
#include "com/centreon/process_manager.hh"

using namespace com::centreon;

// Default varibale.
static int const DEFAULT_TIMEOUT = 200;

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
    throw basic_error() << "invalid process: null pointer";

  // We lock _lock_processes before to avoid deadlocks
  std::lock_guard<std::mutex> lock(_lock_processes);

  // Check if the process need to be managed.
  std::lock_guard<std::mutex> lock_process(p->_lock_process);
  if (p->_process == static_cast<pid_t>(-1))
    throw basic_error() << "invalid process: not running";

  // Add pid process to use waitpid.
  _processes_pid[p->_process] = p;

  // Monitor err/out output if necessary.
  if (p->_enable_stream[process::out])
    _processes_fd[p->_stream[process::out]] = p;
  if (p->_enable_stream[process::err])
    _processes_fd[p->_stream[process::err]] = p;

  // Add timeout to kill process if necessary.
  if (p->_timeout)
    _processes_timeout.insert({p->_timeout, p});

  // Need to update file descriptor list.
  _update = true;
  write(_fds_exit[1], "up", 2);
}

/**
 *  Get instance of the process manager.
 *
 *  @return the process manager.
 */
process_manager& process_manager::instance() {
  static process_manager instance;
  return instance;
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
    : _thread{nullptr},
      _fds(new pollfd[64]),
      _fds_capacity(64),
      _fds_size(0),
      _update(true) {
  // Create pipe to notify ending to the process manager thread.
  if (::pipe(_fds_exit)) {
    char const* msg(strerror(errno));
    throw basic_error() << "pipe creation failed: " << msg;
  }

  process::_set_cloexec(_fds_exit[1]);

  // Add exit fd to the file descriptor list.
  _processes_fd[_fds_exit[0]] = nullptr;

  // Run process manager thread.
  _thread = new std::thread(&process_manager::_run, this);
}

/**
 *  Destructor.
 */
process_manager::~process_manager() noexcept {
  // Kill all running process.
  {
    std::lock_guard<std::mutex> lock(_lock_processes);
    for (auto it = _processes_pid.begin(), end = _processes_pid.end();
         it != end; ++it) {
      try {
        it->second->kill();
      }
      catch (std::exception const& e) {
        (void)e;
      }
    }
  }

  // Exit process manager thread.
  _close(_fds_exit[1]);

  // Waiting the end of the process manager thread.
  _thread->join();
  delete _thread;
  _thread = nullptr;

  {
    std::lock_guard<std::mutex> lock(_lock_processes);

    // Release memory.
    delete[] _fds;

    // Release ressources.
    _close(_fds_exit[0]);

    // Waiting all process.
    int ret(0);
    int status(0);
    while ((ret = ::waitpid(-1, &status, 0)) > 0 || (ret && errno == EINTR))
      ;
  }
}

/**
 *  close syscall wrapper.
 *
 *  @param[in, out] fd The file descriptor to close.
 */
void process_manager::_close(int& fd) noexcept {
  if (fd >= 0) {
    while (::close(fd) < 0 && errno == EINTR)
      std::this_thread::yield();
  }
  fd = -1;
}

/**
 *  Close stream.
 *
 *  @param[in] fd  The file descriptor to close.
 */
void process_manager::_close_stream(int fd) noexcept {
  try {
    process* p;
    // Get process to link with fd and remove this
    // fd to the process manager.
    {
      std::lock_guard<std::mutex> lock(_lock_processes);
      _update = true;
      std::unordered_map<int, process*>::iterator it(_processes_fd.find(fd));
      if (it == _processes_fd.end()) {
        _update = true;
        throw basic_error() << "invalid fd: "
                               "not found into processes fd list";
      }
      p = it->second;
      _processes_fd.erase(it);
    }

    // Update process informations.
    p->do_close(fd);
  }
  catch (std::exception const& e) {
    log_error(logging::high) << e.what();
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
  std::lock_guard<std::mutex> lock(_lock_processes);
  auto range = _processes_timeout.equal_range(p->_timeout);
  for (auto it = range.first; it != range.second; ++it) {
    if (it->second == p) {
      _processes_timeout.erase(it);
      break;
    }
  }
}

/**
 *  Kill process to reach the timeout.
 */
void process_manager::_kill_processes_timeout() noexcept {
  std::lock_guard<std::mutex> lock(_lock_processes);
  // Get the current time.
  std::time_t now(time(nullptr));

  // Kill process who timeout and remove it from timeout list.
  for (auto it = _processes_timeout.begin(), end = _processes_timeout.end(); it != end && it->first <= now; ) {
    process* p = it->second;
    try {
      p->kill();
    }
    catch (std::exception const& e) {
      log_error(logging::high) << e.what();
    }
    it = _processes_timeout.erase(it);
  }
}

/**
 *  Read stream.
 *
 *  @param[in] fd  The file descriptor to read.
 *
 *  @return Number of bytes read.
 */
unsigned int process_manager::_read_stream(int fd) noexcept {
  unsigned int size(0);
  try {
    process* p;
    // Get process to link with fd.
    {
      std::lock_guard<std::mutex> lock(_lock_processes);
      std::unordered_map<int, process*>::iterator it(_processes_fd.find(fd));
      if (it == _processes_fd.end()) {
        _update = true;
        throw basic_error() << "invalid fd: not found into processes fd list";
      }
      p = it->second;
    }

    size = p->do_read(fd);
  }
  catch (std::exception const& e) {
    log_error(logging::high) << e.what();
  }
  return size;
}

/**
 *  Internal thread to monitor processes.
 */
void process_manager::_run() {
  try {
    bool quit(false);
    while (true) {
      // Update the file descriptor list.
      _update_list();

      if (quit && _fds_size == 0)
        break;

      // Wait event on file descriptor.
      int ret(poll(_fds, _fds_size, DEFAULT_TIMEOUT));
      if (ret < 0 && errno == EINTR)
        ret = 0;
      else if (ret < 0) {
        char const* msg(strerror(errno));
        throw basic_error() << "poll failed: " << msg;
      }
      for (unsigned int i = 0, checked = 0;
           checked < static_cast<unsigned int>(ret) && i < _fds_size;
           ++i) {

        // No event.
        if (!_fds[i].revents)
          continue;

        ++checked;

        // The process manager destructor was called,
        // it's time to quit the loop.
        if (_fds[i].fd == _fds_exit[0]) {
          if (_fds[i].revents & POLLIN) {
            char buf[3];
            buf[3] = 0;
            read(_fds_exit[0], buf, 2);
            continue;
          } else {
            _processes_fd.erase(_fds[i].fd);
            _update = true;
            quit = true;
            continue;
          }
        }

        // Data are available.
        unsigned int size = 0;
        if (_fds[i].revents & (POLLIN | POLLPRI))
          size = _read_stream(_fds[i].fd);
        // File descriptor was close.
        if ((_fds[i].revents & POLLHUP) && !size)
          _close_stream(_fds[i].fd);

        //  Error!
        else if (_fds[i].revents & (POLLERR | POLLNVAL)) {
          _update = true;
          log_error(logging::high) << "invalid fd " << _fds[i].fd
                                   << " from process manager";
        }
      }
      // Release finished process.
      _wait_processes();
      _wait_orphans_pid();
      // Kill process in timeout.
      _kill_processes_timeout();
    }
  }
  catch (std::exception const& e) {
    log_error(logging::high) << e.what();
  }
}

/**
 *  Update process informations at the end of the process.
 *
 *  @param[in] p       The process to update informations.
 *  @param[in] status  The status of the process to set.
 */
void process_manager::_update_ending_process(process* p, int status) noexcept {
  // Check process viability.
  if (!p)
    return;

  p->update_ending_process(status);
  _erase_timeout(p);
}

/**
 *  Update list of file descriptor to watch.
 */
void process_manager::_update_list() {
  std::lock_guard<std::mutex> lock(_lock_processes);
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
  for (auto it = _processes_fd.begin(), end = _processes_fd.end();
       it != end; ++it) {
    _fds[_fds_size].fd = it->first;
    _fds[_fds_size].events = POLLIN | POLLPRI | POLL_HUP;
    _fds[_fds_size].revents = 0;
    ++_fds_size;
  }
  // Disable update.
  _update = false;
}

/**
 *  Waiting orphans pid.
 */
void process_manager::_wait_orphans_pid() noexcept {
  try {
    std::unique_lock<std::mutex> lock(_lock_processes);
    std::deque<orphan>::iterator it = _orphans_pid.begin();
    while (it != _orphans_pid.end()) {
      process* p(nullptr);
      // Get process to link with pid and remove this pid
      // to the process manager.
      {
        auto it_p = _processes_pid.find(it->pid);
        if (it_p == _processes_pid.end()) {
          ++it;
          continue;
        }
        p = it_p->second;
        _processes_pid.erase(it_p);
      }

      // Update process.
      lock.unlock();
      _update_ending_process(p, it->status);
      lock.lock();

      // Erase orphan pid.
      it = _orphans_pid.erase(it);
    }
  }
  catch (std::exception const& e) {
    log_error(logging::high) << e.what();
  }
}

/**
 *  Waiting finished process.
 */
void process_manager::_wait_processes() noexcept {
  try {
    while (true) {
      int status = 0;
      pid_t pid(::waitpid(-1, &status, WNOHANG));
      // No process are finished.
      if (pid <= 0)
        break;

      process* p = nullptr;
      // Get process to link with pid and remove this pid
      // to the process manager.
      {
        std::lock_guard<std::mutex> lock(_lock_processes);
        auto it = _processes_pid.find(pid);
        if (it == _processes_pid.end()) {
          _orphans_pid.push_back(orphan(pid, status));
          continue;
        }
        p = it->second;
        _processes_pid.erase(it);
      }

      // Update process.
      if (WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL)
        p->_is_timeout = true;
      _update_ending_process(p, status);
    }
  }
  catch (std::exception const& e) {
    log_error(logging::high) << e.what();
  }
}
