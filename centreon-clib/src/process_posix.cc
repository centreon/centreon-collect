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

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#ifdef HAVE_SPAWN_H
#  include <spawn.h>
#endif // HAVE_SPAWN_H
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include "com/centreon/concurrency/locker.hh"
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/misc/command_line.hh"
#include "com/centreon/process_manager_posix.hh"
#include "com/centreon/process_posix.hh"

using namespace com::centreon;

// environ is not declared on *BSD.
extern char** environ;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Default constructor.
 */
process::process(process_listener* listener)
  : _is_timeout(false),
    _listener(listener),
    _process(static_cast<pid_t>(-1)),
    _status(0),
    _timeout(0) {
  memset(_enable_stream, 1, sizeof(_enable_stream));
  memset(_stream, -1, sizeof(_stream));
}

/**
 *  Destructor.
 */
process::~process() throw () {
  kill();
  wait();
}

/**
 *  Enable or disable process' stream.
 *
 *  @param[in] s      The stream to set.
 *  @param[in] enable Set to true to enable stderr.
 */
void process::enable_stream(stream s, bool enable) {
  concurrency::locker lock(&_lock_process);
  if (_enable_stream[s] != enable) {
    // Process not running juste set variable.
    if (!_is_running())
      _enable_stream[s] = enable;
    // Process running and stream is enable, close stream.
    else if (!enable)
      _close(_stream[s]);
    // Do not change stream status.
    else
      throw (basic_error() << "cannot reenable \""
             << s << "\" while process is running");
  }
  return;
}

/**
 *  Get the time when the process execution finished.
 *
 *  @return The ending timestamp.
 */
timestamp const& process::end_time() const throw () {
  concurrency::locker lock(&_lock_process);
  return (_end_time);
}

/**
 *  Run process.
 *
 *  @param[in] cmd     Command line.
 *  @param[in] env     Array of strings (on form key=value), which are
 *                     passed as environment to the new process. If env
 *                     is NULL, the current environement are passed of
 *                     the new process.
 *  @param[in] timeout Maximum time in seconde to execute process. After
 *                     this time the process will be kill.
 */
void process::exec(char const* cmd, char** env, unsigned int timeout) {
  concurrency::locker lock(&_lock_process);

  // Check if process already running.
  if (_is_running())
    throw (basic_error() << "process " << _process
           << " is already started and has not been waited");

  // Reset variable.
  _buffer_err.clear();
  _buffer_out.clear();
  _end_time.clear();
  _is_timeout = false;
  _start_time.clear();
  _status = 0;

  // Close the last file descriptor;
  for (unsigned int i(0); i < 3; ++i)
    _close(_stream[i]);

  // Init file desciptor.
  int std[3] = { -1, -1, -1 };
  int pipe_stream[3][2] = {
    { -1, -1 },
    { -1, -1 },
    { -1, -1 }
  };

  // volatile prevent compiler optimization that might clobber variable.
  volatile bool restore_std(false);

  try {
    // Create backup FDs.
    std[0] = _dup(STDIN_FILENO);
    std[1] = _dup(STDOUT_FILENO);
    std[2] = _dup(STDERR_FILENO);

    // Backup FDs do not need to be inherited.
    for (unsigned int i(0); i < 3; ++i)
      _set_cloexec(std[i]);

    restore_std = true;

    // Create pipes if necessary and duplicate FDs.
    if (!_enable_stream[in])
      _dev_null(STDIN_FILENO, O_RDONLY);
    else {
      _pipe(pipe_stream[in]);
      _dup2(pipe_stream[in][0], STDIN_FILENO);
      _close(pipe_stream[in][0]);
      _set_cloexec(pipe_stream[in][1]);
    }
    if (!_enable_stream[out])
      _dev_null(STDOUT_FILENO, O_WRONLY);
    else {
      _pipe(pipe_stream[out]);
      _dup2(pipe_stream[out][1], STDOUT_FILENO);
      _close(pipe_stream[out][1]);
      _set_cloexec(pipe_stream[out][0]);
    }
    if (!_enable_stream[err])
      _dev_null(STDERR_FILENO, O_WRONLY);
    else {
      _pipe(pipe_stream[err]);
      _dup2(pipe_stream[err][1], STDERR_FILENO);
      _close(pipe_stream[err][1]);
      _set_cloexec(pipe_stream[err][0]);
    }

    // Parse and get command line arguments.
    misc::command_line cmdline(cmd);
    char** args(cmdline.get_argv());

    // volatile prevent compiler optimization
    // that might clobber variable.
    char** volatile my_env(env ? env : environ);

#ifdef HAVE_SPAWN_H
    // Create new process.
    if (posix_spawnp(&_process, args[0], NULL, NULL, args, my_env)) {
      char const* msg(strerror(errno));
      throw (basic_error() << "could not create process: " << msg);
    }
#else
    // Create new process.
    _process = vfork();
    if (_process == static_cast<pid_t>(-1)) {
      char const* msg(strerror(errno));
      throw (basic_error() << "could not create process: " << msg);
    }

    // Child execution.
    if (!_process) {
      ::execve(args[0], args, my_env);
      ::_exit(EXIT_FAILURE);
    }
#endif // spawn.h or not

    // Parent execution.
    _start_time = timestamp::now();
    _timeout = (timeout ? time(NULL) + timeout : 0);

    // Restore original FDs.
    _dup2(std[0], STDIN_FILENO);
    _dup2(std[1], STDOUT_FILENO);
    _dup2(std[2], STDERR_FILENO);
    for (unsigned int i(0); i < 3; ++i) {
      _close(std[i]);
      _close(pipe_stream[i][i == in ? 0 : 1]);
      _stream[i] = pipe_stream[i][i == in ? 1 : 0];
    }

    // Add process to the process manager.
    process_manager::instance().add(this);
  }
  catch (...) {
    // Restore original FDs.
    if (restore_std) {
      _dup2(std[0], STDIN_FILENO);
      _dup2(std[1], STDOUT_FILENO);
      _dup2(std[2], STDERR_FILENO);
    }

    // Close all file descriptor.
    for (unsigned int i(0); i < 3; ++i) {
      _close(std[i]);
      _close(_stream[i]);
      for (unsigned int j(0); j < 2; ++j)
        _close(pipe_stream[i][j]);
    }
    throw;
  }
  return;
}

