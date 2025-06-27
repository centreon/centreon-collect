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
#include "com/centreon/common/process/process.hh"

using namespace com::centreon::agent;

/******************************************************************
 * check_exec
 ******************************************************************/

check_exec::check_exec(const std::shared_ptr<asio::io_context>& io_context,
                       const std::shared_ptr<spdlog::logger>& logger,
                       time_point exp,
                       const std::string& serv,
                       const std::string& cmd_name,
                       const std::string& cmd_line,
                       const engine_to_agent_request_ptr& cnf,
                       check::completion_handler&& handler)
    : check(io_context,
            logger,
            exp,
            serv,
            cmd_name,
            cmd_line,
            cnf,
            std::move(handler)) {
  _process_args =
      com::centreon::common::process<false>::parse_cmd_line(cmd_line);
}

/**
 * @brief create and initialize a check_exec object (don't use
 * constructor)
 *
 * @tparam handler_type
 * @param io_context
 * @param logger
 * @param exp start expected
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
    time_point exp,
    const std::string& serv,
    const std::string& cmd_name,
    const std::string& cmd_line,
    const engine_to_agent_request_ptr& cnf,
    check::completion_handler&& handler) {
  std::shared_ptr<check_exec> ret =
      std::make_shared<check_exec>(io_context, logger, exp, serv, cmd_name,
                                   cmd_line, cnf, std::move(handler));
  return ret;
}

/**
 * @brief start a check, completion handler is always called asynchronously even
 * in case of failure
 *
 * @param timeout
 */
void check_exec::start_check(const duration& timeout) {
  check::start_check(timeout);

  try {
    auto proc = std::make_shared<com::centreon::common::process<false>>(
        _io_context, _logger, _process_args, true, false, nullptr);
    // we add 100ms to time out in order to let check class manage timeout
    proc->start_process(
        [me = std::static_pointer_cast<check_exec>(shared_from_this()),
         running_index = _get_running_check_index()](
            const com::centreon::common::process<false>&, int exit_code,
            int exit_status, const std::string& std_out, const std::string&) {
          me->on_completion(running_index, exit_code, exit_status, std_out);
        },
        timeout + std::chrono::milliseconds(100));
    _pid = proc->get_pid();
  } catch (const boost::system::system_error& e) {
    SPDLOG_LOGGER_ERROR(_logger, " serv {} fail to execute {}: {}",
                        get_service(), get_command_line(), e.code().message());
    _io_context->post([me = check::shared_from_this(),
                       start_check_index = _get_running_check_index(), e]() {
      me->on_completion(
          start_check_index, 3, std::list<com::centreon::common::perfdata>(),
          {fmt::format("Fail to execute {} : {}", me->get_command_line(),
                       e.code().message())});
    });
  } catch (const std::exception& e) {
    std::string output =
        fmt::format("Fail to execute {} : {}", get_command_line(), e.what());
    SPDLOG_LOGGER_ERROR(_logger, " serv {} {}", get_service(), output);
    asio::post(*_io_context, [me = check::shared_from_this(),
                              start_check_index = _get_running_check_index(),
                              output]() {
      me->on_completion(start_check_index, e_status::unknown,
                        std::list<com::centreon::common::perfdata>(), {output});
    });
  }
}

/**
 * @brief called on process completion
 *
 * @param running_index
 */
void check_exec::on_completion(unsigned running_index,
                               int exit_code,
                               int exit_status,
                               const std::string& std_out) {
  if (running_index != _get_running_check_index()) {
    SPDLOG_LOGGER_ERROR(_logger, "running_index={}, running_index={}",
                        running_index, _get_running_check_index());
    return;
  }

  std::list<std::string> outputs;
  std::list<com::centreon::common::perfdata> perfs;

  // split multi line output
  outputs = absl::StrSplit(std_out, absl::ByAnyChar("\r\n"), absl::SkipEmpty());
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

  check::on_completion(running_index, exit_code, perfs, outputs);
}
