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

#ifndef CENTREON_AGENT_CHECK_EXEC_HH
#define CENTREON_AGENT_CHECK_EXEC_HH

#include "check.hh"
#include "com/centreon/common/process/process.hh"
#include "com/centreon/common/process/process_args.hh"

namespace com::centreon::agent {

/**
 * @brief check that executes a process (plugins)
 *
 */
class check_exec : public check {
  common::process_args::pointer _process_args;
  int _pid;

 protected:
  using check::completion_handler;

 public:
  check_exec(const std::shared_ptr<asio::io_context>& io_context,
             const std::shared_ptr<spdlog::logger>& logger,
             time_point exp,
             const std::string& serv,
             const std::string& cmd_name,
             const std::string& cmd_line,
             const engine_to_agent_request_ptr& cnf,
             check::completion_handler&& handler);

  static std::shared_ptr<check_exec> load(
      const std::shared_ptr<asio::io_context>& io_context,
      const std::shared_ptr<spdlog::logger>& logger,
      time_point exp,
      const std::string& serv,
      const std::string& cmd_name,
      const std::string& cmd_line,
      const engine_to_agent_request_ptr& cnf,
      check::completion_handler&& handler);

  void start_check(const duration& timeout) override;

  void on_completion(unsigned running_index,
                     int exit_code,
                     int exit_status,
                     const std::string&);

  int get_pid() const { return _pid; }
};

}  // namespace com::centreon::agent

#endif
