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
 * @brief Base class for child processes created via POSIX fork(2).
 *
 * Inherits from child_process and adds the fork(2) mechanics: pipe creation,
 * process spawning, and Boost.Asio integration for async I/O and process-end
 * notification.
 *
 * ## Lifecycle
 * 1. Construct the derived object (no process is created yet).
 * 2. Call do_fork() to actually fork.
 *    - **Parent** side: the three Boost.Asio pipe objects (_stdin_pipe,
 *      _stdout_pipe, _stderr_pipe) become usable and async reads start
 *      immediately.  _on_stdout_read() / _on_stderr_read() / _on_process_end()
 *      are called on the io_context thread as data arrives or the process
exits.
 *    - **Child** side: _run() is invoked with the raw pipe file descriptors;
 *      when it returns the child calls ::exit() with the return value.
 * 3. The parent can write to the child via write_to_child_stdin() and kill it
 *    via kill().
 *
 * ## Thread safety
 * @tparam use_mutex    When true (default) an absl::Mutex guards all shared
 *                      state, making the object safe to use from multiple
 *                      threads.  Pass false in single-threaded contexts to
 *                      eliminate locking overhead.
 *
 * ## Asio fork notification
 * @tparam asio_notify_fork  When true, do_fork() calls
 *                           io_context::notify_fork() with the prepare /
 *                           parent / child phases around the fork(2) syscall.
 *                           This is required when the io_context has active
 *                           async operations in the parent (e.g. timers,
 *                           sockets) so that internal file descriptors are
 *                           correctly reset in the child.
 * Use it in single thread io_context run. In multithread program, there is an
issue in io_context::notify_fork. Internally, ctx.notify_fork calls
epoll_reactor::notify_fork which locks registered_descriptors_mutex_. An issue
occurs when registered_descriptors_mutex_ is locked by another thread at fork
timepoint. In such a case, child process starts with
registered_descriptors_mutex_ already locked and both child and parent process
will hang.
 */
template <bool use_mutex = true, bool asio_notify_fork = false>
class fork : public child_process<use_mutex> {
 private:
  using child_process<use_mutex>::_stdout_pipe;
  using child_process<use_mutex>::_stderr_pipe;
  using child_process<use_mutex>::_use_stderr_pipe;
  using child_process<use_mutex>::_proc;

  using child_process<use_mutex>::_stdout_read;
  using child_process<use_mutex>::_stderr_read;
  using child_process<use_mutex>::_async_wait_process_end;
  using child_process<use_mutex>::_stdin_pipe;

 protected:
  using child_process<use_mutex>::_io_context;
  using child_process<use_mutex>::_logger;

  /**
   * @brief Entry point executed in the child process after fork(2).
   *
   * Implement this pure-virtual method in a derived class to define the
   * child's behaviour.  It runs in a freshly forked process; the parent is
   * already unblocked at that point.
   *
   * The three file descriptors are the raw pipe ends connecting the child to
   * the parent:
   *  - @p stdin_fd   read end of the parent→child pipe; data written by the
   *                  parent via write_to_child_stdin() arrives here.
   *  - @p stdout_fd  write end of the child→parent stdout pipe; bytes written
   *                  here trigger _on_stdout_read() in the parent.
   *  - @p stderr_fd  write end of the child→parent stderr pipe; bytes written
   *                  here trigger _on_stderr_read() in the parent.  Equals -1
   *                  when do_fork() was called with use_stderr_pipe = false.
   *
   * The return value is forwarded to ::exit(), becoming the process exit code
   * visible to waitpid(2).
   *
   * @param stdin_fd   Read end of the stdin pipe.
   * @param stdout_fd  Write end of the stdout pipe.
   * @param stderr_fd  Write end of the stderr pipe, or -1 if not created.
   * @return Exit code for the child process (0 = success, non-zero = error).
   */
  virtual int _run(int stdin_fd, int stdout_fd, int stderr_fd) = 0;

 public:
  /**
   * @brief Construct a fork object without starting the child process.
   *
   * No fork(2) happens here.  Call do_fork() when ready to spawn the child.
   *
   * @param io_context  Boost.Asio context that drives all async I/O for the
   *                    pipes and the process-end notification in the parent.
   * @param logger      spdlog logger instance shared with the caller.
   */
  fork(const std::shared_ptr<asio::io_context> io_context,
       const std::shared_ptr<spdlog::logger>& logger)
      : child_process<use_mutex>(io_context, logger) {}

  fork(const fork&) = delete;
  fork& operator=(const fork&) = delete;

  std::shared_ptr<fork> shared_from_this() {
    return std::static_pointer_cast<fork>(
        child_process<use_mutex>::shared_from_this());
  }

  /**
   * @brief Spawn the child process.
   *
   * Creates the anonymous pipes, then calls fork(2):
   *
   * **Parent** — assigns the write end of stdin and the read ends of stdout
   * (and stderr when @p use_stderr_pipe is true) to the corresponding Asio
   * pipe objects, wraps the child PID in a Boost.Process handle, and starts
   * async reads and a process-end wait on the io_context.
   *
   * **Child** — calls _run() with the raw pipe file descriptors, then
   * ::exit() with the return value.
   *
   * If @p asio_notify_fork is true the io_context receives fork_prepare /
   * fork_parent / fork_child notifications around the fork(2) call so that
   * internal Asio file descriptors are correctly handled on both sides.
   *
   * @param use_stderr_pipe  When true a dedicated stderr pipe is created;
   *                         _on_stderr_read() will be called with the child's
   *                         stderr output.  When false no stderr pipe is
   *                         allocated, @p stderr_fd is -1 in _run(), and the
   *                         child inherits the parent's stderr file descriptor.
   *
   * @throws com::centreon::exceptions::msg_fmt if pipe(2) or fork(2) fails.
   */
  void do_fork(bool use_stderr_pipe);
};

}  // namespace com::centreon::common

#endif
