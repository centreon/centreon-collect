/**
 * Copyright 2012-2013,2020-2021 Centreon
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

#include <sstream>

#include <fcntl.h>
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#ifdef HAVE_SPAWN_H
#include <spawn.h>
#endif  // HAVE_SPAWN_H
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include "com/centreon/exceptions/msg_fmt.hh"
#include "com/centreon/misc/command_line.hh"
#include "com/centreon/process_listener.hh"
#include "com/centreon/process_manager.hh"

using namespace com::centreon;
using com::centreon::exceptions::msg_fmt;

// environ is not declared on *BSD.
extern char** environ;

// Global process lock.
static std::mutex gl_process_lock;

/**
 *  Default constructor.
 */
process::process(process_listener* listener,
                 bool in_stream,
                 bool out_stream,
                 bool err_stream)
    : _enable_stream{in_stream, out_stream, err_stream},
      _listener{listener},
      _timeout{0},
      _is_timeout{false},
      _status{0},
      _stream{-1, -1, -1},
      _process{-1},
      _create_process{&_create_process_with_setpgid} {}

/**
 *  Destructor.
 */
process::~process() {
  std::unique_lock<std::mutex> lock(_lock_process);
  _kill(SIGKILL);
  _cv_process_running.wait(lock, [this] { return !_is_running(); });
}

/**
 *  Get the time when the process execution finished.
 *
 *  @return The ending timestamp.
 */
timestamp const& process::end_time() const noexcept {
  std::lock_guard<std::mutex> lock(_lock_process);
  return _end_time;
}

/**
 *  Get is the current process run.
 *
 *  @return True is process run, otherwise false.
 */
bool process::_is_running() const noexcept {
  return _process != -1 || _stream[in] != -1 || _stream[out] != -1 ||
         _stream[err] != -1;
}

/**
 *  Run process.
 *
 *  @param[in] cmd     Command line.
 *  @param[in] env     Array of strings (on form key=value), which are
 *                     passed as environment to the new process. If env
 *                     is NULL, the current environement are passed to
 *                     the new process.
 *  @param[in] timeout Maximum time in seconds to execute process. After
 *                     this time the process will be kill.
 */
void process::exec(char const* cmd, char** env, uint32_t timeout) {
  std::unique_lock<std::mutex> lock(_lock_process);

  // Check if process already running.
  if (_is_running())
    throw msg_fmt("process {} is already started and has not been waited",
                  _process);

  // Reset variable.
  _buffer_err.clear();
  _buffer_out.clear();
  _end_time.clear();
  _is_timeout = false;
  _start_time.clear();
  _status = 0;

  // Close the last file descriptor;
  for (int32_t i = 0; i < 3; ++i)
    _close(_stream[i]);

  // Init file desciptor.
  int std[3] = {-1, -1, -1};
  int pipe_stream[3][2] = {{-1, -1}, {-1, -1}, {-1, -1}};

  // volatile prevent compiler optimization that might clobber variable.
  volatile bool restore_std(false);

  std::lock_guard<std::mutex> gl_lock(gl_process_lock);
  try {
    // Create backup FDs.
    std[0] = _dup(STDIN_FILENO);
    std[1] = _dup(STDOUT_FILENO);
    std[2] = _dup(STDERR_FILENO);

    // Backup FDs do not need to be inherited.
    for (int32_t i = 0; i < 3; ++i)
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
    char* const* args = cmdline.get_argv();

    // volatile prevent compiler optimization
    // that might clobber variable.
    char** volatile my_env(env ? env : environ);

    // Create new process.
    _process = _create_process(args, my_env);
    assert(_process != -1);

    // Parent execution.
    _start_time = timestamp::now();
    _timeout = (timeout ? time(nullptr) + timeout : 0);

    // Restore original FDs.
    _dup2(std[0], STDIN_FILENO);
    _dup2(std[1], STDOUT_FILENO);
    _dup2(std[2], STDERR_FILENO);
    for (int32_t i = 0; i < 3; ++i) {
      _close(std[i]);
      //_close(pipe_stream[i][i == in ? 0 : 1]);
      _stream[i] = pipe_stream[i][i == in ? 1 : 0];
    }

    // Add process to the process manager.
    lock.unlock();
    process_manager::instance().add(this);
  } catch (...) {
    // Restore original FDs.
    if (restore_std) {
      _dup2(std[0], STDIN_FILENO);
      _dup2(std[1], STDOUT_FILENO);
      _dup2(std[2], STDERR_FILENO);
    }

    // Close all file descriptor.
    for (uint32_t i = 0; i < 3; ++i) {
      _close(std[i]);
      _close(_stream[i]);
      for (unsigned int j(0); j < 2; ++j)
        _close(pipe_stream[i][j]);
    }
    throw;
  }
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
}

