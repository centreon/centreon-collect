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

#include "check.hh"

using namespace com::centreon::agent;

/**
 * @brief Construct a new check::check object
 *
 * @param io_context
 * @param logger
 * @param exp
 * @param serv
 * @param command_name
 * @param cmd_line
 * @param cnf
 * @param handler
 */
check::check(const std::shared_ptr<asio::io_context>& io_context,
             const std::shared_ptr<spdlog::logger>& logger,
             time_point exp,
             const std::string& serv,
             const std::string& command_name,
             const std::string& cmd_line,
             const engine_to_agent_request_ptr& cnf,
             completion_handler&& handler)
    : _start_expected(exp),
      _service(serv),
      _command_name(command_name),
      _command_line(cmd_line),
      _conf(cnf),
      _io_context(io_context),
      _logger(logger),
      _time_out_timer(*io_context),
      _completion_handler(handler) {}

/**
 * @brief scheduler uses this method to increase start_expected
 *
 * @param to_add
 */
void check::add_duration_to_start_expected(const duration& to_add) {
  _start_expected += to_add;
}

/**
 * @brief start a asynchronous check
 *
 * @param timeout
 */
void check::start_check(const duration& timeout) {
  if (_running_check) {
    SPDLOG_LOGGER_ERROR(_logger, "check for service {} is already running",
                        _service);
    _io_context->post(
        [me = shared_from_this(), to_call = _completion_handler]() {
          to_call(me, 3, std::list<com::centreon::common::perfdata>(),
                  {"a check is already running"});
        });
    return;
  }
  // we refresh start expected in order that next call will occur at now + check
  // period
  _start_expected = std::chrono::system_clock::now();
  _running_check = true;
  _start_timeout_timer(timeout);
  SPDLOG_LOGGER_TRACE(_logger, "start check for service {}", _service);
}

/**
 * @brief start check timeout timer
 *
 * @param timeout
 */
void check::_start_timeout_timer(const duration& timeout) {
  _time_out_timer.expires_from_now(timeout);
  _time_out_timer.async_wait(
      [me = shared_from_this(), start_check_index = _running_check_index](
          const boost::system::error_code& err) {
        me->_timeout_timer_handler(err, start_check_index);
      });
}

/**
 * @brief timeout timer handler
 *
 * @param err
 * @param start_check_index
 */
void check::_timeout_timer_handler(const boost::system::error_code& err,
                                   unsigned start_check_index) {
  if (err) {
    return;
  }
  if (start_check_index == _running_check_index) {
    SPDLOG_LOGGER_ERROR(_logger, "check timeout for service {} cmd: {}",
                        _service, _command_name);
    on_completion(start_check_index, 3 /*unknown*/,
                  std::list<com::centreon::common::perfdata>(),
                  {"Timeout at execution of " + _command_line});
  }
}

/**
 * @brief called when check is ended
 * _running_check is increased so that next check will be identified by this new
 * id. We also cancel timeout timer
 *
 * @param start_check_index
 * @param status
 * @param perfdata
 * @param outputs
 */
void check::on_completion(
    unsigned start_check_index,
    unsigned status,
    const std::list<com::centreon::common::perfdata>& perfdata,
    const std::list<std::string>& outputs) {
  if (start_check_index == _running_check_index) {
    SPDLOG_LOGGER_TRACE(_logger, "end check for service {} cmd: {}", _service,
                        _command_name);
    _time_out_timer.cancel();
    _running_check = false;
    ++_running_check_index;
    _completion_handler(shared_from_this(), status, perfdata, outputs);
  }
}
