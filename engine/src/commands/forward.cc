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

#include "com/centreon/engine/commands/forward.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/version.hh"

using namespace com::centreon::engine::logging;
using namespace com::centreon::engine::commands;

/**
 *  Constructor.
 *
 *  @param[in] command_name  The command name.
 *  @param[in] command_line  The command command line.
 *  @param[in] command       The command to forward command.
 */
forward::forward(std::string const& command_name,
                 std::string const& command_line,
                 const std::shared_ptr<command>& cmd)
    : command(command_name, command_line, nullptr, e_type::forward),
      _s_command(cmd),
      _command(cmd.get()) {
  if (_name.empty())
    throw engine_error() << "Could not create a command with an empty name";
  if (_command_line.empty())
    throw engine_error() << "Could not create '" << _name
                         << "' command: command line is empty";
}

/**
 *  Run a command.
 *
 *  @param[in] args    The command arguments.
 *  @param[in] macros  The macros data struct.
 *  @param[in] timeout The command timeout.
 *  @param[in] to_push_to_checker This check_result will be pushed to checher.
 *  @param[in] caller  pointer to the caller
 *
 *  @return The command id.
 */
uint64_t forward::run(std::string const& processed_cmd,
                      nagios_macros& macros,
                      uint32_t timeout,
                      const check_result::pointer& to_push_to_checker,
                      const void* caller) {
  return _command->run(processed_cmd, macros, timeout, to_push_to_checker,
                       caller);
}

/**
 *  Run a command and wait the result.
 *
 *  @param[in]  args    The command arguments.
 *  @param[in]  macros  The macros data struct.
 *  @param[in]  timeout The command timeout.
 *  @param[out] res     The result of the command.
 */
void forward::run(std::string const& processed_cmd,
                  nagios_macros& macros,
                  uint32_t timeout,
                  result& res) {
  _command->run(processed_cmd, macros, timeout, res);
}

/**
 * @brief notify a command of host service owner
 *
 * @param host
 * @param service_description
 */
void forward::register_host_serv(const std::string& host,
                                 const std::string& service_description) {
  _command->register_host_serv(host, service_description);
}

/**
 * @brief notify a command that a service is not using it anymore
 *
 * @param host
 * @param service_description
 */
void forward::unregister_host_serv(const std::string& host,
                                   const std::string& service_description) {
  _command->unregister_host_serv(host, service_description);
}
