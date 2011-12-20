/*
** Copyright 2011 Merethis
**
** This file is part of Centreon Connector SSH.
**
** Centreon Connector SSH is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector SSH is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector SSH. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include "com/centreon/connector/ssh/check_result.hh"

using namespace com::centreon::connector::ssh;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Default constructor.
 */
check_result::check_result()
  : _cmd_id(0), _executed(false), _exit_code(-1) {}

/**
 *  Copy constructor.
 *
 *  @param[in] cr Object to copy.
 */
check_result::check_result(check_result const& cr) {
  _internal_copy(cr);
}

/**
 *  Destructor.
 */
check_result::~check_result() {}

/**
 *  Assignment operator.
 *
 *  @param[in] cr Object to copy.
 *
 *  @return This object.
 */
check_result& check_result::operator=(check_result const& cr) {
  if (this != &cr)
    _internal_copy(cr);
  return (*this);
}

/**
 *  Get the command ID.
 *
 *  @return Command ID.
 */
unsigned long long check_result::get_command_id() const throw () {
  return (_cmd_id);
}

/**
 *  Get the check error string.
 *
 *  @return Check error string.
 */
std::string const& check_result::get_error() const throw () {
  return (_error);
}

/**
 *  Get the executed flag.
 *
 *  @return true if check was executed, false otherwise.
 */
bool check_result::get_executed() const throw () {
  return (_executed);
}

/**
 *  Get the exit code.
 *
 *  @return Check exit code.
 */
int check_result::get_exit_code() const throw () {
  return (_exit_code);
}

/**
 *  Get the check output.
 *
 *  @return Check output.
 */
std::string const& check_result::get_output() const throw () {
  return (_output);
}

/**
 *  Set the command ID.
 *
 *  @param[in] cmd_id Command ID.
 */
void check_result::set_command_id(unsigned long long cmd_id) throw () {
  _cmd_id = cmd_id;
  return ;
}

/**
 *  Set the error string.
 *
 *  @param[in] error Error string.
 */
void check_result::set_error(std::string const& error) {
  _error = error;
  return ;
}

/**
 *  Set the executed flag.
 *
 *  @param[in] executed Set to true if check was executed, false
 *                      otherwise.
 */
void check_result::set_executed(bool executed) throw () {
  _executed = executed;
  return ;
}

/**
 *  Set the exit code.
 *
 *  @param[in] code Check exit code.
 */
void check_result::set_exit_code(int code) throw () {
  _exit_code = code;
  return ;
}

/**
 *  Set the check output.
 *
 *  @param[in] output Check output.
 */
void check_result::set_output(std::string const& output) {
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
 *  @param[in] cr Object to copy.
 */
void check_result::_internal_copy(check_result const& cr) {
  _cmd_id = cr._cmd_id;
  _error = cr._error;
  _executed = cr._executed;
  _exit_code = cr._exit_code;
  _output = cr._output;
  return ;
}
