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
 *  Copy constructor.
 *
 *  @param[in] r Object to copy.
 */
result::result(result const& r) {
  _internal_copy(r);
}

/**
 *  Destructor.
 */
result::~result() {}

/**
 *  Assignment operator.
 *
 *  @param[in] r Object to copy.
 *
 *  @return This object.
 */
result& result::operator=(result const& r) {
  if (this != &r)
    _internal_copy(r);
  return (*this);
}

/**
 *  Get the command ID.
 *
 *  @return Command ID.
 */
unsigned long long result::get_command_id() const throw () {
  return (_cmd_id);
}

/**
 *  Get the check error string.
 *
 *  @return Check error string.
 */
std::string const& result::get_error() const throw () {
  return (_error);
}

/**
 *  Get the executed flag.
 *
 *  @return true if check was executed, false otherwise.
 */
bool result::get_executed() const throw () {
  return (_executed);
}

/**
 *  Get the exit code.
 *
 *  @return Check exit code.
 */
int result::get_exit_code() const throw () {
  return (_exit_code);
}

/**
 *  Get the check output.
 *
 *  @return Check output.
 */
std::string const& result::get_output() const throw () {
  return (_output);
}

/**
 *  Set the command ID.
 *
 *  @param[in] cmd_id Command ID.
 */
void result::set_command_id(unsigned long long cmd_id) throw () {
  _cmd_id = cmd_id;
  return ;
}

/**
 *  Set the error string.
 *
 *  @param[in] error Error string.
 */
void result::set_error(std::string const& error) {
  _error = error;
  return ;
}

/**
 *  Set the executed flag.
 *
 *  @param[in] executed Set to true if check was executed, false
 *                      otherwise.
 */
void result::set_executed(bool executed) throw () {
  _executed = executed;
  return ;
}

/**
 *  Set the exit code.
 *
 *  @param[in] code Check exit code.
 */
void result::set_exit_code(int code) throw () {
  _exit_code = code;
  return ;
}

/**
 *  Set the check output.
 *
 *  @param[in] output Check output.
 */
void result::set_output(std::string const& output) {
  _output = output;
  return ;
}

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Copy internal data members.
 *
 *  @param[in] r Object to copy.
 */
void result::_internal_copy(result const& r) {
  _cmd_id = r._cmd_id;
  _error = r._error;
  _executed = r._executed;
  _exit_code = r._exit_code;
  _output = r._output;
  return ;
}
