/**
 * Copyright 2011-2013 Merethis
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
    : command(command_name, command_line, nullptr), _command(cmd) {
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