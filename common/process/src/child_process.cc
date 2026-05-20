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

#include <sys/syscall.h>
#include <boost/icl/interval_set.hpp>
#include <boost/process/v2/process.hpp>
#include "absl/strings/numbers.h"
#include "boost/icl/concept/interval_bounds.hpp"
#include "boost/icl/discrete_interval.hpp"

#include "com/centreon/common/file_system.hh"
#include "com/centreon/common/process/child_process.hh"
#include "com/centreon/common/process/detail/boost_process.hh"

using namespace com::centreon::common;

/**
 * @brief Close all file descriptors except those in @p fds_to_not_close.
 *
 * Computes the complement of @p fds_to_not_close over [0, INT_MAX] using
 * boost::icl, then closes each resulting interval with a single close_range(2)
 * syscall.  @p flags is forwarded verbatim (e.g. CLOSE_RANGE_CLOEXEC to set
 * FD_CLOEXEC instead of actually closing).
 *
 * Requires Linux 5.9+ (SYS_close_range) and glibc 2.34+ (CLOSE_RANGE_CLOEXEC).
 * Falls back to the /proc/self/fd implementation on older systems.
 *
 * @param fds_to_not_close  File descriptors to leave open.
 * @param flags             Flags for close_range(2) (0 or CLOSE_RANGE_CLOEXEC).
 */
#if defined SYS_close_range && defined CLOSE_RANGE_CLOEXEC
void com::centreon::common::close_range(const std::set<int>& fds_to_not_close,
                                        int flags) {
  boost::icl::interval_set<int> intervals;
  intervals.add(boost::icl::discrete_interval<int>::closed(
      0, std::numeric_limits<int>::max()));
  for (int fd : fds_to_not_close) {
    intervals -= fd;
  }

  for (const auto& inter : intervals) {
    int lower = inter.lower();
    if (!boost::icl::is_left_closed(inter.bounds())) {
      ++lower;
    }
    int upper = inter.upper();
    if (!boost::icl::is_right_closed(inter.bounds())) {
      --upper;
    }
    if (lower <= upper) {
      ::syscall(SYS_close_range, lower, upper, flags);
    }
  }
}
#else
/**
 * @brief Fallback: close all fds not in @p fds_to_not_close via /proc/self/fd.
 *
 * Used when SYS_close_range or CLOSE_RANGE_CLOEXEC are unavailable.
 * @p flags is ignored in this implementation.
 */
void com::centreon::common::close_range(const std::set<int>& fds_to_not_close,
                                        int flags) {
  std::list<std::filesystem::path> opened =
      dir_content("/proc/self/fd", false, true);

  for (const std::filesystem::path& fd_path : opened) {
    int numeric_fd;
    if (absl::SimpleAtoi(fd_path.filename().native(), &numeric_fd) &&
        fds_to_not_close.find(numeric_fd) == fds_to_not_close.end()) {
      ::close(numeric_fd);
    }
  }
}

#endif

/**
 * @brief Destroy the process<use mutex>::process object
 *
 * @tparam use_mutex
 */
template <bool use_mutex>
child_process<use_mutex>::~child_process() {
  SPDLOG_LOGGER_TRACE(_logger, "delete process {:p}",
                      static_cast<const void*>(this));
  if (_proc) {
    delete _proc;
  }
}

/**
 * @brief returns pid of process, -1 otherwise
 *
 * @tparam use_mutex
 * @return int
 */
template <bool use_mutex>
int child_process<use_mutex>::get_pid() const {
  detail::lock<use_mutex> l(&_protect);
  if (_proc) {
    return _proc->proc.id();
  }
  return -1;
}

/**
 * @brief Schedule an asynchronous wait on the child process termination.
 *
 * Registers a one-shot async_wait handler on the underlying boost::process
 * object. When the process exits, the handler calls _on_process_end() with
 * the resulting error code and raw exit status.
 *
 * @tparam use_mutex Whether internal operations are mutex-protected.
 */
template <bool use_mutex>
void child_process<use_mutex>::_async_wait_process_end() {
  if (_proc) {
    _proc->proc.async_wait(
        [me = shared_from_this()](const boost::system::error_code& err,
                                  int raw_exit_status) {
          me->_on_process_end(err, raw_exit_status);
        });
  }
}

/**
 * @brief called when child process end
 *
 * @param err
 * @param raw_exit_status end status of the process
 */
