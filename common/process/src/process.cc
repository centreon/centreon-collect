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

#include <boost/program_options/parsers.hpp>
#include "boost/system/detail/error_code.hpp"
#include "com/centreon/common/process/process_args.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

#include <boost/process/v2/stdio.hpp>

#include "com/centreon/common/process/process.hh"

#if !defined(BOOST_PROCESS_V2_WINDOWS)
#include "com/centreon/common/process/detail/spawnp_launcher.hh"
#else
#include <boost/process/v2/process.hpp>
#endif

#include "com/centreon/common/process/detail/boost_process.hh"

#pragma GCC diagnostic pop

namespace com::centreon::common::detail {

#if defined(BOOST_PROCESS_V2_WINDOWS)

/**
 * @brief The only goal of this struct is to set CREATE_NO_WINDOW flag in
 * CreateProcess call. Agent only start console applications, so we ensure that
 * no parasit cmd windows will be created.
 */
struct create_no_window {
  template <class launcher>
  boost::system::error_code on_setup(
      launcher& windows_launcher,
      const std::filesystem::path& /*executable*/,
      std::wstring& /*cmd_line*/) {
    windows_launcher.creation_flags |= CREATE_NO_WINDOW;
    return {};
  }
};
#endif

}  // namespace com::centreon::common::detail

using namespace com::centreon::common;

/**
 * @brief Construct a new process<use mutex>::process object
 *
 * @tparam use_mutex
 * @param io_context
 * @param logger
 * @param exe_path exe without arguments
 * @param use_setpgid if true, we set process group of child process
 * @param use_stdin if true, we open a stding pipe to child process, if false,
 * we open a stdin pipe, but we close it as soon as child process is created. By
 * doing this, child process will read an eof.
 * @param args command arguments
 * @param env environment (boost)
 */
template <bool use_mutex>
process<use_mutex>::process(
    const std::shared_ptr<boost::asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    const process_args::pointer& args,
    bool use_setpgid,
    bool use_stdin,
    const process::shared_env& env)
    : child_process<use_mutex>(io_context, logger),
      _args(args),
      _use_setpgid(use_setpgid),
      _use_stdin(use_stdin),
      _env(env),
      _timeout_timer(*io_context) {
  _use_stderr_pipe = true;
}

/**
 * @brief Construct a new process<use mutex>::process object
 *
 * @tparam use_mutex
 * @param io_context
 * @param logger
 * @param cmd_line command line
 * @param use_setpgid if true, we set process group of child process
 * @param use_stdin if true, we open a stding pipe to child process
 * @param env environment (boost)
 *
 * This constructor uses parse_cmd_line to create exe path and vector of
 * arguments.
 * If you execute several times the same command, you should rather
 * call static parse_cmd_line, then store the result and pass it to other
 * constructor
 */
template <bool use_mutex>
process<use_mutex>::process(
    const std::shared_ptr<boost::asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    const std::string_view& cmd_line,
    bool use_setpgid,
    bool use_stdin,
    const process::shared_env& env)
    : child_process<use_mutex>(io_context, logger),
      _args(parse_cmd_line(cmd_line)),
      _use_setpgid(use_setpgid),
      _use_stdin(use_stdin),
      _env(env),
      _timeout_timer(*io_context) {
  _use_stderr_pipe = true;
  SPDLOG_LOGGER_TRACE(logger, "create process {:p}",
                      static_cast<const void*>(this));
}

template <bool use_mutex>
process_args::pointer process<use_mutex>::parse_cmd_line(
    const std::string_view& cmd_line) {
#ifdef _WIN32
  auto split_res = boost::program_options::split_winmain(std::string(cmd_line));
  if (split_res.begin() == split_res.end()) {
    throw exceptions::msg_fmt("empty command line:\"{}\"", cmd_line);
  }

  std::string exe_path = *split_res.begin();
  split_res.erase(split_res.begin());

  return std::make_shared<process_args>(exe_path, std::move(split_res));
#else
  return std::make_shared<process_args>(cmd_line);
#endif
}

/**
 * @brief Destroy the process<use mutex>::process object
 *
 * @tparam use_mutex
 */
template <bool use_mutex>
process<use_mutex>::~process() {
  SPDLOG_LOGGER_TRACE(_logger, "delete process {:p}",
                      static_cast<const void*>(this));
}

/**
 * @brief start a new process, if a previous one is running, it's killed
 * In this function, we start child process and stdout, stderr asynchronous read
 * we also start an asynchronous read on process fd to be aware of child process
 * termination
 *
 * @param handler handler called at the end of child process
 * @param stdout_handler handler called each time child process write something
 * to stdout
 * @param stderr_handler handler called each time child process write something
 * to stderr
 * @param timeout child process will be called at this end of this timeout, pass
 * {} to avoid it
 */
template <bool use_mutex>
void process<use_mutex>::start_process(
    handler_type&& handler,
    reader_type&& stdout_handler,
    reader_type&& stderr_handler,
    const std::chrono::system_clock::duration& timeout) {
  detail::lock<use_mutex> l(&_protect);
  _stdout_handler = std::move(stdout_handler);
  _stderr_handler = std::move(stderr_handler);
  _start_process_nolock(std::move(handler), timeout);
}

