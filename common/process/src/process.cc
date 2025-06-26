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

#pragma GCC diagnostic pop

namespace proc = boost::process::v2;

namespace com::centreon::common::detail {

/**
 * @brief The only goal of this struct is to hide boost::process implementation
 * So, you will find a shared_ptr<boost_process> attribute in process class
 * I don't know why, but you can't define a unique_ptr of unknown struct in a
 * class attribute so, we use raw pointer instead
 *
 */
struct boost_process {
  boost_process(
      boost::process::v2::basic_process<asio::io_context::executor_type>&&
          proc_created)
      : proc(std::move(proc_created)) {}

  boost_process(const boost_process&) = delete;
  boost_process& operator=(const boost_process&) = delete;

  boost::process::v2::basic_process<asio::io_context::executor_type> proc;
};

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
 * @param use_stdin if true, we open a stding pipe to child process
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
    : _args(args),
      _use_setpgid(use_setpgid),
      _use_stdin(use_stdin),
      _env(env),
      _logger(logger),
      _io_context(io_context),
      _timeout_timer(*io_context),
      _stdout_pipe(*io_context),
      _stderr_pipe(*io_context),
      _stdin_pipe(*io_context) {}

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
    : _use_setpgid(use_setpgid),
      _use_stdin(use_stdin),
      _env(env),
      _logger(logger),
      _io_context(io_context),
      _timeout_timer(*io_context),
      _stdout_pipe(*io_context),
      _stderr_pipe(*io_context),
      _stdin_pipe(*io_context) {
  _args = parse_cmd_line(cmd_line);
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
  if (_proc) {
    delete _proc;
  }
}

/**
 * @brief start a new process, if a previous one is running, it's killed
 * In this function, we start child process and stdout, stderr asynchronous read
 * we also start an asynchronous read on process fd to be aware of child process
 * termination
 *
 * @param enable_stdin On Windows set it to false if you doesn't want to write
 * on child stdin
 */
template <bool use_mutex>
void process<use_mutex>::start_process(
    handler_type&& handler,
    const std::chrono::system_clock::duration& timeout) {
  SPDLOG_LOGGER_DEBUG(_logger, "start process: {}", *_args);
  detail::lock<use_mutex> l(&_protect);
  _handler = std::move(handler);

  if (_completion_flags) {
    throw exceptions::msg_fmt(
        "this class must be used only one time for process: {}", *_args);
  }

  try {
    _create_process();
    _proc->proc.async_wait(
        [me = shared_from_this()](const boost::system::error_code& err,
                                  int raw_exit_status) {
          me->_on_process_end(err, raw_exit_status);
        });
    SPDLOG_LOGGER_DEBUG(_logger, "pid:{} process started: {}",
                        _proc->proc.native_handle(), *_args);
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "fail to start {}: {}", *_args, e.what());
    throw;
  }
  _stdout_read();
  _stderr_read();

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
    if (_use_stdin) {
      _proc = new detail::boost_process(
          boost::process::v2::basic_process<asio::io_context::executor_type>(
              *_io_context, _args->get_exe_path(), _args->get_args(),
              boost::process::v2::process_stdio{_stdin_pipe, _stdout_pipe,
                                                _stderr_pipe},
              *_env));
    } else {
      _proc = new detail::boost_process(
          boost::process::v2::basic_process<asio::io_context::executor_type>(
              *_io_context, _args->get_exe_path(), _args->get_args(),
              boost::process::v2::process_stdio{{}, _stdout_pipe, _stderr_pipe},
              *_env));
    }
  } else {
    if (_use_stdin) {
      _proc = new detail::boost_process(
          boost::process::v2::basic_process<asio::io_context::executor_type>(
              *_io_context, _args->get_exe_path(), _args->get_args(),
              boost::process::v2::process_stdio{_stdin_pipe, _stdout_pipe,
                                                _stderr_pipe}));
    } else {
      _proc = new detail::boost_process(
          boost::process::v2::basic_process<asio::io_context::executor_type>(
              *_io_context, _args->get_exe_path(), _args->get_args(),
              boost::process::v2::process_stdio{
                  {}, _stdout_pipe, _stderr_pipe}));
    }
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
  if (_use_stdin) {
    _proc = new detail::boost_process(detail::spawnp(
        *_io_context, _args, _use_setpgid,
        proc::detail::process_input_binding(_stdin_pipe).fd,
        proc::detail::process_output_binding(_stdout_pipe).fd,
        proc::detail::process_error_binding(_stderr_pipe).fd, env));
  } else {
    _proc = new detail::boost_process(detail::spawnp(
        *_io_context, _args, _use_setpgid, -1,
        proc::detail::process_output_binding(_stdout_pipe).fd,
        proc::detail::process_error_binding(_stderr_pipe).fd, env));
  }
}

