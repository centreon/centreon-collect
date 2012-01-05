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

#ifndef CCB_CONNECTOR_MISC
#  define CCB_CONNECTOR_MISC

#  include <string>
#  include <vector>
#  include "com/centreon/benchmark/connector/namespace.hh"

CCB_CONNECTOR_BEGIN()

std::vector<std::string> load_commands_file(std::string const& file);
char**                   vector_to_tab(
                           std::vector<std::string> const& v,
                           unsigned int size = 0);

CCB_CONNECTOR_END()

#endif // !CCB_CONNECTOR_MISC
