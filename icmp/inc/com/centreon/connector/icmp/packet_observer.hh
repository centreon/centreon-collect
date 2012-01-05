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

#ifndef CCC_ICMP_PACKET_OBSERVER_HH
#  define CCC_ICMP_PACKET_OBSERVER_HH

#  include <string>
#  include "com/centreon/connector/icmp/namespace.hh"
#  include "com/centreon/connector/icmp/packet.hh"
#  include "com/centreon/timestamp.hh"

CCC_ICMP_BEGIN()

/**
 *  @class packet_observer packet_observer.hh "com/centreon/connector/icmp/packet_observer.hh"
 *  @brief Allow receive data from packet dispatcher.
 *
 *  This class is an observer to allow receive event from packet
 *  dispatcher when some data are available.
 */
class          packet_observer {
public:
  virtual      ~packet_observer() throw ();
  virtual void emit_receive(packet const& pkt) = 0;
};

CCC_ICMP_END()

#endif // !CCC_ICMP_PACKET_OBSERVER_HH