/**
 *  Get the exit code, return by the executed process.
 *
 *  @return The exit code.
 */
int process::exit_code() const noexcept {
  std::lock_guard<std::mutex> lock(_lock_process);
  if (WIFEXITED(_status))
    return WEXITSTATUS(_status);
  return 0;
}

/**
 *  Get the exit status, return normal if the executed process end
 *  normaly, return crash if the executed process terminated abnormaly
 *  or return timeout.
 *
 *  @return The exit status.
 */
process::status process::exit_status() const noexcept {
  std::lock_guard<std::mutex> lock(_lock_process);
  if (_is_timeout)
    return timeout;
  if (WIFEXITED(_status))
    return normal;
  return crash;
}

/**
 *  Kill process.
 */
void process::kill(int sig) {
  std::lock_guard<std::mutex> lock(_lock_process);
  _kill(sig);
}

void process::update_ending_process(int status) {
  // Update process informations.
  std::unique_lock<std::mutex> lock(_lock_process);
  if (!_is_running())
    return;

  _end_time = timestamp::now();
  _status = status;
  _process = -1;
  _close(_stream[in]);
  if (!_is_running()) {
    // Notify listener if necessary.
    if (_listener) {
      lock.unlock();
      (_listener->finished)(*this);
    }
    // Release condition variable.
    _cv_buffer_err.notify_one();
    _cv_buffer_out.notify_one();
    _cv_process_running.notify_one();
  }
}

/**
 *  Read data from stdout.
 *
 *  @param[out] data Destination buffer.
 */
void process::read(std::string& data) {
  std::unique_lock<std::mutex> lock(_lock_process);
  // If buffer is empty and stream is open, we waiting data.
  _cv_buffer_out.wait(
      lock, [this] { return !_buffer_out.empty() || _stream[out] == -1; });
  // Switch content.
  data.clear();
  data.swap(_buffer_out);
}

/**
 *  Read data from stderr.
 *
 *  @param[out] data Destination buffer.
 */
void process::read_err(std::string& data) {
  std::unique_lock<std::mutex> lock(_lock_process);
  // If buffer is empty and stream is open, we waiting data.
  _cv_buffer_err.wait(
      lock, [this] { return !_buffer_err.empty() || _stream[err] == -1; });
  // Switch content.
  data.clear();
  data.swap(_buffer_err);
}

/**
 *  Used setpgid when exec is call.
 *
 *  @param[in] enable  True to  use setpgid, otherwise false.
 */
void process::setpgid_on_exec(bool enable) noexcept {
  std::lock_guard<std::mutex> lock(_lock_process);
  if (enable)
    _create_process = &_create_process_with_setpgid;
  else
    _create_process = &_create_process_without_setpgid;
}

/**
 *  Get if used setpgid is enable.
 *
 *  @return True if setpgid is enable, otherwise false.
 */
bool process::setpgid_on_exec() const noexcept {
  std::lock_guard<std::mutex> lock(_lock_process);
  return _create_process == &_create_process_with_setpgid;
}

/**
 *  Get the time when the process execution start.
 *
 *  @return The starting timestamp.
 */
timestamp const& process::start_time() const noexcept {
  std::lock_guard<std::mutex> lock(_lock_process);
  return _start_time;
}

/**
 *  Terminate process. We don't wait for the termination, the SIGTERM signal
 *  is just sent.
 */
void process::terminate() {
  std::lock_guard<std::mutex> lock(_lock_process);
  _kill(SIGTERM);
}

/**
 *  Wait for process termination.
 */
void process::wait() const {
  {
    std::unique_lock<std::mutex> lock(_lock_process);
    _cv_process_running.wait(lock, [this] { return !_is_running(); });
  }
  process_manager::instance().wait_for_update();
}

/**
 *  Wait for process termination.
 *
 * @param timeout Maximum number of milliseconds to wait for process
 * termination.
 *
 * @return true if process exited.
 */
bool process::wait(uint32_t timeout) const {
  std::unique_lock<std::mutex> lock(_lock_process);
  return _cv_process_running.wait_for(lock, std::chrono::milliseconds(timeout),
                                      [this] { return !_is_running(); });
}

