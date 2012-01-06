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

#ifndef CCC_ICMP_CMD_OPTIONS_HH
#  define CCC_ICMP_CMD_OPTIONS_HH

#  include <string>
#  include "com/centreon/connector/icmp/namespace.hh"
#  include "com/centreon/misc/get_options.hh"

CCC_ICMP_BEGIN()

/**
 *  @class cmd_options cmd_options.hh "com/centreon/connector/icmp/cmd_options.hh"
 *  @brief Parse command line arguments.
 *
 *  Parse connector's command line arguments.
 */
class                cmd_options : public misc::get_options {
public:
                     cmd_options(int argc, char** argv);
                     cmd_options(cmd_options const& right);
                     ~cmd_options() throw ();
  cmd_options&       operator=(cmd_options const& right);
  std::string const& get_appname() const throw ();
  unsigned int       get_max_concurrent_checks() const throw();
  std::string        help() const;

private:
  cmd_options&       _internal_copy(cmd_options const& right);

  std::string        _appname;
  unsigned int       _max_concurrent_checks;
};

CCC_ICMP_END()

#endif // !CCC_ICMP_CMD_OPTIONS_HH
