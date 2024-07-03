/*
 * Copyright 2024 Centreon
 *
 * This file is part of Centreon Engine.
 *
 * Centreon Engine is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * Centreon Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Centreon Engine. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <boost/process/v2/stdio.hpp>
#include <boost/program_options/parsers.hpp>

#include "com/centreon/common/process/process.hh"

#include "com/centreon/common/process/detail/centreon_posix_process_launcher.hh"

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
  boost_process(asio::io_context& io_context,
                const std::string& exe_path,
                const std::vector<std::string>& args)
      : stdout(io_context),
        stderr(io_context),
        stdin(io_context),
        proc(io_context,
             exe_path,
             args,
             proc::process_stdio{stdin, stdout, stderr}) {}
#else
  boost_process(asio::io_context& io_context,
                const std::string& exe_path,
                const std::vector<std::string>& args)
      : stdout(io_context),
        stderr(io_context),
        stdin(io_context),
        proc(proc::posix::centreon_posix_default_launcher()
             /*proc::default_process_launcher()*/ (
                 io_context.get_executor(),
                 exe_path,
                 args,
                 proc::posix::centreon_process_stdio{stdin, stdout, stderr})) {}
#endif

  asio::readable_pipe stdout;
  asio::readable_pipe stderr;
  asio::writable_pipe stdin;
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
process::process(const std::shared_ptr<boost::asio::io_context>& io_context,
                 const std::shared_ptr<spdlog::logger>& logger,
                 const std::string_view& cmd_line)
    : _io_context(io_context), _logger(logger) {
  auto split_res = boost::program_options::split_unix(std::string(cmd_line));
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
 * @brief start a new process, if a previous one is running, it's killed
 * In this function, we start child process and stdout, stderr asynchronous read
 * we also start an asynchronous read on process fd to be aware of child process
 * termination
 */
void process::start_process() {
  SPDLOG_LOGGER_DEBUG(_logger, "start process: {}", _exe_path);
  absl::MutexLock l(&_protect);
  _stdin_write_queue.clear();
  _write_pending = false;

  try {
    _proc =
        std::make_shared<detail::boost_process>(*_io_context, _exe_path, _args);
    SPDLOG_LOGGER_TRACE(_logger, "process started: {} pid: {}", _exe_path,
                        _proc->proc.id());
    _proc->proc.async_wait(
        [me = shared_from_this(), current = _proc](
            const boost::system::error_code& err, int raw_exit_status) {
          absl::MutexLock l(&me->_protect);
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
void process::on_process_end(const boost::system::error_code& err,
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
void process::kill() {
  absl::MutexLock l(&_protect);
  if (_proc) {
    boost::system::error_code err;
    _proc->proc.terminate(err);
    _proc.reset();
  }
}

/**
 * @brief write some data to child process stdin, if a write is pending, data is
 * pushed to a queue
 *
 * @param data
 */
void process::stdin_write(const std::shared_ptr<std::string>& data) {
  absl::MutexLock l(&_protect);
  stdin_write_no_lock(data);
}

/**
 * @brief asynchronously write some data to child process stdin, if a write is
 * pending, data is pushed to a queue
 *
 * @param data
 */
void process::stdin_write_no_lock(const std::shared_ptr<std::string>& data) {
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
      _proc->stdin.async_write_some(
          asio::buffer(*data),
          [me = shared_from_this(), caller = _proc, data](
              const boost::system::error_code& err, size_t nb_written) {
            absl::MutexLock l(&me->_protect);
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
void process::on_stdin_write(const boost::system::error_code& err) {
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
void process::stdout_read() {
  if (_proc) {
    try {
      _proc->stdout.async_read_some(
          asio::buffer(_stdout_read_buffer),
          [me = shared_from_this(), caller = _proc](
              const boost::system::error_code& err, size_t nb_read) {
            absl::MutexLock l(&me->_protect);
            if (caller != me->_proc) {
              return;
            }
            me->on_stdout_read(err, nb_read);
          });
    } catch (const std::exception& e) {
      _io_context->post([me = shared_from_this(), caller = _proc]() {
        absl::MutexLock l(&me->_protect);
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
void process::on_stdout_read(const boost::system::error_code& err,
                             size_t nb_read) {
  if (err) {
    if (err == asio::error::eof) {
      SPDLOG_LOGGER_DEBUG(_logger, "fail read from stdout of process {}: {}",
                          _exe_path, err.message());
    } else {
      SPDLOG_LOGGER_ERROR(_logger, "fail read from stdout of process {}: {}",
                          _exe_path, err.message());
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
void process::stderr_read() {
  if (_proc) {
    try {
      _proc->stderr.async_read_some(
          asio::buffer(_stderr_read_buffer),
          [me = shared_from_this(), caller = _proc](
              const boost::system::error_code& err, size_t nb_read) {
            absl::MutexLock l(&me->_protect);
            if (caller != me->_proc) {
              return;
            }
            me->on_stderr_read(err, nb_read);
          });
    } catch (const std::exception& e) {
      _io_context->post([me = shared_from_this(), caller = _proc]() {
        absl::MutexLock l(&me->_protect);
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
void process::on_stderr_read(const boost::system::error_code& err,
                             size_t nb_read) {
  if (err) {
    if (err == asio::error::eof) {
      SPDLOG_LOGGER_DEBUG(_logger, "fail read from stderr of process {}: {}",
                          _exe_path, err.message());
    } else {
      SPDLOG_LOGGER_ERROR(_logger, "fail read from stderr of process {}: {}",
                          _exe_path, err.message());
    }
  } else {
    SPDLOG_LOGGER_TRACE(_logger, " process: {} read from stdout: {}", _exe_path,
                        std::string_view(_stderr_read_buffer, nb_read));
    stderr_read();
  }
}
