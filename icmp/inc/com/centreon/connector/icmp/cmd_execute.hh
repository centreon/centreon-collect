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

#ifndef CCC_ICMP_CMD_EXECUTE_HH
#  define CCC_ICMP_CMD_EXECUTE_HH

#  include <string>
#  include "com/centreon/connector/icmp/check_observer.hh"

CCC_ICMP_BEGIN()

/**
 *  @class cmd_execute cmd_execute.hh "com/centreon/connector/icmp/cmd_execute.hh"
 *  @brief Execute command line wait the result and print it on the
 *         standard output.
 */
class                cmd_execute : private check_observer {
public:
                     cmd_execute(unsigned int max_concurrent_checks);
                     cmd_execute(cmd_execute const& right);
                     ~cmd_execute() throw ();
  cmd_execute&       operator=(cmd_execute const& right);
  void               execute(std::string const& command_line);
  std::string const& get_message() const throw ();
  int                get_status() const throw ();

private:
  void               emit_check_result(
                       unsigned int command_id,
                       unsigned int status,
                       std::string const& msg);
  cmd_execute&       _internal_copy(cmd_execute const& right);

  unsigned int       _max_concurrent_checks;
  std::string        _message;
  unsigned int       _status;
};

CCC_ICMP_END()

#endif // !CCC_ICMP_CMD_EXECUTE_HH
