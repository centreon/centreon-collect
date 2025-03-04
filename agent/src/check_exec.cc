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

#include "check_exec.hh"

using namespace com::centreon::agent;

/**
 * @brief Construct a new detail::process::process object
 *
 * @param io_context
 * @param logger
 * @param cmd_line
 * @param parent
 */
detail::process::process(const std::shared_ptr<asio::io_context>& io_context,
                         const std::shared_ptr<spdlog::logger>& logger,
                         const std::string& cmd_line,
                         const std::shared_ptr<check_exec>& parent)
    : common::process<false>(io_context, logger, cmd_line), _parent(parent) {}

/**
 * @brief start a new process, if a previous one is already running, it's killed
 *
 * @param running_index
 */
void detail::process::start(unsigned running_index) {
  _process_ended = false;
  _stdout_eof = false;
  _running_index = running_index;
  _stdout.clear();
  common::process<false>::start_process(false);
}

/**
 * @brief son process stdout read handler
 *
 * @param err
 * @param nb_read
 */
void detail::process::on_stdout_read(const boost::system::error_code& err,
                                     size_t nb_read) {
  if (!err && nb_read > 0) {
    _stdout.append(_stdout_read_buffer, nb_read);
  } else if (err) {
    _stdout_eof = true;
    _on_completion();
  }
  common::process<false>::on_stdout_read(err, nb_read);
}

/**
 * @brief son process stderr read handler
 *
 * @param err
 * @param nb_read
 */
void detail::process::on_stderr_read(const boost::system::error_code& err,
                                     size_t nb_read) {
  if (!err) {
    SPDLOG_LOGGER_ERROR(_logger, "process error: {}",
                        std::string_view(_stderr_read_buffer, nb_read));
  }
  common::process<false>::on_stderr_read(err, nb_read);
}

/**
 * @brief called when son process ends
 *
 * @param err
 * @param raw_exit_status
 */
void detail::process::on_process_end(const boost::system::error_code& err,
                                     int raw_exit_status) {
  if (err) {
    _stdout += fmt::format("fail to execute process {} : {}", get_exe_path(),
                           err.message());
  }
  common::process<false>::on_process_end(err, raw_exit_status);
  _process_ended = true;
  _on_completion();
}

/**
 * @brief if both stdout read and process are terminated, we call
 * check_exec::on_completion
 *
 */
void detail::process::_on_completion() {
  if (_stdout_eof && _process_ended) {
    std::shared_ptr<check_exec> parent = _parent.lock();
    if (parent) {
      parent->on_completion(_running_index);
    }
  }
}

/******************************************************************
 * check_exec
 ******************************************************************/

check_exec::check_exec(const std::shared_ptr<asio::io_context>& io_context,
                       const std::shared_ptr<spdlog::logger>& logger,
                       time_point first_start_expected,
                       duration check_interval,
                       const std::string& serv,
                       const std::string& cmd_name,
                       const std::string& cmd_line,
                       const engine_to_agent_request_ptr& cnf,
                       check::completion_handler&& handler,
                       const checks_statistics::pointer& stat)
    : check(io_context,
            logger,
            first_start_expected,
            check_interval,
            serv,
            cmd_name,
            cmd_line,
            cnf,
            std::move(handler),
            stat) {}

/**
 * @brief create and initialize a check_exec object (don't use constructor)
 *
 * @tparam handler_type
 * @param io_context
 * @param logger
 * @param first_start_expected start expected
 * @param check_interval check interval between two checks (not only this but
 * also others)
 * @param serv
 * @param cmd_name
 * @param cmd_line
 * @param cnf   agent configuration
 * @param handler  completion handler
 * @return std::shared_ptr<check_exec>
 */
std::shared_ptr<check_exec> check_exec::load(
    const std::shared_ptr<asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    time_point first_start_expected,
    duration check_interval,
    const std::string& serv,
    const std::string& cmd_name,
    const std::string& cmd_line,
    const engine_to_agent_request_ptr& cnf,
    check::completion_handler&& handler,
    const checks_statistics::pointer& stat) {
  std::shared_ptr<check_exec> ret = std::make_shared<check_exec>(
      io_context, logger, first_start_expected, check_interval, serv, cmd_name,
      cmd_line, cnf, std::move(handler), stat);
  ret->_init();
  return ret;
}

/**
 * @brief to call after construction
 * constructor mustn't be called, use check_exec::load instead
 *
 */
void check_exec::_init() {
  try {
    _process = std::make_shared<detail::process>(
        _io_context, _logger, get_command_line(),
        std::static_pointer_cast<check_exec>(shared_from_this()));
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "fail to create process of cmd_line '{}' : {}",
                        get_command_line(), e.what());
    throw;
  }
}

/**
 * @brief start a check, completion handler is always called asynchronously even
 * in case of failure
 *
 * @param timeout
 */