#endif

/**
 * @brief called when child process end
 *
 * @param err
 * @param raw_exit_status end status of the process
 */
template <bool use_mutex>
void process<use_mutex>::_on_process_end(const boost::system::error_code& err,
                                         int raw_exit_status) {
  {
    detail::lock<use_mutex> l(&_protect);
    if (err) {
      // due to a bug in boost::process, we don't take this error into account
      // if we had terminated child process before
      if (_terminated) {
        _exit_code = _proc->proc.exit_code();
      } else {
        SPDLOG_LOGGER_ERROR(_logger, "pid:{} fail async_wait of {}: {}",
                            _proc->proc.native_handle(), *_args, err.message());
        _exit_code = -1;
      }
    } else {
      if (_exit_status != e_exit_status::timeout) {
        _exit_status = e_exit_status::normal;
      }
      _exit_code = proc::evaluate_exit_code(raw_exit_status);
      SPDLOG_LOGGER_DEBUG(_logger, "pid:{} end of process {}, exit_code={}",
                          _proc->proc.native_handle(), *_args, _exit_code);
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
void process<use_mutex>::_stdin_write(
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
void process<use_mutex>::_stdin_write_no_lock(
    const std::shared_ptr<std::string>& data) {
  if (!_proc) {
    SPDLOG_LOGGER_ERROR(_logger, "stdin_write process {} not started", *_args);
    throw exceptions::msg_fmt("stdin_write process {} not started", *_args);
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
            detail::lock<use_mutex> l(&me->_protect);
            me->_on_stdin_write(err);
          });
    } catch (const std::exception& e) {
      _write_pending = false;
      SPDLOG_LOGGER_ERROR(_logger,
                          "stdin_write process {} fail to write to stdin {}",
                          *_args, e.what());
    }
  }
}

/**
 * @brief stdin write handler
 * if data remains in queue, we send them
 * if override process::on_stdin_write must be called
 *
 * @param err
 */
template <bool use_mutex>
void process<use_mutex>::_on_stdin_write(const boost::system::error_code& err) {
  _write_pending = false;

  if (err) {
    if (err == asio::error::eof) {
      SPDLOG_LOGGER_DEBUG(_logger,
                          "on_stdin_write process {} fail to write to stdin {}",
                          *_args, err.message());
    } else {
      SPDLOG_LOGGER_ERROR(_logger,
                          "on_stdin_write process {} fail to write to stdin {}",
                          *_args, err.message());
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
void process<use_mutex>::_stdout_read() {
  if (_proc) {
    try {
      _stdout_pipe.async_read_some(
          asio::buffer(_stdout_read_buffer),
          [me = shared_from_this()](const boost::system::error_code& err,
                                    size_t nb_read) {
            me->_on_stdout_read(err, nb_read);
          });
    } catch (const std::exception& e) {
      asio::post(*_io_context, [me = shared_from_this()]() {
        detail::lock<use_mutex> l(&me->_protect);
        me->_on_stdout_read(std::make_error_code(std::errc::broken_pipe), 0);
      });
    }
  }
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
                                         size_t nb_read) {
  bool eof = false;
  {
    detail::lock<use_mutex> l(&_protect);
    if (err) {
      if (err == asio::error::eof || err == asio::error::broken_pipe) {
        SPDLOG_LOGGER_DEBUG(_logger, "fail read from stdout of process {}: {}",
                            *_args, err.message());
      } else {
        SPDLOG_LOGGER_ERROR(_logger,
                            "fail read from stdout of process {}: {} {}",
                            *_args, err.value(), err.message());
      }
      _completion_flags.fetch_or(e_completion_flags::stdout_eof);
      eof = true;
    } else {
      SPDLOG_LOGGER_TRACE(_logger, " process: {} read from stdout: {}", *_args,
                          std::string_view(_stdout_read_buffer, nb_read));
      _stdout.append(_stdout_read_buffer, nb_read);
      _stdout_read();
    }
  }
  if (eof) {
    _on_completion();
  }
}

/**
 * @brief asynchronous read from child process stderr
 *
 */
template <bool use_mutex>
void process<use_mutex>::_stderr_read() {
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
        detail::lock<use_mutex> l(&me->_protect);
        me->_on_stderr_read(std::make_error_code(std::errc::broken_pipe), 0);
      });
    }
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
                                         size_t nb_read) {
  bool eof = false;
  {
    detail::lock<use_mutex> l(&_protect);
    if (err) {
      if (err == asio::error::eof || err == asio::error::broken_pipe) {
        SPDLOG_LOGGER_DEBUG(_logger, "fail read from stderr of process {}: {}",
                            *_args, err.message());
      } else {
        SPDLOG_LOGGER_ERROR(_logger,
                            "fail read from stderr of process {}: {} {}",
                            *_args, err.value(), err.message());
      }
      _completion_flags.fetch_or(e_completion_flags::stderr_eof);
      eof = true;
    } else {
      SPDLOG_LOGGER_TRACE(_logger, " process: {} read from stdout: {}", *_args,
                          std::string_view(_stderr_read_buffer, nb_read));
      _stderr.append(_stderr_read_buffer, nb_read);
      _stderr_read();
    }
  }
  if (eof) {
    _on_completion();
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
  detail::lock<use_mutex> l(&_protect);
  _exit_status = e_exit_status::timeout;
  if (_proc->proc.is_open()) {
    SPDLOG_LOGGER_ERROR(_logger, "pid:{} timeout process {} => kill",
                        _proc->proc.native_handle(), *_args);
    boost::system::error_code err;
    _proc->proc.terminate(err);
    _terminated = true;
  }
}

/**
 * @brief called when process end or stdout/stderr eof.
 * Once process is ended and stdin and stdout also, we call handler
 *
 * @tparam use_mutex
 */
template <bool use_mutex>
void process<use_mutex>::_on_completion() {
  unsigned expected = e_completion_flags::all_completed;
  if (_completion_flags.compare_exchange_strong(
          expected, e_completion_flags::handler_called)) {
    {
      detail::lock<use_mutex> l(&_protect);
      _timeout_timer.cancel();
    }
    _handler(*this, _exit_code, _exit_status, _stdout, _stderr);
  }
}

/**
 * @brief kill child process
 *
 */
template <bool use_mutex>
void process<use_mutex>::kill() {
  detail::lock<use_mutex> l(&_protect);
  if (_proc) {
    SPDLOG_LOGGER_INFO(_logger, "kill process {}", *_args);
    boost::system::error_code err;
    _proc->proc.terminate(err);
    _terminated = true;
    if (err) {
      SPDLOG_LOGGER_INFO(_logger, "fail to kill {}: {}", *_args, err.message());
    }
  }
}

namespace com::centreon::common {

template class process<true>;

template class process<false>;

}  // namespace com::centreon::common