template <bool use_mutex>
void child_process<use_mutex>::_on_process_end(
    const boost::system::error_code& err,
    int raw_exit_status) {
  {
    detail::lock<use_mutex> l(&_protect);
    if (err) {
      // due to a bug in boost::process, we don't take this error into account
      // if we had terminated child process before
      if (_terminated) {
        _exit_code = _proc->proc.exit_code();
      } else {
        SPDLOG_LOGGER_ERROR(_logger, "pid:{} fail async_wait: {}",
                            _proc->proc.handle().id(), err.message());
        _exit_code = -1;
      }
    } else {
      if (_exit_status != e_exit_status::timeout) {
        _exit_status = e_exit_status::normal;
      }
      _exit_code = boost::process::v2::evaluate_exit_code(raw_exit_status);
      SPDLOG_LOGGER_DEBUG(_logger, "pid:{} end of process, exit_code={}",
                          _proc->proc.handle().id(), _exit_code);
    }
  }
  _completion_flags.fetch_or(e_completion_flags::process_end);
  _on_completion();
}

/**
 * @brief write some data to child process stdin, if a write is pending, data is
 * pushed to a queue
 *
 * @param data
 */
template <bool use_mutex>
void child_process<use_mutex>::_stdin_write(
    const std::shared_ptr<std::string>& data) {
  detail::lock<use_mutex> l(&_protect);
  _stdin_write_no_lock(data);
}

/**
 * @brief asynchronously write some data to child process stdin, if a write is
 * pending, data is pushed to a queue
 *
 * @param data
 */
template <bool use_mutex>
void child_process<use_mutex>::_stdin_write_no_lock(
    const std::shared_ptr<std::string>& data) {
  if (!_proc) {
    SPDLOG_LOGGER_ERROR(_logger, "stdin_write process not started");
    throw exceptions::msg_fmt("stdin_write process not started");
  }
  if (_write_pending) {
    _stdin_write_queue.push_back(data);
  } else {
    try {
      _write_pending = true;
      _stdin_pipe.async_write_some(
          asio::buffer(*data),
          [me = shared_from_this(), data](const boost::system::error_code& err,
                                          size_t nb_written [[maybe_unused]]) {
            me->_on_stdin_write(err);
            detail::lock<use_mutex> l(&me->_protect);
            me->_priv_on_stdin_write(err);
          });
    } catch (const std::exception& e) {
      _write_pending = false;
      SPDLOG_LOGGER_ERROR(_logger,
                          "pid:{} stdin_write fail to write to stdin {}",
                          _proc->proc.handle().id(), e.what());
    }
  }
}

/**
 * @brief stdin write handler
 * if data remains in queue, we send them
 *
 * @param err
 */
template <bool use_mutex>
void child_process<use_mutex>::_priv_on_stdin_write(
    const boost::system::error_code& err) {
  _write_pending = false;

  if (err) {
    if (err == asio::error::eof) {
      SPDLOG_LOGGER_DEBUG(_logger, "fail to write to child pid:{} to stdin {}",
                          _proc->proc.handle().id(), err.message());
    } else {
      SPDLOG_LOGGER_ERROR(_logger, "fail to write to child pid:{} to stdin {}",
                          _proc->proc.handle().id(), err.message());
    }
    return;
  }

  if (!_stdin_write_queue.empty()) {
    std::shared_ptr<std::string> to_send = _stdin_write_queue.front();
    _stdin_write_queue.pop_front();
    _stdin_write_no_lock(to_send);
  }
}

/**
 * @brief asynchronous read from child process stdout
 *
 */
template <bool use_mutex>
void child_process<use_mutex>::_stdout_read() {
  if (_proc) {
    SPDLOG_LOGGER_TRACE(_logger, "_stdout_read from child pid:{}",
                        _proc->proc.handle().id());
    try {
      _stdout_pipe.async_read_some(
          asio::buffer(_stdout_read_buffer),
          [me = shared_from_this()](const boost::system::error_code& err,
                                    size_t nb_read) {
            SPDLOG_LOGGER_TRACE(
                me->_logger, "from child pid:{}, {} bytes received on stdout",
                me->_proc->proc.handle().id(), nb_read);
            me->_on_stdout_read(err, nb_read);
          });
    } catch (const std::exception& e) {
      asio::post(*_io_context, [me = shared_from_this()]() {
        me->_on_stdout_read(std::make_error_code(std::errc::broken_pipe), 0);
      });
    }
  }
}

/**
 * @brief stdout read handler
 *
 * @param err
 * @param nb_read
 */
template <bool use_mutex>
void child_process<use_mutex>::_on_stdout_read(
    const boost::system::error_code& err,
    size_t nb_read) {
  std::string received;
  {
    detail::lock<use_mutex> l(&_protect);
    if (err) {
      if (err == asio::error::eof || err == asio::error::broken_pipe) {
        SPDLOG_LOGGER_DEBUG(_logger,
                            "from child pid:{} end read from stdout: {}",
                            _proc->proc.handle().id(), err.message());
      } else {
        SPDLOG_LOGGER_ERROR(
            _logger, "pid:{} fail read from stdout of child process: {} {}",
            _proc->proc.handle().id(), err.value(), err.message());
      }
      _completion_flags.fetch_or(e_completion_flags::stdout_eof);
    } else {
      SPDLOG_LOGGER_TRACE(_logger, "from child pid:{} read from stdout: {}",
                          _proc->proc.handle().id(),
                          std::string_view(_stdout_read_buffer, nb_read));
      received.assign(_stdout_read_buffer, nb_read);
    }
  }
  if (!received.empty()) {
    _on_stdout_read(err, received);
  }
  if (!err) {
    detail::lock<use_mutex> l(&_protect);
    _stdout_read();
  } else {
    _on_completion();
  }
}