/**
 *  Write data to process' standard input.
 *
 *  @param[in] data Source buffer.
 *
 *  @return Number of bytes actually written.
 */
unsigned int process::write(std::string const& data) {
  return write(data.c_str(), data.size());
}

/**
 * @brief This function is only used by process object. Its goal is to show
 * the content of buffers sent or received by process through pipes. The
 * buffer is given by a const char array but it contains non ascii characters.
 * So for all characters not displayable, we show the hexadecimal code instead.
 * And this function transforms a such binary buffer to a string that can be
 * shown to understand an error.
 *
 * @param data A char array representing a binary buffer.
 * @param size The size of the buffer.
 *
 * @return A string containing the data buffer but displayable in a string with
 * bad characters converted into hexadecimal numbers.
 */
static std::string to_string(const char* data, size_t size) {
  std::ostringstream oss;
  for (size_t i = 0; i < size; i++) {
    if (!isprint(*data)) {
      unsigned int c = *data;
      unsigned char c1, c2;
      c1 = c >> 4;
      c2 = c & 0xf;
      if (c1 <= 9)
        c1 += '0';
      else if (c1 > 9)
        c1 += 'A' - 10;
      if (c2 <= 9)
        c2 += '0';
      else if (c2 > 9)
        c2 += 'A' - 10;
      oss << "\\x" << c1 << c2;
    } else
      oss << *data;
    data++;
  }
  return oss.str();
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
  int fd;
  pid_t my_process;
  {
    std::lock_guard<std::mutex> lock(_lock_process);
    fd = _stream[in];
    my_process = _process;
  }

  ssize_t wb = ::write(fd, data, size);
  if (wb < 0) {
    char const* msg(strerror(errno));
    if (errno == EINTR)
      throw msg_fmt("{}", msg);
    throw msg_fmt("could not write '{}' on process {}'s input: {}",
                  to_string(static_cast<const char*>(data), size), my_process,
                  msg);
  }
  return wb;
}

void process::do_close(int fd) {
  std::unique_lock<std::mutex> lock(_lock_process);
  if (_stream[out] == fd) {
    _close(_stream[out]);
    _cv_buffer_out.notify_one();
  } else if (_stream[err] == fd) {
    _close(_stream[err]);
    _cv_buffer_err.notify_one();
  }
  if (!_is_running()) {
    // Notify listener if necessary.
    _cv_process_running.notify_one();

    if (_listener) {
      lock.unlock();
      (_listener->finished)(*this);
    }
  }
}

/**
 *  close syscall wrapper.
 *
 *  @param[in, out] fd The file descriptor to close.
 */
void process::_close(int& fd) noexcept {
  if (fd >= 0) {
    while (::close(fd) < 0 && errno == EINTR)
      ;
  }
  fd = -1;
}

pid_t process::_create_process_with_setpgid(char* const* args, char** env) {
  pid_t pid(static_cast<pid_t>(-1));
#ifdef HAVE_SPAWN_H
  posix_spawnattr_t attr;
  int ret = posix_spawnattr_init(&attr);
  if (ret)
    throw msg_fmt("cannot initialize spawn attributes: {}", strerror(ret));
  ret = posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP);
  if (ret) {
    posix_spawnattr_destroy(&attr);
    throw msg_fmt("cannot set spawn flag: {}", strerror(ret));
  }
  ret = posix_spawnattr_setpgroup(&attr, 0);
  if (ret) {
    posix_spawnattr_destroy(&attr);
    throw msg_fmt("cannot set process group ID of to-be-spawned process: {}",
                  strerror(ret));
  }
  if (posix_spawnp(&pid, args[0], NULL, &attr, args, env)) {
    char const* msg(strerror(errno));
    posix_spawnattr_destroy(&attr);
    throw msg_fmt("could not create process '{}': {}", args[0], msg);
  }
  posix_spawnattr_destroy(&attr);
#else
  pid = fork();
  if (pid == static_cast<pid_t>(-1)) {
    char const* msg(strerror(errno));
    throw msg_fmt("could not create process '{}': {}", args[0], msg);
  }

  // Child execution.
  if (!pid) {
    // Set process to its own group.
    ::setpgid(0, 0);

    ::execve(args[0], args, env);
    ::_exit(EXIT_FAILURE);
  }
#endif  // HAVE_SPAWN_H
  return pid;
}

