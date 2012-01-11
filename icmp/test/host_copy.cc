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
#include <string.h>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/connector/icmp/host.hh"

using namespace com::centreon::connector::icmp;
using namespace com::centreon;

/**
 *  Compare host.
 *
 *  @param[in] h1  The first object to compare.
 *  @param[in] h2  The second object to compare.
 *
 *  @return True if is the same, otherwize false.
 */
bool is_same(host const& h1, host const& h2) {
  if (h1.get_address() != h2.get_address()
      || h1.get_id() != h2.get_id()
      || h1.get_error() != h2.get_error()
      || h1.get_name() != h2.get_name()
      || h1.get_packet_lost() != h2.get_packet_lost()
      || h1.get_packet_recv() != h2.get_packet_recv()
      || h1.get_packet_send() != h2.get_packet_send()
      || h1.get_roundtrip_avg() != h2.get_roundtrip_avg()
      || h1.get_roundtrip_max() != h2.get_roundtrip_max()
      || h1.get_roundtrip_min() != h2.get_roundtrip_min()
      || h1.get_total_time_waited() != h2.get_total_time_waited())
    return (false);
  return (true);
}

/**
 *  Check host constructor.
 *
 *  @return 0 on success.
 */
int main() {
  int ret(0);
  try {
    host ref("localhost", 42);
    ref.set_id(24);
    ref.set_error("error: message");
    ref.has_send_packet();
    ref.has_lost_packet(100);
    ref.has_send_packet();
    ref.has_recv_packet(10);

    host h1(ref);
    if (!is_same(ref, h1))
      throw (basic_error() << "copy constructor failed");

    host h2 = ref;
    if (!is_same(ref, h2))
      throw (basic_error() << "copy operator failed");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    ret = 1;
  }
  return (ret);
}