void check_exec::start_check(const duration& timeout) {
  if (!check::_start_check(timeout)) {
    return;
  }
  if (!_process) {
    _io_context->post([me = check::shared_from_this(),
                       start_check_index = _get_running_check_index()]() {
      me->on_completion(start_check_index, e_status::unknown,
                        std::list<com::centreon::common::perfdata>(),
                        {"empty command"});
    });
  }

  try {
    _process->start(_get_running_check_index());
  } catch (const boost::system::system_error& e) {
    SPDLOG_LOGGER_ERROR(_logger, " serv {} fail to execute {}: {}",
                        get_service(), get_command_line(), e.code().message());
    _io_context->post([me = check::shared_from_this(),
                       start_check_index = _get_running_check_index(), e]() {
      me->on_completion(
          start_check_index, e_status::unknown,
          std::list<com::centreon::common::perfdata>(),
          {fmt::format("Fail to execute {} : {}", me->get_command_line(),
                       e.code().message())});
    });
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, " serv {} fail to execute {}: {}",
                        get_service(), get_command_line(), e.what());
    _io_context->post([me = check::shared_from_this(),
                       start_check_index = _get_running_check_index(), e]() {
      me->on_completion(start_check_index, e_status::unknown,
                        std::list<com::centreon::common::perfdata>(),
                        {fmt::format("Fail to execute {} : {}",
                                     me->get_command_line(), e.what())});
    });
  }
}

/**
 * @brief get process id of the check (only used by tests)
 *
 * @return int
 */
int check_exec::get_pid() const {
  if (!_process) {
    return 0;
  }
  return _process->get_pid();
}
/**
 * @brief process is killed in case of timeout and handler is called
 *
 * @param err
 * @param start_check_index
 */
void check_exec::_on_timeout() {
  _process->kill();
}

/**
 * @brief called on process completion
 *
 * @param running_index
 */
void check_exec::on_completion(unsigned running_index) {
  if (running_index != _get_running_check_index()) {
    SPDLOG_LOGGER_ERROR(_logger, "running_index={}, running_index={}",
                        running_index, _get_running_check_index());
    return;
  }

  std::list<std::string> outputs;
  std::list<com::centreon::common::perfdata> perfs;

  // split multi line output
  outputs = absl::StrSplit(_process->get_stdout(), '\n', absl::SkipEmpty());
  if (!outputs.empty()) {
    const std::string& first_line = *outputs.begin();
    size_t pipe_pos = first_line.find('|');
    if (pipe_pos != std::string::npos) {
      std::string perfdatas = outputs.begin()->substr(pipe_pos + 1);
      boost::trim(perfdatas);
      perfs = com::centreon::common::perfdata::parse_perfdata(
          0, 0, perfdatas.c_str(), _logger);
    }
  }
  check::on_completion(running_index, _process->get_exit_status(), perfs,
                       outputs);
}

/******************************************************************
 * check_dummy
 ******************************************************************/

check_dummy::check_dummy(const std::shared_ptr<asio::io_context>& io_context,
                         const std::shared_ptr<spdlog::logger>& logger,
                         time_point first_start_expected,
                         duration check_interval,
                         const std::string& serv,
                         const std::string& cmd_name,
                         const std::string& cmd_line,
                         const std::string& output,
                         const engine_to_agent_request_ptr& cnf,
                         check::completion_handler&& handler,
                         const checks_statistics::pointer& stat)
    : check(io_context,
            logger,
            first_start_expected,
            check_interval,
            serv,
            cmd_name,
            cmd_line,
            cnf,
            std::move(handler),
            stat),
      _output(output) {}

/**
 * @brief create and initialize a check_dummy object (don't use constructor)
 *
 * @tparam handler_type
 * @param io_context
 * @param logger
 * @param first_start_expected start expected
 * @param check_interval check interval between two checks (not only this but
 * also others)
 * @param serv
 * @param cmd_name
 * @param cmd_line
 * @param cnf   agent configuration
 * @param handler  completion handler
 * @return std::shared_ptr<check_dummy>
 */
std::shared_ptr<check_dummy> check_dummy::load(
    const std::shared_ptr<asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    time_point first_start_expected,
    duration check_interval,
    const std::string& serv,
    const std::string& cmd_name,
    const std::string& cmd_line,
    const std::string& output,
    const engine_to_agent_request_ptr& cnf,
    check::completion_handler&& handler,
    const checks_statistics::pointer& stat) {
  std::shared_ptr<check_dummy> ret = std::make_shared<check_dummy>(
      io_context, logger, first_start_expected, check_interval, serv, cmd_name,
      cmd_line, output, cnf, std::move(handler), stat);
  return ret;
}

/**
 * @brief start a check, completion handler is always called asynchronously even
 * in case of failure
 *
 * @param timeout
 */
void check_dummy::start_check(const duration& timeout) {
  if (!check::_start_check(timeout)) {
    return;
  }

  _io_context->post([me = check::shared_from_this(),
                     start_check_index = _get_running_check_index(), this]() {
    me->on_completion(
        start_check_index, e_status::critical,
        std::list<com::centreon::common::perfdata>(),
        {fmt::format("unable to execute native check {} , output error : {}",
                     me->get_command_line(), get_output())});
  });
}
