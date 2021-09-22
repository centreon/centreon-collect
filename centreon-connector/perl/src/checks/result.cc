/*
** Copyright 2011-2013 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#include "com/centreon/connector/perl/checks/result.hh"

using namespace com::centreon::connector::perl::checks;

/**************************************
 *                                     *
 *           Public Methods            *
 *                                     *
 **************************************/

/**
 *  Default constructor.
 */
result::result() : _cmd_id(0), _executed(false), _exit_code(-1) {}

/**
 *  Get the command ID.
 *
 *  @return Command ID.
 */
uint64_t result::get_command_id() const noexcept {
  return (_cmd_id);
}

/**
 *  Get the check error string.
 *
 *  @return Check error string.
 */
std::string const& result::get_error() const noexcept {
  return (_error);
}

/**
 *  Get the executed flag.
 *
 *  @return true if check was executed, false otherwise.
 */
bool result::get_executed() const noexcept {
  return _executed;
}

/**
 *  Get the exit code.
 *
 *  @return Check exit code.
 */
int result::get_exit_code() const noexcept {
  return _exit_code;
}

/**
 *  Get the check output.
 *
 *  @return Check output.
 */
std::string const& result::get_output() const noexcept {
  return (_output);
}

/**
 *  Set the command ID.
 *
 *  @param[in] cmd_id Command ID.
 */
void result::set_command_id(uint64_t cmd_id) noexcept {
  _cmd_id = cmd_id;
}

/**
 *  Set the error string.
 *
 *  @param[in] error Error string.
 */
void result::set_error(std::string const& error) {
  _error = error;
}

/**
 *  Set the executed flag.
 *
 *  @param[in] executed Set to true if check was executed, false
 *                      otherwise.
 */
void result::set_executed(bool executed) noexcept {
  _executed = executed;
}

/**
 *  Set the exit code.
 *
 *  @param[in] code Check exit code.
 */
void result::set_exit_code(int code) noexcept {
  _exit_code = code;
}

/**
 *  Set the check output.
 *
 *  @param[in] output Check output.
 */
void result::set_output(std::string const& output) {
  _output = output;
}
