/*
** Copyright 2011 Merethis
**
** This file is part of Centreon Connector ICMP.
**
** Centreon Connector ICMP is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector ICMP is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector ICMP. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include "com/centreon/connector/icmp/check_dispatch.hh"
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/connector/icmp/cmd_execute.hh"

using namespace com::centreon::connector::icmp;

/**
 *  Default constructor.
 */
cmd_execute::cmd_execute(unsigned int max_concurrent_checks)
  : check_observer(),
    _max_concurrent_checks(max_concurrent_checks),
    _status(0) {
  if (!_max_concurrent_checks)
    throw (basic_error() << "cmd_execute constructor failed: "
           "null argument");
}

/**
 *  Default copy consttructor.
 */
cmd_execute::cmd_execute(cmd_execute const& right)
  : check_observer() {
  _internal_copy(right);
}

/**
 *  Default destructor.
 */
cmd_execute::~cmd_execute() throw () {

}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
cmd_execute& cmd_execute::operator=(cmd_execute const& right) {
  return (_internal_copy(right));
}

/**
 *  Execute command line, wait the result and print it on the
 *  standard output.
 *
 *  @param[in] command_line  The command line to execute.
 */
void cmd_execute::execute(std::string const& command_line) {
  check_dispatch cd(this);
  cd.set_max_concurrent_checks(_max_concurrent_checks);
  cd.submit(command_line);
}

/**
 *  Get the result message.
 *
 *  @return The message.
 */
std::string const& cmd_execute::get_message() const throw () {
  return (_message);
}

/**
 *  Get the status message.
 *
 *  @return The status message
 */
int cmd_execute::get_status() const throw () {
  return (_status);
}

/**
 *  Recv result of one check.
 *
 *  @param[in] command_id  none.
 *  @param[in] status      Status of check result.
 *  @param[in] msg         Message of check result.
 */
void cmd_execute::emit_check_result(
                    unsigned int command_id,
                    unsigned int status,
                    std::string const& msg) {
  (void)command_id;
  _message = msg;
  _status = status;
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
cmd_execute& cmd_execute::_internal_copy(cmd_execute const& right) {
  if (this != &right) {
    _message = right._message;
    _status = right._status;
  }
  return (*this);
}
