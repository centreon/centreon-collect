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

#include <boost/process/v2/stdio.hpp>
#include <boost/program_options/parsers.hpp>

#include "com/centreon/common/process/process.hh"

#if !defined(BOOST_PROCESS_V2_WINDOWS)
#include "com/centreon/common/process/detail/centreon_posix_process_launcher.hh"
#endif

#include <boost/process/v2/process.hpp>

namespace proc = boost::process::v2;

namespace com::centreon::common::detail {
/**
 * @brief each time we start a process we create this struct witch contains all
 * sub-process objects
 *
 */
struct boost_process {
#if defined(BOOST_PROCESS_V2_WINDOWS)
  /**
   * @brief Construct a new boost process object
   * stdin of the child process is managed
   *
   * @param io_context
   * @param exe_path  absolute or relative exe path
   * @param args  arguments of the command
   */
  boost_process(asio::io_context& io_context,
                const std::string& exe_path,
                const std::vector<std::string>& args)
      : stdout_pipe(io_context),
        stderr_pipe(io_context),
        stdin_pipe(io_context),
        proc(io_context,
             exe_path,
             args,
             proc::process_stdio{stdin_pipe, stdout_pipe, stderr_pipe}) {}

  /**
   * @brief Construct a new boost process object
   * stdin of the child process is not managed
   *
   * @param io_context
   * @param logger
   * @param cmd_line cmd line split (the first element is the path of the
   * executable)
   * @param  no_stdin (not used)
   */
  boost_process(asio::io_context& io_context,
                const std::string& exe_path,
                const std::vector<std::string>& args,
                bool no_stdin)
      : stdout_pipe(io_context),
        stderr_pipe(io_context),
        stdin_pipe(io_context),
        proc(io_context,
             exe_path,
             args,
             proc::process_stdio{{}, stdout_pipe, stderr_pipe}) {}

#else
  /**
   * @brief Construct a new boost process object
   * stdin of the child process is managed
   *
   * @param io_context
   * @param exe_path  absolute or relative exe path
   * @param args  arguments of the command
   */
  boost_process(asio::io_context& io_context,
                const std::string& exe_path,
                const std::vector<std::string>& args)
      : stdout_pipe(io_context),
        stderr_pipe(io_context),
        stdin_pipe(io_context),
        proc(proc::posix::centreon_posix_default_launcher()(
            io_context.get_executor(),
            exe_path,
            args,
            proc::posix::centreon_process_stdio{stdin_pipe, stdout_pipe,
                                                stderr_pipe})) {}

  /**
   * @brief Construct a new boost process object
   * stdin of the child process is not managed
   *
   * @param io_context
   * @param logger
   * @param cmd_line cmd line split (the first element is the path of the
   * executable)
   * @param  no_stdin (not used)
   */
  boost_process(asio::io_context& io_context,
                const std::string& exe_path,
                const std::vector<std::string>& args,
                bool no_stdin)
      : stdout_pipe(io_context),
        stderr_pipe(io_context),
        stdin_pipe(io_context),
        proc(proc::posix::centreon_posix_default_launcher()(
            io_context,
            exe_path,
            args,
            proc::posix::centreon_process_stdio{{},
                                                stdout_pipe,
                                                stderr_pipe})) {}

#endif

  asio::readable_pipe stdout_pipe;
  asio::readable_pipe stderr_pipe;
  asio::writable_pipe stdin_pipe;
  proc::process proc;
};
}  // namespace com::centreon::common::detail

using namespace com::centreon::common;

/**
 * @brief Construct a new process::process object
 *
 * @param io_context
 * @param logger
 * @param cmd_line cmd line split (the first element is the path of the
 * executable)
 */
template <bool use_mutex>
process<use_mutex>::process(
    const std::shared_ptr<boost::asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    const std::string_view& cmd_line)
    : _io_context(io_context), _logger(logger) {
#ifdef _WIN32
  auto split_res = boost::program_options::split_winmain(std::string(cmd_line));
#else
  auto split_res = boost::program_options::split_unix(std::string(cmd_line));
#endif
  if (split_res.begin() == split_res.end()) {
    SPDLOG_LOGGER_ERROR(_logger, "empty command line:\"{}\"", cmd_line);
    throw exceptions::msg_fmt("empty command line:\"{}\"", cmd_line);
  }
  auto field_iter = split_res.begin();

  _exe_path = *field_iter++;
  for (; field_iter != split_res.end(); ++field_iter) {
    _args.emplace_back(*field_iter);
  }
}

