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

#ifndef CCCS_CHECK_RESULT_HH
#  define CCCS_CHECK_RESULT_HH

#  include <string>
#  include "com/centreon/connector/ssh/namespace.hh"

CCCS_BEGIN()

/**
 *  @class check_result check_result.hh "com/centreon/connector/ssh/check_result.hh"
 *  @brief Check result.
 *
 *  Store check result.
 */
class                check_result {
public:
                     check_result();
                     check_result(check_result const& cr);
                     ~check_result();
  check_result&      operator=(check_result const& cr);
  unsigned long long get_command_id() const throw ();
  std::string const& get_error() const throw ();
  bool               get_executed() const throw ();
  int                get_exit_code() const throw ();
  std::string const& get_output() const throw ();
  void               set_command_id(unsigned long long cmd_id) throw ();
  void               set_error(std::string const& error);
  void               set_executed(bool executed) throw ();
  void               set_exit_code(int code) throw ();
  void               set_output(std::string const& output);

private:
  void               _internal_copy(check_result const& cr);

  unsigned long long _cmd_id;
  std::string        _error;
  bool               _executed;
  int                _exit_code;
  std::string        _output;
};

CCCS_END()

#endif // !CCCS_CHECK_RESULT_HH
