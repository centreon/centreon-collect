/**
 * Copyright 2024 Centreon
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

#ifndef CENTREON_COMMON_CHILD_PROCESS_HH
#define CENTREON_COMMON_CHILD_PROCESS_HH

#include "boost/asio/readable_pipe.hpp"
#include "boost/asio/writable_pipe.hpp"
namespace com::centreon::common {
namespace detail {
template <bool use_mutex>
class mutex;

template <bool use_mutex>
class lock;

template <>
class mutex<true> : public absl::Mutex {};

template <>
class lock<true> : public absl::MutexLock {
 public:
  lock(absl::Mutex* mut) : absl::MutexLock(mut) {}
};

template <>
class mutex<false> {};

template <>
class lock<false> {
 public:
  lock(mutex<false>* /* dummy_mut*/) {}
};

struct boost_process;
}  // namespace detail

/**
 * @brief status of execution of a child process
 * crash is never returned but is there to ensure backward compatibility
 * with clib
 *
 */
enum e_exit_status : unsigned { normal = 0, crash = 1, timeout = 2 };

template <bool use_mutex = true>
class child_process
    : public std::enable_shared_from_this<child_process<use_mutex>> {
 public:
  using std::enable_shared_from_this<
      child_process<use_mutex>>::shared_from_this;

 private:
  std::deque<std::shared_ptr<std::string>> _stdin_write_queue
      ABSL_GUARDED_BY(_protect);
  bool _write_pending = false;

  /**
   * @brief workaround
   * in process lib, terminate method calls waitpid and father process can get
   * exit status. Then on async_wait completion, waitpid is also called and
   * waitpid may return ECHILD( unknown child). So in that case, we don't take
   * this error into account and we use status previously stored in first
   * waitpid
   * issue: https://github.com/boostorg/process/issues/496
   */
  bool _terminated = false;

  char _stdout_read_buffer[0x1000];
  char _stderr_read_buffer[0x1000];

  enum e_completion_flags : unsigned {
    process_end = 1,
    stdout_eof = 2,
    stderr_eof = 4,
    all_completed = 7,
    handler_called = 8
  };

  std::atomic_uint _completion_flags = 0;

  std::atomic<e_exit_status> _exit_status = e_exit_status::crash;
  int _exit_code = -1;

  void _stdin_write_no_lock(const std::shared_ptr<std::string>& data)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(_protect);
  void _stdin_write(const std::shared_ptr<std::string>& data);
  void _on_stdin_write(const boost::system::error_code& err);

  void _on_stdout_read(const boost::system::error_code& err, size_t nb_read);

  void _on_stderr_read(const boost::system::error_code& err, size_t nb_read);

  void _on_process_end(const boost::system::error_code& err,
                       int raw_exit_status);

  void _on_completion();

 protected:
  const std::shared_ptr<asio::io_context> _io_context;
  const std::shared_ptr<spdlog::logger> _logger;
  asio::readable_pipe _stdout_pipe ABSL_GUARDED_BY(_protect);
  asio::readable_pipe _stderr_pipe ABSL_GUARDED_BY(_protect);
  asio::writable_pipe _stdin_pipe ABSL_GUARDED_BY(_protect);
  detail::boost_process* _proc = nullptr;
  mutable detail::mutex<use_mutex> _protect;

  void _stdout_read();
  void _stderr_read();
  void _async_wait_process_end();

  virtual void _on_stdout_read([[maybe_unused]] const std::string received) {};
  virtual void _on_stderr_read([[maybe_unused]] const std::string received) {};

  virtual void _on_process_end() {};

  void _set_exit_status(e_exit_status status) { _exit_status = status; }

 public:
  /**
   * @brief Construct a new child_process object.
   *
   * @param io_context The Boost.Asio I/O context used for all asynchronous
   *        operations (reads, writes, process wait).
   * @param logger spdlog logger instance shared with the caller.
   */
  child_process(const std::shared_ptr<asio::io_context> io_context,
                const std::shared_ptr<spdlog::logger>& logger)
      : _io_context(io_context),
        _logger(logger),
        _stdout_pipe(*io_context),
        _stderr_pipe(*io_context),
        _stdin_pipe(*io_context) {}

  virtual ~child_process();

  /**
   * @brief Indicate whether the child process has ever been started.
   *
   * @return true if at least one completion flag has been set (i.e. the process
   *         was launched and produced at least one I/O or termination event),
   *         false if the process was never started.
   */
  bool had_been_started() const { return _completion_flags; }

  std::shared_ptr<spdlog::logger> get_logger() const { return _logger; }

  int get_pid() const;
  e_exit_status get_exit_status() const { return _exit_status; }
  int get_exit_code() const { return _exit_code; }

  bool is_alive() const;

  template <typename string_class>
  void write_to_stdin(const string_class& content);

  void close_stdin() {
    boost::system::error_code ec;
    [[maybe_unused]] auto ignored = _stdin_pipe.close(ec);
  }

  void kill();
};

/**
 * @brief write string to child process stdin
 *
 * @tparam string_class such as string_view, char* string or anything else that
 * can be used to construct a std::string
 * @param content
 */
template <bool use_mutex>
template <typename string_class>
void child_process<use_mutex>::write_to_stdin(const string_class& content) {
  _stdin_write(std::make_shared<std::string>(content));
}

}  // namespace com::centreon::common

#endif
