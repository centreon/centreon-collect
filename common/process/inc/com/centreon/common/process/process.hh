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

#ifndef CENTREON_COMMON_PROCESS_HH
#define CENTREON_COMMON_PROCESS_HH

#include "boost/process/v2/environment.hpp"
#include "com/centreon/common/process/process_args.hh"

namespace com::centreon::common {

namespace detail {
// here to limit included files
struct boost_process;
}  // namespace detail

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
enum e_exit_status { normal = 0, crash = 1, timeout = 2 };

/**
 * @brief This class creates a child process, stdin, stdout and stderr are piped
 * to father process.
 * It's full asynchronous, and relies on boost v2 process. On linux version, we
 * don't use boost process child process launcher but a spawnp home made one.
 *
 * It's a one shot class not reusable.
 * That's why we pass executable path, arguments and environment with shared
 * pointers in order to not compute, allocate these parameters each time we
 * start the same process.
 *
 * It also manages a timeout. When child duration goes more than
 * timeout, we kill (-9) child process and we handle child process die the same
 * way as normal completion. The normal usage of this class is to create a
 * shared_ptr of this class, start child process with start_process method and
 * forget it. Be carefull if you keep a reference of this pointer because
 * process keeps completion handler and you may have a mutual ownership.
 *
 * pipe to stdin is optional because some windows scripts may fails if they do
 * not manage stdin
 * You can write to stdin with write_to_stdin method. writes are non blocking,
 * buffered and thread safe.
 *
 * Example:
 * @code {.cpp}
 *  std::tie(_last_exe_path, _last_args) =
 *  common::process<true>::parse_cmd_line(processed_cmd);
 *
 *  std::shared_ptr<common::process<true>> p =
 *  std::make_shared<common::process<true>>(g_io_context, commands_logger,
 *                                               _last_exe_path, true, false,
 *                                               _last_args, env);
 *  p->start_process(
 *      [me = shared_from_this(), command_id, start = time(nullptr)](
 *         const common::process<true>& proc, int exit_code, int exit_status,
 *         const std::string& std_out, const std::string& std_err) {
 *         me->_on_complete(command_id, start, exit_code, exit_status, std_out,
 *         std_err);
 *      },
 *      std::chrono::seconds(timeout));
 * @endcode
 *
 *
 * @tparam use_mutex true for multi-threads programs
 */
template <bool use_mutex = true>
class process : public std::enable_shared_from_this<process<use_mutex>> {
 public:
  using std::enable_shared_from_this<process<use_mutex>>::shared_from_this;

  using shared_env = std::shared_ptr<boost::process::v2::process_environment>;

 private:
  process_args::pointer _args;
  bool _use_setpgid;
  bool _use_stdin;
  shared_env _env;
  std::deque<std::shared_ptr<std::string>> _stdin_write_queue;
  bool _write_pending = false;
  std::shared_ptr<spdlog::logger> _logger;

  std::shared_ptr<asio::io_context> _io_context;
  asio::system_timer _timeout_timer;
  asio::readable_pipe _stdout_pipe;
  asio::readable_pipe _stderr_pipe;
  asio::writable_pipe _stdin_pipe;
  detail::boost_process* _proc = nullptr;
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

  using handler_type = std::function<void(const process<use_mutex>& proc,
                                          int /*exit_code*/,
                                          int, /*exit status*/
                                          const std::string& /*stdout*/,
                                          const std::string& /*stderr*/
                                          )>;

  handler_type _handler;
  std::string _stdout;
  std::string _stderr;
  int _exit_status = e_exit_status::crash;
  int _exit_code = -1;

  enum e_completion_flags : unsigned {
    process_end = 1,
    stdout_eof = 2,
    stderr_eof = 4,
    all_completed = 7,
    handler_called = 8
  };

  std::atomic_uint _completion_flags = 0;

  void _stdin_write_no_lock(const std::shared_ptr<std::string>& data);
  void _stdin_write(const std::shared_ptr<std::string>& data);

  void _stdout_read();
  void _stderr_read();

  char _stdout_read_buffer[0x1000];
  char _stderr_read_buffer[0x1000];

  mutable detail::mutex<use_mutex> _protect;

  void _on_stdout_read(const boost::system::error_code& err, size_t nb_read);
  void _on_stderr_read(const boost::system::error_code& err, size_t nb_read);

  void _on_process_end(const boost::system::error_code& err,
                       int raw_exit_status);

  void _on_stdin_write(const boost::system::error_code& err);

  void _on_timeout();

  void _on_completion();

  void _create_process();

 public:
  template <typename string_type>
  process(const std::shared_ptr<boost::asio::io_context>& io_context,
          const std::shared_ptr<spdlog::logger>& logger,
          const std::string_view& exe_path,
          bool use_setpgid,
          bool use_stdin,
          const std::initializer_list<string_type>& args,
          const shared_env& env);

  process(const std::shared_ptr<boost::asio::io_context>& io_context,
          const std::shared_ptr<spdlog::logger>& logger,
          const process_args::pointer& args,
          bool use_setpgid,
          bool use_stdin,
          const shared_env& env);

  process(const std::shared_ptr<boost::asio::io_context>& io_context,
          const std::shared_ptr<spdlog::logger>& logger,
          const std::string_view& cmd_line,
          bool use_setpgid,
          bool use_stdin,
          const shared_env& env);

  ~process();

  static process_args::pointer parse_cmd_line(const std::string_view& cmd_line);

  template <typename string_class>
  void write_to_stdin(const string_class& content);

  void start_process(handler_type&& handler,
                     const std::chrono::system_clock::duration& timeout);

  std::string get_stdout() const {
    detail::lock<use_mutex> l(&_protect);
    return _stdout;
  }

  std::string get_stderr() const {
    detail::lock<use_mutex> l(&_protect);
    return _stderr;
  }

  int get_exit_code() const { return _exit_code; }

  void kill();
};

/**
 * @brief Construct a new process::process object
 *
 * @tparam string_type string_class such as string_view, char* string or
 * anything else that can be used to construct a std::string
 * @param io_context
 * @param logger
 * @param exe_path exe without arguments
 * @param use_setpgid if true, we set process group of child process
 * @param use_stdin if true, we open a pipe to child stdin
 * @param args brace of arguments {"--flag1", "arg1", "-c", "arg2"}
 * @param env child environment
 */
template <bool use_mutex>
template <typename string_type>
process<use_mutex>::process(
    const std::shared_ptr<boost::asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    const std::string_view& exe_path,
    bool use_setpgid,
    bool use_stdin,
    const std::initializer_list<string_type>& args,
    const shared_env& env)
    : _use_setpgid(use_setpgid),
      _use_stdin(use_stdin),
      _env(env),
      _logger(logger),
      _io_context(io_context),
      _timeout_timer(*io_context),
      _stdout_pipe(*io_context),
      _stderr_pipe(*io_context),
      _stdin_pipe(*io_context) {
  _args = std::make_shared<process_args>(exe_path, args);
}

/**
 * @brief write string to child process stdin
 *
 * @tparam string_class such as string_view, char* string or anything else that
 * can be used to construct a std::string
 * @param content
 */
template <bool use_mutex>
template <typename string_class>
void process<use_mutex>::write_to_stdin(const string_class& content) {
  _stdin_write(std::make_shared<std::string>(content));
}

}  // namespace com::centreon::common
#endif
