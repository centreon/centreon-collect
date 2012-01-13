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
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/process_posix.hh"

using namespace com::centreon;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Default constructor.
 */
process::process()
  : _err(-1),
    _in(-1),
    _out(-1),
    _process((pid_t)-1),
    _with_err(false),
    _with_in(false),
    _with_out(false) {}

/**
 *  Destructor.
 */
process::~process() throw () {
  _terminated();
}

/**
 *  Run process.
 *
 *  @param[in] cmd Command line.
 */
void process::exec(std::string const& cmd) {
  // Check viability.
  if (_process != (pid_t)-1)
    throw (basic_error() << "process " << _process
           << " is already started and has not been waited");

  // Create pipes if necessary.
  int err[2];
  int in[2];
  int out[2];
  err[0] = -1;
  err[1] = -1;
  in[0] = -1;
  in[1] = -1;
  out[0] = -1;
  out[1] = -1;
  try {
    if (_with_err)
      _pipe(err);
    if (_with_in)
      _pipe(in);
    if (_with_out)
      _pipe(out);
  }
  catch (...) {
    if (err[0] >= 0) {
      ::close(err[0]);
      ::close(err[1]);
    }
    if (in[0] >= 0) {
      ::close(in[0]);
      ::close(in[1]);
    }
    throw ;
  }

  // Fork.
  _process = fork();
  if (_process == (pid_t)-1) {
    char const* msg(strerror(errno));
    throw (basic_error() << "could not fork: " << msg);
  }

  // Child.
  if (!_process) {
    try {
      // Connect standard error.
      if (err[0] >= 0) {
        ::close(err[0]);
        _dup2(err[1], STDERR_FILENO);
        ::close(err[1]);
      }
      else
        _dev_null(STDERR_FILENO, O_WRONLY);

      // Connect standard input.
      if (in[0] >= 0) {
        ::close(in[1]);
        _dup2(in[0], STDIN_FILENO);
        ::close(in[0]);
      }
      else
        _dev_null(STDIN_FILENO, O_RDONLY);

      // Connect standard output.
      if (out[0] >= 0) {
        ::close(out[0]);
        _dup2(out[1], STDOUT_FILENO);
        ::close(out[1]);
      }
      else
        _dev_null(STDOUT_FILENO, O_WRONLY);

      // XXX: parsing sucks, I know.
      // Count spaces in command line.
      std::string cmdline(cmd);
      unsigned int size(std::count(
                               cmdline.begin(),
                               cmdline.end(),
                               ' '));

      // Allocate argument array.
      size += 2;
      char** array;
      array = new char*[size];

      // Replace spaces with \0.
      size_t pos(cmdline.find_first_of(' '));
      while (pos != std::string::npos) {
        cmdline[pos] = '\0';
        pos = cmdline.find_first_of(' ', pos + 1);
      }

      // Fill array.
      array[0] = const_cast<char*>(cmdline.c_str());
      for (unsigned int i = 1; i < size - 1; ++i)
        array[i] = array[i - 1] + strlen(array[i - 1]) + 1;
      array[size - 1] = NULL;

      // Execute.
      execvp(array[0], array);
    }
    catch (...) {}
    exit(EXIT_FAILURE);
  }

  // Parent.
  ::close(err[1]);
  ::close(in[0]);
  ::close(out[1]);
  _err = err[0];
  _in = in[1];
  _out = out[0];

  return ;
}

/**
 *  Read data from stdout.
 *
 *  @param[in] data Destination buffer.
 *  @param[in] size Maximum number of bytes to read.
 *
 *  @return Number of bytes actually read.
 */
unsigned int process::read(void* data, unsigned int size) {
  return (_read(_out, data, size));
}

/**
 *  Read data from stderr.
 *
 *  @param[in] data Destination buffer.
 *  @param[in] size Maximum number of bytes to read.
 *
 *  @return Number of bytes actually read.
 */
unsigned int process::read_err(void* data, unsigned int size) {
  return (_read(_err, data, size));
}

/**
 *  Terminate the process.
 */
void process::terminate() {
  if (_process != (pid_t)-1) {
    if (kill(_process, SIGKILL) != 0) {
      char const* msg(strerror(errno));
      throw (basic_error() << "could not terminate process "
             << _process << ": " << msg);
    }
  }
  return ;
}