/**
 * @brief asynchronous read from child process stderr
 *
 */
template <bool use_mutex>
void child_process<use_mutex>::_stderr_read() {
  if (_proc) {
    try {
      _stderr_pipe.async_read_some(
          asio::buffer(_stderr_read_buffer),
          [me = shared_from_this()](const boost::system::error_code& err,
                                    size_t nb_read) {
            me->_on_stderr_read(err, nb_read);
          });
    } catch (const std::exception& e) {
      asio::post(*_io_context, [me = shared_from_this()]() {
        me->_on_stderr_read(std::make_error_code(std::errc::broken_pipe), 0);
      });
    }
  }
}

/**
 * @brief stderr read handler
 *
 * @param err
 * @param nb_read
 */
template <bool use_mutex>
void child_process<use_mutex>::_on_stderr_read(
    const boost::system::error_code& err,
    size_t nb_read) {
  std::string received;
  {
    detail::lock<use_mutex> l(&_protect);
    if (err) {
      _completion_flags.fetch_or(e_completion_flags::stderr_eof);
      if (err == asio::error::eof || err == asio::error::broken_pipe) {
        SPDLOG_LOGGER_DEBUG(_logger,
                            "from child pid:{} end read from stderr : {}",
                            _proc->proc.handle().id(), err.message());
      } else {
        SPDLOG_LOGGER_ERROR(
            _logger, "from child pid:{} fail read from stderr: {} {}",
            _proc->proc.handle().id(), err.value(), err.message());
      }
    } else {
      SPDLOG_LOGGER_TRACE(_logger,
                          "from child pid:{} process: read from stderr: {}",
                          _proc->proc.handle().id(),
                          std::string_view(_stderr_read_buffer, nb_read));
      received.assign(_stderr_read_buffer, nb_read);
    }
  }

  if (!received.empty()) {
    _on_stderr_read(err, received);
  }
  if (!err) {
    detail::lock<use_mutex> l(&_protect);
    _stderr_read();
  } else {
    _on_completion();
  }
}

/**
 * @brief called when process end or stdout/stderr eof.
 * Once process is ended and stdin and stdout also, we call handler
 *
 * @tparam use_mutex
 */
template <bool use_mutex>
void child_process<use_mutex>::_on_completion() {
  unsigned expected = _stderr_pipe.is_open()
                          ? e_completion_flags::all_completed
                          : e_completion_flags::stdout_process_completed;
  if (_completion_flags.compare_exchange_strong(
          expected, e_completion_flags::handler_called)) {
    _on_process_end();
  }
}

/**
 * @brief Check whether the child process is still running.
 *
 * @tparam use_mutex Whether internal operations are mutex-protected.
 * @return true if the process has been started and its handle is still open,
 *         false if it has not been started or has already exited.
 */
template <bool use_mutex>
bool child_process<use_mutex>::is_alive() const {
  detail::lock<use_mutex> l(&_protect);
  return _proc && _proc->proc.is_open();
}

/**
 * @brief kill child process 9
 *
 */
template <bool use_mutex>
void child_process<use_mutex>::kill() {
  detail::lock<use_mutex> l(&_protect);
  if (_proc) {
    auto child_pid = _proc->proc.handle().id();
    SPDLOG_LOGGER_INFO(_logger, "kill with SIGKILL process child pid:{}",
                       child_pid);
    boost::system::error_code err;
    _proc->proc.terminate(err);
    _terminated = true;
    if (err) {
      SPDLOG_LOGGER_INFO(_logger, "fail to kill with SIGKILL child pid:{}: {}",
                         child_pid, err.message());
    }
  }
}

/**
 * @brief kill child process SIGTERM
 *
 */
template <bool use_mutex>
void child_process<use_mutex>::request_exit() {
  detail::lock<use_mutex> l(&_protect);
  if (_proc) {
    auto child_pid = _proc->proc.handle().id();
    SPDLOG_LOGGER_INFO(_logger, "kill with SIGTERM process child pid:{}",
                       child_pid);
    boost::system::error_code err;
    _proc->proc.request_exit(err);
    _terminated = true;
    if (err) {
      SPDLOG_LOGGER_INFO(_logger, "fail to kill with SIGTERM child pid:{}: {}",
                         child_pid, err.message());
    }
  }
}

namespace com::centreon::common {

template class child_process<true>;

template class child_process<false>;

}  // namespace com::centreon::common
