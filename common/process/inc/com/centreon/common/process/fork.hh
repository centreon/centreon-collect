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

#ifndef CENTREON_COMMON_FORK_HH
#define CENTREON_COMMON_FORK_HH

#include "boost/asio/readable_pipe.hpp"
#include "boost/asio/writable_pipe.hpp"
#include "child_process.hh"

namespace com::centreon::common {

/**
 * @brief Base class for child processes created via fork(2).
 *
 * This CRTP-friendly template inherits from child_process and provides the
 * mechanics for spawning a child via the POSIX fork(2) syscall.  The parent
 * side receives three Boost.Asio pipes (stdin/stdout/stderr) and a
 * boost::process handle so that all I/O and lifetime management go through the
 * async machinery of child_process.  The child side calls the pure-virtual
 * _run() method and then exits with its return value.
 *
 * Typical usage: derive from fork<>, implement _run(), then call do_fork() to
 * actually create the child process.
 *
 * @tparam use_mutex When true (default) an absl::Mutex protects shared state,
 *         allowing concurrent calls from multiple threads.  Pass false in
 *         single-threaded contexts to eliminate locking overhead.
 */
template <bool use_mutex = true>
class fork : public child_process<use_mutex> {
  using child_process<use_mutex>::_logger;
  using child_process<use_mutex>::_proc;

 protected:
  /**
   * @brief Entry point executed in the child process after fork(2).
   *
   * Implement this method in a derived class to define what the child process
   * actually does.  The method runs in a freshly forked process; the parent
   * is already unblocked at that point.  The return value is passed to
   * ::exit(), so it becomes the process exit code visible to waitpid(2).
   *
   * @return Exit code for the child process (0 = success, non-zero = error).
   */
  virtual int _run() = 0;

 public:
  /**
   * @brief Construct a fork instance without starting the child process.
   *
   * The child is not created until do_fork() is called.
   *
   * @param io_context Boost.Asio context that will drive all async I/O for
   *        the pipes and the process-end notification.
   * @param logger spdlog logger shared with the caller, used for debug traces.
   */
  fork(const std::shared_ptr<asio::io_context> io_context,
       const std::shared_ptr<spdlog::logger>& logger)
      : child_process<use_mutex>(io_context, logger) {}

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
  void do_fork();
};

}  // namespace com::centreon::common

#endif
