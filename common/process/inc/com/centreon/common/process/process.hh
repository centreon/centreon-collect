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

#include <memory>
#include "boost/process/v2/environment.hpp"
#include "com/centreon/common/process/child_process.hh"
#include "com/centreon/common/process/process_args.hh"

namespace com::centreon::common {

/**
 * @brief This class creates a child process, stdin, stdout and stderr are piped
 * to father process.
 * It's full asynchronous, and relies on boost v2 process. On linux version, we
 * don't use boost process child process launcher but a spawnp home made one.
 *
 * It manages stdout and stderr in two modes.
 *   - For one shot process, we start process without stderr and stdout read
 *     handlers and we get stdout and stderr at process ending.
 *   - For continuous used process, we start process with stderr and stdout read
 *     handlers. And these handlers will be called each time child process sends
 *     data.
 *
 * It's a one shot class not reusable. That's why we can pass executable path,
 * arguments and environment with shared pointers in order to not compute,
 * and allocate these parameters each time we start the same process.
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
 * You can write to stdin with write_to_child_stdin method. writes are non
 * blocking, buffered and thread safe.
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
class process : public child_process<use_mutex> {
 public:
  using shared_env = std::shared_ptr<boost::process::v2::process_environment>;

 private:
  const process_args::pointer _args;
  const bool _use_setpgid;
  const bool _use_stdin;
  const shared_env _env;

  using child_process<use_mutex>::_protect;
  using child_process<use_mutex>::_logger;
  using child_process<use_mutex>::_proc;

  asio::system_timer _timeout_timer ABSL_GUARDED_BY(_protect);

  using handler_type = std::function<void(const process<use_mutex>& proc,
                                          int /*exit_code*/,
                                          e_exit_status, /*exit status*/
                                          const std::string& /*stdout*/,
                                          const std::string& /*stderr*/
                                          )>;

  using reader_type = std::function<void(const boost::system::error_code&,
                                         const std::string_view&)>;

  handler_type _handler;
  std::string _stdout ABSL_GUARDED_BY(_protect);
  reader_type _stdout_handler;
  std::string _stderr ABSL_GUARDED_BY(_protect);
  reader_type _stderr_handler;

  void _start_process_nolock(handler_type&& handler,
                             const std::chrono::system_clock::duration& timeout)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(_protect);

  void _on_stdout_read(const boost::system::error_code& err,
                       const std::string received) override;
  void _on_stderr_read(const boost::system::error_code& err,
                       const std::string received) override;
  void _on_process_end() override;

  void _on_timeout();

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

  std::shared_ptr<process<use_mutex>> shared_from_this() {
    return std::static_pointer_cast<process<use_mutex>>(
        child_process<use_mutex>::shared_from_this());
  }

  std::shared_ptr<const process<use_mutex>> shared_from_this() const {
    return std::static_pointer_cast<const process<use_mutex>>(
        child_process<use_mutex>::shared_from_this());
  }

  static process_args::pointer parse_cmd_line(const std::string_view& cmd_line);

  void start_process(handler_type&& handler,
                     const std::chrono::system_clock::duration& timeout);

  void start_process(handler_type&& handler,
                     reader_type&& stdout_handler,
                     reader_type&& stderr_handler,
                     const std::chrono::system_clock::duration& timeout);

  std::string get_stdout() const {
    detail::lock<use_mutex> l(&_protect);
    return _stdout;
  }

  std::string get_stderr() const {
    detail::lock<use_mutex> l(&_protect);
    return _stderr;
  }

  const std::string& get_exe_path() const { return _args->get_exe_path(); }
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
    : child_process<use_mutex>(io_context, logger),
      _args(std::make_shared<process_args>(exe_path, args)),
      _use_setpgid(use_setpgid),
      _use_stdin(use_stdin),
      _env(env),
      _timeout_timer(*io_context) {}

}  // namespace com::centreon::common
#endif
