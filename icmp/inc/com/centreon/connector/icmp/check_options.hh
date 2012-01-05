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

#ifndef CCC_ICMP_CHECK_OPTIONS_HH
#  define CCC_ICMP_CHECK_OPTIONS_HH

#  include <string>
#  include <vector>
#  include "com/centreon/connector/icmp/namespace.hh"
#  include "com/centreon/misc/get_options.hh"

CCC_ICMP_BEGIN()

/**
 *  @class check_options check_options.hh "com/centreon/connector/icmp/check_options.hh"
 *  @brief Parse connector command line arguments.
 *
 *  Parse connector's command line arguments.
 */
class            check_options : public misc::get_options {
public:
                 check_options(std::string const& command_line);
                 check_options(check_options const& right);
                 ~check_options() throw ();
  check_options& operator=(check_options const& right);

private:
  check_options& _internal_copy(check_options const& right);
};

CCC_ICMP_END()

#endif // !CCC_ICMP_CHECK_OPTIONS_HH
