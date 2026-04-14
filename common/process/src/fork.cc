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
 * Storing 0 for a closed or stolen fd relies on the convention that fd 0
 * (STDIN_FILENO) is never owned by this class; it is always redirected via
 * dup2 before any pipe object refers to it.
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

  /**
   * @brief Redirect the write end to a standard file descriptor via dup2(2).
   *
   * Used in the child process to wire the pipe's write end onto STDOUT_FILENO
   * or STDERR_FILENO.
   *
   * @param std_fd Target file descriptor (e.g. STDOUT_FILENO).
   */
  void write_fd_dup(int std_fd) {
    if (::dup2(_fd[1], std_fd)) {
      std::cerr << "dup2 on fd: " << std_fd << ": " << strerror(errno)
                << std::endl;
    }
  }

  /**
   * @brief Redirect the read end to a standard file descriptor via dup2(2).
   *
   * Used in the child process to wire the pipe's read end onto STDIN_FILENO.
   *
   * @param std_fd Target file descriptor (e.g. STDIN_FILENO).
   */
  void read_fd_dup(int std_fd) {
    if (::dup2(_fd[0], std_fd)) {
      std::cerr << "dup2 on fd: " << std_fd << ": " << strerror(errno)
                << std::endl;
    }
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
 * Creates three anonymous pipes (stdin, stdout, stderr), calls fork(2), then:
 * - Parent side: takes ownership of the write end of stdin and the read ends
 *   of stdout/stderr, wraps the child PID in a boost::process handle, and
 *   starts async-wait for process termination.
 * - Child side: redirects STDIN/STDOUT/STDERR to the pipe ends via dup2(2),
 *   closes unused file descriptors, then calls _run() and exits.
 *
 * @throws com::centreon::exceptions::msg_fmt if fork(2) fails.
 */
template <bool use_mutex>
void com::centreon::common::fork<use_mutex>::do_fork() {
  detail::pipe stdin;
  detail::pipe stdout;
  detail::pipe stderr;

  pid_t child = ::fork();
  if (child < 0) {
    throw exceptions::msg_fmt("unable to fork:{}", strerror(errno));
  }

  if (child > 0) {  // parent
    this->_stdin_pipe.assign(stdin.steal_write_fd());
    this->_stdout_pipe.assign(stdout.steal_read_fd());
    this->_stderr_pipe.assign(stderr.steal_read_fd());
    _proc = new detail::boost_process(
        boost::process::v2::basic_process<asio::io_context::executor_type>(
            this->_io_context->get_executor(), child));
    this->_async_wait_process_end();
    SPDLOG_LOGGER_DEBUG(_logger, "child started pid:{} ", child);

  } else {  // child
    stdin.read_fd_dup(STDIN_FILENO);
    stdout.write_fd_dup(STDOUT_FILENO);
    stderr.write_fd_dup(STDERR_FILENO);
    stdin.close_write();
    stdout.close_read();
    stderr.close_read();
    ::exit(_run());
  }
}

template class com::centreon::common::fork<true>;
template class com::centreon::common::fork<false>;
