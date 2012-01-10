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
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/connector/icmp/host.hh"

using namespace com::centreon::connector::icmp;

/**
 *  Check host has lost packet.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    host hst("localhost", 42);
    hst.has_send_packet();
    hst.has_recv_packet(200);
    if (hst.get_roundtrip_avg() != 200)
      throw (basic_error() << "invalid number roundtrip avg");

    hst.has_send_packet();
    hst.has_recv_packet(600);
    if (hst.get_roundtrip_avg() != 400)
      throw (basic_error() << "invalid number roundtrip avg");

    hst.has_send_packet();
    hst.has_recv_packet(200);
    if (hst.get_roundtrip_avg() != 333)
      throw (basic_error() << "invalid number roundtrip avg");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