/**
 * @brief returns pid of process, -1 otherwise
 *
 * @tparam use_mutex
 * @return int
 */
template <bool use_mutex>
int process<use_mutex>::get_pid() {
  detail::lock<use_mutex> l(&_protect);
  if (_proc) {
    return _proc->proc.id();
  }
  return -1;
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
void process<use_mutex>::start_process(bool enable_stdin) {
  SPDLOG_LOGGER_DEBUG(_logger, "start process: {}", _exe_path);
  detail::lock<use_mutex> l(&_protect);
  _stdin_write_queue.clear();
  _write_pending = false;

  try {
    _proc = enable_stdin ? std::make_shared<detail::boost_process>(
                               *_io_context, _exe_path, _args)
                         : std::make_shared<detail::boost_process>(
                               *_io_context, _exe_path, _args, false);
    SPDLOG_LOGGER_TRACE(_logger, "process started: {} pid: {}", _exe_path,
                        _proc->proc.id());
    _proc->proc.async_wait(
        [me = shared_from_this(), current = _proc](
            const boost::system::error_code& err, int raw_exit_status) {
          detail::lock<use_mutex> l(&me->_protect);
          if (current != me->_proc) {
            return;
          }
          me->on_process_end(err, raw_exit_status);
        });
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "fail to start {}: {}", _exe_path, e.what());
    throw;
  }
  stdout_read();
  stderr_read();
}

/**
 * @brief called when child process end
 *
 * @param err
 * @param raw_exit_status end status of the process
 */
template <bool use_mutex>
void process<use_mutex>::on_process_end(const boost::system::error_code& err,
                                        int raw_exit_status) {
  if (err) {
    SPDLOG_LOGGER_ERROR(_logger, "fail async_wait of {}: {}", _exe_path,
                        err.message());
    _exit_status = -1;
  } else {
    _exit_status = proc::evaluate_exit_code(raw_exit_status);
    SPDLOG_LOGGER_DEBUG(_logger, "end of process {}, exit_status={}", _exe_path,
                        _exit_status);
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
    SPDLOG_LOGGER_INFO(_logger, "kill process");
    boost::system::error_code err;
    _proc->proc.terminate(err);
    if (err) {
      SPDLOG_LOGGER_INFO(_logger, "fail to kill {}: {}", _exe_path,
                         err.message());
    }
  }
}

/**
 * @brief write some data to child process stdin, if a write is pending, data is
 * pushed to a queue
 *
 * @param data
 */
template <bool use_mutex>
void process<use_mutex>::stdin_write(const std::shared_ptr<std::string>& data) {
  detail::lock<use_mutex> l(&_protect);
  stdin_write_no_lock(data);
}

/**
 * @brief asynchronously write some data to child process stdin, if a write is
 * pending, data is pushed to a queue
 *
 * @param data
 */