/**
 *  Wait for process termination.
 *
 *  @return Process exit code.
 */
int process::wait() {
  if (_process == (pid_t)-1)
    throw (basic_error() << "attempt to wait an unstarted process");
  int status(0);
  pid_t ret(waitpid(_process, &status, 0));
  if (ret == (pid_t)-1) {
    char const* msg(strerror(errno));
    throw (basic_error() << "error while waiting for process: " << msg);
  }
  _terminated();
  if (WIFEXITED(status))
    status = WEXITSTATUS(status);
  return (status);
}

/**
 *  Wait for process termination.
 *
 *  @param[in]  timeout   Maximum number of milliseconds to wait for
 *                        process termination.
 *  @param[out] exit_code Will be set to the process' exit code.
 *
 *  @return true if process exited.
 */
bool process::wait(unsigned long timeout, int* exit_code) {
  if (_process == (pid_t)-1)
    throw (basic_error() << "attempt to wait an unstarted process");

  // Get the current time.
  timeval now;
  gettimeofday(&now, NULL);
  timeval limit;
  memcpy(&limit, &now, sizeof(limit));

  // Add timeout.
  limit.tv_sec += timeout / 1000;
  timeout %= 1000;
  limit.tv_usec += timeout * 1000;
  if (limit.tv_usec > 1000000) {
    limit.tv_usec -= 1000000;
    ++limit.tv_sec;
  }

  // Wait for the end of process or timeout.
  int status(0);
  bool running(true);
  while (running
         && ((now.tv_sec * 1000000ull + now.tv_usec)
             < (limit.tv_sec * 1000000ull + limit.tv_usec))) {
    usleep(10000);
    pid_t ret(waitpid(_process, &status, WNOHANG));
    if (ret == (pid_t)-1) {
      char const* msg(strerror(errno));
      throw (basic_error() << "could not wait process "
             << _process << ": " << msg);
    }
    running = (ret == 0);
    gettimeofday(&now, NULL);
  }
  if (!running) {
    _terminated();
    if (WIFEXITED(status))
      status = WEXITSTATUS(status);
    if (exit_code)
      *exit_code = status;
  }
  return (!running);
}

/**
 *  Enable or disable process' stderr.
 *
 *  @param[in] enable Set to true to enable stderr.
 */
void process::with_stderr(bool enable) {
  _with_std(enable, &_with_err, &_err, "stderr");
  return ;
}

/**
 *  Enable or disable process' stdin.
 *
 *  @param[in] enable Set to true to enable stdin.
 */
void process::with_stdin(bool enable) {
  _with_std(enable, &_with_in, &_in, "stdin");
  return ;
}

/**
 *  Enable or disable process' stdout.
 *
 *  @param[in] enable Set to true to enable stdout.
 */
void process::with_stdout(bool enable) {
  _with_std(enable, &_with_out, &_out, "stdout");
  return ;
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
  ssize_t wb(::write(_in, data, size));
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
  return ;
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
  return ;
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
  return ;
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
  return ;
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
 *  Reset fields of process after execution.
 */
void process::_terminated() {
  _process = (pid_t)-1;
  if (_err >= 0) {
    ::close(_err);
    _err = -1;
  }
  if (_in >= 0) {
    ::close(_in);
    _in = -1;
  }
  if (_out >= 0) {
    ::close(_out);
    _out = -1;
  }
  return ;
}

/**
 *  Enable or disable standard process objects.
 *
 *  @param[in]     enable New boolean flag.
 *  @param[in,out] b      Boolean flag.
 *  @param[in,out] fd     FD.
 *  @param[in]     name   Object name (for error reporting).
 */
void process::_with_std(
                bool enable,
                bool* b,
                int* fd,
                char const* name) {
  if (*b != enable) {
    if (_process == (pid_t)-1)
      *b = enable;
    else if (!enable) {
      if (*fd >= 0) {
        ::close(*fd);
        *fd = -1;
      }
    }
    else
      throw (basic_error() << "cannot reenable "
             << name << " while process is running");
  }
  return ;
}