/**
 *  Run process.
 *
 *  @param[in] cmd     Command line.
 *  @param[in] timeout Maximum time in seconde to execute process. After
 *                     this time the process will be kill.
 */
void process::exec(std::string const& cmd, unsigned int timeout) {
  exec(cmd.c_str(), NULL, timeout);
  return;
}

/**
 *  Get the exit code, return by the executed process.
 *
 *  @return The exit code.
 */
int process::exit_code() const throw () {
  concurrency::locker lock(&_lock_process);
  if (WIFEXITED(_status))
    return (WEXITSTATUS(_status));
  return (0);
}

/**
 *  Get the exit status, return normal if the executed process end
 *  normaly, return crash if the executed process terminated abnormaly
 *  or return timeout.
 *
 *  @return The exit status.
 */
process::status process::exit_status() const throw () {
  concurrency::locker lock(&_lock_process);
  if (_is_timeout)
    return (timeout);
  if (WIFEXITED(_status))
    return (normal);
  return (crash);
}

/**
 *  Kill process.
 */
void process::kill() {
  concurrency::locker lock(&_lock_process);
  _kill(SIGKILL);
  return;
}

/**
 *  Read data from stdout.
 *
 *  @param[out] data Destination buffer.
 */
void process::read(std::string& data) {
  concurrency::locker lock(&_lock_process);
  // If buffer is empty and stream is open, we waiting data.
  if (_buffer_out.empty() && _stream[out] != -1)
    _cv_buffer_out.wait(&_lock_process);
  // Switch content.
  data.clear();
  data.swap(_buffer_out);
  return;
}

/**
 *  Read data from stderr.
 *
 *  @param[out] data Destination buffer.
 */
void process::read_err(std::string& data) {
  concurrency::locker lock(&_lock_process);
  // If buffer is empty and stream is open, we waiting data.
  if (_buffer_err.empty() && _stream[err] != -1)
    _cv_buffer_err.wait(&_lock_process);
  // Switch content.
  data.clear();
  data.swap(_buffer_err);
  return;
}

/**
 *  Get the time when the process execution start.
 *
 *  @return The starting timestamp.
 */
timestamp const& process::start_time() const throw () {
  concurrency::locker lock(&_lock_process);
  return (_start_time);
}

/**
 *  Terminate process.
 */
void process::terminate() {
  concurrency::locker lock(&_lock_process);
  _kill(SIGTERM);
  return;
}

/**
 *  Wait for process termination.
 */
void process::wait() const {
  concurrency::locker lock(&_lock_process);
  if (_is_running())
    _cv_process.wait(&_lock_process);
  return;
}

/**
 *  Wait for process termination.
 *
 *  @param[in]  timeout Maximum number of milliseconds to wait for
 *                      process termination.
 *
 *  @return true if process exited.
 */
bool process::wait(unsigned long timeout) const {
  concurrency::locker lock(&_lock_process);
  if (!_is_running())
    return (true);
  _cv_process.wait(&_lock_process, timeout);
  return (!_is_running());
}

/**
 *  Write data to process' standard input.
 *
 *  @param[in] data Source buffer.
 *
 *  @return Number of bytes actually written.
 */