template <bool use_mutex>
void process<use_mutex>::start_process(
    handler_type&& handler,
    const std::chrono::system_clock::duration& timeout) {
  detail::lock<use_mutex> l(&_protect);
  _stdout_handler = reader_type();
  _stderr_handler = reader_type();
  _start_process_nolock(std::move(handler), timeout);
}

/**
 * @brief start a new process, if a previous one is running, it's killed
 * In this function, we start child process and stdout, stderr asynchronous read
 * we also start an asynchronous read on process fd to be aware of child process
 * termination
 *
 * @param handler handler called at the end of child process
 * @param timeout child process will be called at this end of this timeout, pass
 * {} to avoid it
 */
template <bool use_mutex>
void process<use_mutex>::_start_process_nolock(
    handler_type&& handler,
    const std::chrono::system_clock::duration& timeout) {
  SPDLOG_LOGGER_DEBUG(_logger, "start process: {}", *_args);
  _handler = std::move(handler);

  if (this->had_been_started()) {
    throw exceptions::msg_fmt(
        "this class must be used only one time for process: {}", *_args);
  }

  try {
    _create_process();
    this->_async_wait_process_end();
    SPDLOG_LOGGER_DEBUG(_logger, "pid:{} process started: {}",
                        _proc->proc.handle().id(), *_args);
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "fail to start {}: {}", *_args, e.what());
    throw;
  }
  this->_stdout_read();
  this->_stderr_read();

  if (timeout.count()) {
    _timeout_timer.expires_after(timeout);
    _timeout_timer.async_wait(
        [me = shared_from_this()](const boost::system::error_code& err) {
          if (!err) {
            me->_on_timeout();
          }
        });
  }
}

static const std::vector<std::string> _no_args;

#if defined(BOOST_PROCESS_V2_WINDOWS)

template <bool use_mutex>
void process<use_mutex>::_create_process() {
  if (_env && !_env->env_buffer.empty()) {
    _proc = new detail::boost_process(
        boost::process::v2::basic_process<asio::io_context::executor_type>(
            *this->_io_context, _args->get_exe_path(), _args->get_args(),
            boost::process::v2::process_stdio{
                this->_stdin_pipe, this->_stdout_pipe, this->_stderr_pipe},
            *_env, detail::create_no_window()));
  } else {
    _proc = new detail::boost_process(
        boost::process::v2::basic_process<asio::io_context::executor_type>(
            *this->_io_context, _args->get_exe_path(), _args->get_args(),
            boost::process::v2::process_stdio{
                this->_stdin_pipe, this->_stdout_pipe, this->_stderr_pipe},
            detail::create_no_window()));
  }
  if (!_use_stdin) {  // we don't want a stdin for child process => stdin read
                      // from child process will get an eof
    this->close_stdin();
  }
}

#else
/**
 * @brief creates a child process (linux version)
 * it uses spawnp.
 *
 * @tparam use_mutex
 */
template <bool use_mutex>
void process<use_mutex>::_create_process() {
  char* const* env = (_env && !_env->env_buffer.empty())
                         ? const_cast<char* const*>(_env->env.data())
                         : nullptr;
  this->_proc = new detail::boost_process(detail::spawnp(
      *this->_io_context, _args, _use_setpgid,
      boost::process::v2::detail::process_input_binding(this->_stdin_pipe).fd,
      boost::process::v2::detail::process_output_binding(this->_stdout_pipe).fd,
      boost::process::v2::detail::process_error_binding(this->_stderr_pipe).fd,
      env));
  if (!_use_stdin) {  // we don't want a stdin for child process => stdin read
    // from child process will get an eof
    this->close_stdin();
  }
}

#endif

template <bool use_mutex>
void process<use_mutex>::_on_process_end() {
  {
    detail::lock<use_mutex> l(&_protect);
    _timeout_timer.cancel();
  }
  _handler(*this, this->get_exit_code(), this->get_exit_status(), _stdout,
           _stderr);
}

/**
 * @brief stdout read handler
 * This method or his override is called with _protect locked.
 * If override process::on_stdout_read must be called
 *
 * @param err
 * @param nb_read
 */
template <bool use_mutex>
void process<use_mutex>::_on_stdout_read(const boost::system::error_code& err,
                                         const std::string received) {
  if (_stdout_handler) {
    _stdout_handler(err, received);
  } else {
    _stdout.append(received);
  }
}

/**
 * @brief stderr read handler
 * This method or his override is called with _protect locked.
 * If override process::on_stderr_read must be called
 *
 * @param err
 * @param nb_read
 */
template <bool use_mutex>
void process<use_mutex>::_on_stderr_read(const boost::system::error_code& err,
                                         const std::string received) {
  if (_stderr_handler) {
    _stderr_handler(err, received);
  } else {
    _stderr.append(received);
  }
}

/**
 * @brief timeout handler. It kills process. Completion will be done by process
 * completion
 *
 * @tparam use_mutex
 */
template <bool use_mutex>
void process<use_mutex>::_on_timeout() {
  if (this->is_alive()) {
    SPDLOG_LOGGER_ERROR(_logger, "pid:{} timeout process {} => kill",
                        this->get_pid(), *_args);
    this->_set_exit_status(e_exit_status::timeout);
    this->kill();
  }
}

namespace com::centreon::common {

template class process<true>;

template class process<false>;

}  // namespace com::centreon::common
