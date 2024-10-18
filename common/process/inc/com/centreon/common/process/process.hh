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

#ifndef CENTREON_AGENT_CHECK_PROCESS_HH
#define CENTREON_AGENT_CHECK_PROCESS_HH

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

}  // namespace detail

/**
 * @brief This class allow to exec a process asynchronously.
 * It's a base class. If you want to get stdin and stdout returned data, you
 * must inherit from this and override on_stdout_read and on_stderr_read
 * You can call start_process at any moment, if a process is already running,
 * it's killed
 * As we can start a process at any moment, all handlers take a caller in
 * parameter, if this caller is not equal to current _proc, we do nothing.
 * When completion methods like on_stdout_read are called, _protect is already
 * locked
 */

template <bool use_mutex = true>
class process : public std::enable_shared_from_this<process<use_mutex>> {
  using std::enable_shared_from_this<process<use_mutex>>::shared_from_this;
  std::string _exe_path;
  std::vector<std::string> _args;

  std::deque<std::shared_ptr<std::string>> _stdin_write_queue;
  bool _write_pending;

  std::shared_ptr<detail::boost_process> _proc;

  int _exit_status = 0;

  detail::mutex<use_mutex> _protect;

  void stdin_write_no_lock(const std::shared_ptr<std::string>& data);
  void stdin_write(const std::shared_ptr<std::string>& data);

  void stdout_read();
  void stderr_read();

 protected:
  std::shared_ptr<asio::io_context> _io_context;
  std::shared_ptr<spdlog::logger> _logger;

  char _stdout_read_buffer[0x1000];
  char _stderr_read_buffer[0x1000];

  virtual void on_stdout_read(const boost::system::error_code& err,
                              size_t nb_read);
  virtual void on_stderr_read(const boost::system::error_code& err,
                              size_t nb_read);

  virtual void on_process_end(const boost::system::error_code& err,
                              int raw_exit_status);

  virtual void on_stdin_write(const boost::system::error_code& err);

 public:
  template <typename string_iterator>
  process(const std::shared_ptr<boost::asio::io_context>& io_context,
          const std::shared_ptr<spdlog::logger>& logger,
          const std::string_view& exe_path,
          string_iterator arg_begin,
          string_iterator arg_end);

  template <typename args_container>
  process(const std::shared_ptr<boost::asio::io_context>& io_context,
          const std::shared_ptr<spdlog::logger>& logger,
          const std::string_view& exe_path,
          const args_container& args);

  template <typename string_type>
  process(const std::shared_ptr<boost::asio::io_context>& io_context,
          const std::shared_ptr<spdlog::logger>& logger,
          const std::string_view& exe_path,
          const std::initializer_list<string_type>& args);

  process(const std::shared_ptr<boost::asio::io_context>& io_context,
          const std::shared_ptr<spdlog::logger>& logger,
          const std::string_view& cmd_line);

  virtual ~process() = default;

  int get_pid();

  template <typename string_class>
  void write_to_stdin(const string_class& content);

  void start_process(bool enable_stdin);

  void kill();

  int get_exit_status() const { return _exit_status; }
  const std::string& get_exe_path() const { return _exe_path; }
};

/**
 * @brief Construct a new process::process object
 *
 * @tparam string_iterator
 * @param io_context
 * @param logger
 * @param exe_path path of executable without arguments
 * @param arg_begin iterator to first argument
 * @param arg_end iterator after the last argument
 */
template <bool use_mutex>
template <typename string_iterator>
process<use_mutex>::process(const std::shared_ptr<asio::io_context>& io_context,
                            const std::shared_ptr<spdlog::logger>& logger,
                            const std::string_view& exe_path,
                            string_iterator arg_begin,
                            string_iterator arg_end)
    : _exe_path(exe_path),
      _args(arg_begin, arg_end),
      _io_context(io_context),
      _logger(logger) {}

/**
 * @brief Construct a new process::process object
 *
 * @tparam args_container
 * @param io_context
 * @param logger
 * @param exe_path path of executable without argument
 * @param args container of arguments
 */
template <bool use_mutex>
template <typename args_container>
process<use_mutex>::process(
    const std::shared_ptr<boost::asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    const std::string_view& exe_path,
    const args_container& args)
    : _exe_path(exe_path),
      _args(args),
      _io_context(io_context),
      _logger(logger) {}

/**
 * @brief Construct a new process::process object
 *
 * @tparam string_type string_class such as string_view, char* string or
 * anything else that can be used to construct a std::string
 * @param io_context
 * @param logger
 * @param exe_path path of executable without argument
 * @param args brace of arguments {"--flag1", "arg1", "-c", "arg2"}
 */
template <bool use_mutex>
template <typename string_type>
process<use_mutex>::process(
    const std::shared_ptr<boost::asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    const std::string_view& exe_path,
    const std::initializer_list<string_type>& args)
    : _exe_path(exe_path), _io_context(io_context), _logger(logger) {
  _args.reserve(args.size());
  for (const auto& str : args) {
    _args.emplace_back(str);
  }
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
  stdin_write(std::make_shared<std::string>(content));
}

}  // namespace com::centreon::common
#endif