unsigned int process::write(std::string const& data) {
  return (write(data.c_str(), data.size()));
}

/**
 *  Write data to process' standard input.
 *
 *  @param[in] data Source buffer.
 *  @param[in] size Maximum number of bytes to write.
 *
 *  @return Number of bytes actually written.
 */
unsigned int process::write(void const* data, unsigned int size) {
  concurrency::locker lock(&_lock_process);
  ssize_t wb(::write(_stream[in], data, size));
  if (wb < 0) {
    char const* msg(strerror(errno));
    throw (basic_error() << "could not write on process "
           << _process << "'s input: " << msg);
  }
  return (wb);
}

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Copy constructor.
 *
 *  @param[in] p Object to copy.
 */
process::process(process const& p) {
  _internal_copy(p);
}

/**
 *  Assignment operator.
 *
 *  @param[in] p Object to copy.
 *
 *  @return This object.
 */
process& process::operator=(process const& p) {
  _internal_copy(p);
  return (*this);
}

/**
 *  close syscall wrapper.
 *
 *  @param[in, out] fd The file descriptor to close.
 */
void process::_close(int& fd) throw () {
  if (fd >= 0) {
    ::close(fd);
    fd = -1;
  }
  return;
}

/**
 *  Open /dev/null and duplicate file descriptor.
 *
 *  @param[in] fd    Target FD.
 *  @param[in] flags Flags for open().
 */
void process::_dev_null(int fd, int flags) {
  int newfd(open("/dev/null", flags));
  if (newfd < 0) {
    char const* msg(strerror(errno));
    throw (basic_error() << "could not open /dev/null: " << msg);
  }
  try {
    _dup2(newfd, fd);
  }
  catch (...) {
    ::close(newfd);
    throw ;
  }
  ::close(newfd);
  return;
}

/**
 *  dup syscall wrapper.
 *
 *  @param[in] oldfd Old FD.
 *
 *  @return The new descriptor.
 */
int process::_dup(int oldfd) {
  int newfd(dup(oldfd));
  if (newfd < 0) {
    char const* msg(strerror(errno));
    throw (basic_error() << "could not duplicate FD: " << msg);
  }
  return (newfd);
}

/**
 *  dup2 syscall wrapper.
 *
 *  @param[in] oldfd Old FD.
 *  @param[in] newfd New FD.
 */
void process::_dup2(int oldfd, int newfd) {
  if (dup2(oldfd, newfd) < 0) {
    char const* msg(strerror(errno));
    throw (basic_error() << "could not duplicate FD: " << msg);
  }
  return;
}

/**
 *  Copy internal data members.
 *
 *  @param[in] p Object to copy.
 */
void process::_internal_copy(process const& p) {
  (void)p;
  assert(!"process is not copyable");
  abort();
  return;
}

/**
 *  Get is the current process run.
 *
 *  @return True is process run, otherwise false.
 */
bool process::_is_running() const throw () {
  return (_process != static_cast<pid_t>(-1)
          || _stream[in] != -1
          || _stream[out] != -1
          || _stream[err] != -1);
}

/**
 *  kill syscall wrapper.
 *
 *  @param[in] sig The signal number.
 */
void process::_kill(int sig) {
  if (_process != static_cast<pid_t>(-1)) {
    if (::kill(_process, sig) != 0) {
      char const* msg(strerror(errno));
      throw (basic_error() << "could not terminate process "
             << _process << ": " << msg);
    }
  }
  return;
}

/**
 *  Open a pipe.
 *
 *  @param[in] fds FD array.
 */
void process::_pipe(int fds[2]) {
  if (pipe(fds) != 0) {
    char const* msg(strerror(errno));
    throw (basic_error() << "pipe creation failed: " << msg);
  }
  return;
}

/**
 *  Read data from FD.
 *
 *  @param[in]  fd   File descriptor.
 *  @param[out] data Destination buffer.
 *  @param[in]  size Maximum number of bytes to read.
 *
 *  @return Number of bytes actually read.
 */
unsigned int process::_read(int fd, void* data, unsigned int size) {
  ssize_t rb(::read(fd, data, size));
  if (rb < 0) {
    char const* msg(strerror(errno));
    throw (basic_error() << "could not read from process "
           << _process << ": " << msg);
  }
  return (rb);
}

/**
 *  Set the close-on-exec flag on the file descriptor.
 *
 *  @param[in] fd The file descriptor to set close on exec.
 */
void process::_set_cloexec(int fd) {
  int flags(fcntl(fd, F_GETFD));
  if (flags < 0) {
    char const* msg(strerror(errno));
    throw (basic_error() << "Could not get file descriptor flags: "
           << msg);
  }
  if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1) {
    char const* msg(strerror(errno));
    throw (basic_error() << "Could not set close-on-exec flag: "
           << msg);
  }
  return;
}
