/**
 * Copyright 2026 Centreon
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

#include <boost/process/v2/process.hpp>
#include <optional>

#include "com/centreon/common/process/detail/boost_process.hh"
#include "com/centreon/common/process/fork.hh"

using namespace com::centreon::common;

namespace com::centreon::common::detail {

/**
 * @brief RAII wrapper around a POSIX anonymous pipe (pipe(2)).
 *
 * Owns a pair of file descriptors {read-end, write-end} and closes them
 * automatically on destruction.  Individual ends can be "stolen" (ownership
 * transferred to the caller) so that they survive the pipe object's lifetime —
 * this is the pattern used in do_fork() to hand the pipe ends to Boost.Asio
 * after the fork.
 *
 */
class pipe {
  /** _fd[0]: read end; _fd[1]: write end.  0 means closed/not owned. */
  int _fd[2] = {
      0,
      0,
  };

 public:
  /** @brief Create the pipe; throws msg_fmt if pipe(2) fails. */
  pipe() {
    if (::pipe(_fd)) {
      throw exceptions::msg_fmt("unable to create pipe:{}", strerror(errno));
    }
  }

  /** @brief Close both ends that are still owned. */
  ~pipe() { close(); }

  /** @return The read-end file descriptor (does not transfer ownership). */
  int get_fd_to_read() const { return _fd[0]; }
  /** @return The write-end file descriptor (does not transfer ownership). */
  int get_fd_to_write() const { return _fd[1]; }

  /**
   * @brief Transfer ownership of the read end to the caller.
   *
   * After this call the pipe object no longer closes _fd[0] on destruction.
   * @return The read-end file descriptor.
   */
  int steal_read_fd() {
    int fd = _fd[0];
    _fd[0] = 0;
    return fd;
  }

  /**
   * @brief Transfer ownership of the write end to the caller.
   *
   * After this call the pipe object no longer closes _fd[1] on destruction.
   * @return The write-end file descriptor.
   */
  int steal_write_fd() {
    int fd = _fd[1];
    _fd[1] = 0;
    return fd;
  }

  /** @brief Close both ends that are still owned. */
  void close() {
    if (_fd[0]) {
      ::close(_fd[0]);
      _fd[0] = 0;
    }
    if (_fd[1]) {
      ::close(_fd[1]);
      _fd[1] = 0;
    }
  }

  /** @brief Close the write end only. */
  void close_write() {
    if (_fd[1]) {
      ::close(_fd[1]);
      _fd[1] = 0;
    }
  }

  /** @brief Close the read end only. */
  void close_read() {
    if (_fd[0]) {
      ::close(_fd[0]);
      _fd[0] = 0;
    }
  }
};

}  // namespace com::centreon::common::detail

/**
 * @brief Fork the current process and start the child.
 *
 * Creates two or three anonymous pipes (stdin, stdout, and optionally stderr),
 * calls fork(2), then:
 * - Parent side: takes ownership of the write end of stdin and the read ends
 *   of stdout (and stderr if @p use_stderr_pipe is true), wraps the child PID
 *   in a boost::process handle, and starts async-wait for process termination.
 *   When @p use_stderr_pipe is false no stderr pipe is created, and the child's
 *   stderr is inherited from the parent process.
 * - Child side: passes the raw pipe file descriptors to _run(); when
 *   @p use_stderr_pipe is false, -1 is passed as the @c stderr_fd argument.
 *   After _run() returns the child calls ::exit() with the return value.
 *
 * @param use_stderr_pipe When true a dedicated stderr pipe is created between
 *        parent and child, and _on_stderr_read() will be called with the
 *        child's stderr output.  When false no stderr pipe is allocated,
 *        stderr_fd is -1 in _run(), and the child inherits the parent's stderr.
 *
 * @throws com::centreon::exceptions::msg_fmt if fork(2) fails.
 */
template <bool use_mutex>
void com::centreon::common::fork<use_mutex>::do_fork(bool use_stderr_pipe) {
  detail::pipe stdin;
  detail::pipe stdout;
  std::unique_ptr<detail::pipe> stderr;
  if (use_stderr_pipe)
    stderr = std::make_unique<detail::pipe>();

  pid_t child = ::fork();
  if (child < 0) {
    throw exceptions::msg_fmt("unable to fork:{}", strerror(errno));
  }

  if (child > 0) {  // parent
    this->_stdin_pipe.assign(stdin.steal_write_fd());
    stdin.close_read();
    this->_stdout_pipe.assign(stdout.steal_read_fd());
    stdout.close_write();
    if (use_stderr_pipe) {
      this->_stderr_pipe.assign(stderr->steal_read_fd());
      stderr->close_write();
    }
    _proc = new detail::boost_process(
        boost::process::v2::basic_process<asio::io_context::executor_type>(
            this->_io_context->get_executor(), child));
    this->_async_wait_process_end();
    this->_stdout_read();
    if (use_stderr_pipe) {
      this->_stderr_read();
    }
    SPDLOG_LOGGER_DEBUG(_logger, "child started pid:{} ", child);

  } else {  // child
    stdin.close_write();
    stdout.close_read();
    if (use_stderr_pipe) {
      stderr->close_read();
    }
    int exit_code = _run(stdin.get_fd_to_read(), stdout.get_fd_to_write(),
                         use_stderr_pipe ? stderr->get_fd_to_write() : -1);
    ::exit(exit_code);
  }
}

template class com::centreon::common::fork<true>;
template class com::centreon::common::fork<false>;