template <bool use_mutex>
void process<use_mutex>::stdin_write_no_lock(
    const std::shared_ptr<std::string>& data) {
  if (!_proc) {
    SPDLOG_LOGGER_ERROR(_logger, "stdin_write process {} not started",
                        _exe_path);
    throw exceptions::msg_fmt("stdin_write process {} not started", _exe_path);
  }
  if (_write_pending) {
    _stdin_write_queue.push_back(data);
  } else {
    try {
      _write_pending = true;
      _proc->stdin_pipe.async_write_some(
          asio::buffer(*data),
          [me = shared_from_this(), caller = _proc, data](
              const boost::system::error_code& err, size_t nb_written) {
            detail::lock<use_mutex> l(&me->_protect);
            if (caller != me->_proc) {
              return;
            }
            me->on_stdin_write(err);
          });
    } catch (const std::exception& e) {
      _write_pending = false;
      SPDLOG_LOGGER_ERROR(_logger,
                          "stdin_write process {} fail to write to stdin {}",
                          _exe_path, e.what());
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
void process<use_mutex>::on_stdin_write(const boost::system::error_code& err) {
  _write_pending = false;

  if (err) {
    if (err == asio::error::eof) {
      SPDLOG_LOGGER_DEBUG(_logger,
                          "on_stdin_write process {} fail to write to stdin {}",
                          _exe_path, err.message());
    } else {
      SPDLOG_LOGGER_ERROR(_logger,
                          "on_stdin_write process {} fail to write to stdin {}",
                          _exe_path, err.message());
    }
    return;
  }

  if (!_stdin_write_queue.empty()) {
    std::shared_ptr<std::string> to_send = _stdin_write_queue.front();
    _stdin_write_queue.pop_front();
    stdin_write_no_lock(to_send);
  }
}

/**
 * @brief asynchronous read from child process stdout
 *
 */
template <bool use_mutex>
void process<use_mutex>::stdout_read() {
  if (_proc) {
    try {
      _proc->stdout_pipe.async_read_some(
          asio::buffer(_stdout_read_buffer),
          [me = shared_from_this(), caller = _proc](
              const boost::system::error_code& err, size_t nb_read) {
            detail::lock<use_mutex> l(&me->_protect);
            if (caller != me->_proc) {
              return;
            }
            me->on_stdout_read(err, nb_read);
          });
    } catch (const std::exception& e) {
      asio::post(*_io_context, [me = shared_from_this(), caller = _proc]() {
        detail::lock<use_mutex> l(&me->_protect);
        me->on_stdout_read(std::make_error_code(std::errc::broken_pipe), 0);
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
void process<use_mutex>::on_stdout_read(const boost::system::error_code& err,
                                        size_t nb_read) {
  if (err) {
    if (err == asio::error::eof || err == asio::error::broken_pipe) {
      SPDLOG_LOGGER_DEBUG(_logger, "fail read from stdout of process {}: {}",
                          _exe_path, err.message());
    } else {
      SPDLOG_LOGGER_ERROR(_logger, "fail read from stdout of process {}: {} {}",
                          _exe_path, err.value(), err.message());
    }
    return;
  }
  SPDLOG_LOGGER_TRACE(_logger, " process: {} read from stdout: {}", _exe_path,
                      std::string_view(_stdout_read_buffer, nb_read));
  stdout_read();
}

/**
 * @brief asynchronous read from child process stderr
 *
 */
template <bool use_mutex>
void process<use_mutex>::stderr_read() {
  if (_proc) {
    try {
      _proc->stderr_pipe.async_read_some(
          asio::buffer(_stderr_read_buffer),
          [me = shared_from_this(), caller = _proc](
              const boost::system::error_code& err, size_t nb_read) {
            detail::lock<use_mutex> l(&me->_protect);
            if (caller != me->_proc) {
              return;
            }
            me->on_stderr_read(err, nb_read);
          });
    } catch (const std::exception& e) {
      asio::post(*_io_context, [me = shared_from_this(), caller = _proc]() {
        detail::lock<use_mutex> l(&me->_protect);
        me->on_stderr_read(std::make_error_code(std::errc::broken_pipe), 0);
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
void process<use_mutex>::on_stderr_read(const boost::system::error_code& err,
                                        size_t nb_read) {
  if (err) {
    if (err == asio::error::eof || err == asio::error::broken_pipe) {
      SPDLOG_LOGGER_DEBUG(_logger, "fail read from stderr of process {}: {}",
                          _exe_path, err.message());
    } else {
      SPDLOG_LOGGER_ERROR(_logger, "fail read from stderr of process {}: {} {}",
                          _exe_path, err.value(), err.message());
    }
  } else {
    SPDLOG_LOGGER_TRACE(_logger, " process: {} read from stdout: {}", _exe_path,
                        std::string_view(_stderr_read_buffer, nb_read));
    stderr_read();
  }
}

namespace com::centreon::common {

template class process<true>;

template class process<false>;

}  // namespace com::centreon::common