pid_t process::_create_process_without_setpgid(char* const* args, char** env) {
  pid_t pid(static_cast<pid_t>(-1));
#ifdef HAVE_SPAWN_H
  if (posix_spawnp(&pid, args[0], NULL, NULL, args, env)) {
    char const* msg(strerror(errno));
    throw msg_fmt("could not create process '{}': {}", args[0], msg);
  }
#else
  pid = vfork();
  if (pid == static_cast<pid_t>(-1)) {
    char const* msg(strerror(errno));
    throw msg_fmt("could not create process '{}': {}", args[0], msg);
  }

  // Child execution.
  if (!pid) {
    ::execve(args[0], args, env);
    ::_exit(EXIT_FAILURE);
  }
#endif  // HAVE_SPAWN_H
  return pid;
}

/**
 *  Open /dev/null and duplicate file descriptor.
 *
 *  @param[in] fd    Target FD.
 *  @param[in] flags Flags for open().
 */
void process::_dev_null(int fd, int flags) {
  int newfd(::open("/dev/null", flags));
  if (newfd < 0) {
    char const* msg(strerror(errno));
    throw msg_fmt("could not open /dev/null: {}", msg);
  }
  try {
    _dup2(newfd, fd);
  } catch (...) {
    _close(newfd);
    throw;
  }
  _close(newfd);
}

/**
 *  dup syscall wrapper.
 *
 *  @param[in] oldfd Old FD.
 *
 *  @return The new descriptor.
 */
int process::_dup(int oldfd) {
  int newfd;
  while ((newfd = dup(oldfd)) < 0) {
    if (errno == EINTR)
      continue;
    char const* msg(strerror(errno));
    throw msg_fmt("could not duplicate FD: {}", msg);
  }
  return newfd;
}

/**
 *  dup2 syscall wrapper.
 *
 *  @param[in] oldfd Old FD.
 *  @param[in] newfd New FD.
 */
void process::_dup2(int oldfd, int newfd) {
  while (dup2(oldfd, newfd) < 0) {
    if (errno == EINTR)
      continue;
    char const* msg(strerror(errno));
    throw msg_fmt("could not duplicate FD: {}", msg);
  }
}

/**
 *  kill syscall wrapper.
 *
 *  @param[in] sig The signal number.
 */
void process::_kill(int sig) {
  if (_process && _process != static_cast<pid_t>(-1)) {
    if (::kill(_process, sig) != 0) {
      char const* msg(strerror(errno));
      throw msg_fmt("could not terminate process {}: {}", _process, msg);
    }
  }
}

/**
 *  Open a pipe.
 *
 *  @param[in] fds FD array.
 */
void process::_pipe(int fds[2]) {
  if (pipe(fds) != 0) {
    char const* msg(strerror(errno));
    throw msg_fmt("pipe creation failed: {}", msg);
  }
}

ssize_t process::do_read(int fd) {
  // Read content of the stream and push it.
  char buffer[4096];
  ssize_t size = ::read(fd, buffer, sizeof(buffer));

  if (size == -1) {
    char const* msg(strerror(errno));
    if (errno == EINTR)
      throw msg_fmt("{}", msg);
    throw msg_fmt("could not read from process {}: {}", _process, msg);
  }
  if (size == 0)
    return 0;

  {
    std::unique_lock<std::mutex> lock(_lock_process);

    if (_stream[out] == fd) {
      _buffer_out.append(buffer, size);
      _cv_buffer_out.notify_one();
      // Notify listener if necessary.
      if (_listener) {
        lock.unlock();
        (_listener->data_is_available)(*this);
      }
    } else if (_stream[err] == fd) {
      _buffer_err.append(buffer, size);
      _cv_buffer_err.notify_one();
      // Notify listener if necessary.
      if (_listener) {
        lock.unlock();
        (_listener->data_is_available_err)(*this);
      }
    }
  }

  return size;
}

/**
 *  Set the close-on-exec flag on the file descriptor.
 *
 *  @param[in] fd The file descriptor to set close on exec.
 */
void process::_set_cloexec(int fd) {
  int flags(0);
  while ((flags = fcntl(fd, F_GETFD)) < 0) {
    if (errno == EINTR)
      continue;
    char const* msg(strerror(errno));
    throw msg_fmt("Could not get file descriptor flags: {}", msg);
  }
  while (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) < 0) {
    if (errno == EINTR)
      continue;
    throw msg_fmt("Could not set close-on-exec flag: {}", strerror(errno));
  }
}

void process::set_timeout(bool timeout) {
  _is_timeout = timeout;
}
