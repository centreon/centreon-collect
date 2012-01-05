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

#ifndef CCC_ICMP_CHECK_OBSERVER_HH
#  define CCC_ICMP_CHECK_OBSERVER_HH

#  include <string>
#  include "com/centreon/connector/icmp/namespace.hh"
#  include "com/centreon/connector/icmp/packet.hh"
#  include "com/centreon/timestamp.hh"

CCC_ICMP_BEGIN()

/**
 *  @class check_observer check_observer.hh "com/centreon/connector/icmp/check_observer.hh"
 *  @brief Allow to receive a check reslult from check dispatcher.
 */
class          check_observer {
public:
  virtual      ~check_observer() throw ();
  virtual void emit_check_result(
                 unsigned int command_id,
                 unsigned int status,
                 std::string const& msg) = 0;
};

CCC_ICMP_END()

#endif // !CCC_ICMP_CHECK_OBSERVER_HH
