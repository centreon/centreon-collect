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
#include "com/centreon/connector/icmp/packet.hh"

using namespace com::centreon::connector::icmp;
using namespace com::centreon;

/**
 *  Compare packet.
 *
 *  @param[in] p1  The first object to compare.
 *  @param[in] p2  The second object to compare.
 *
 *  @return True if is the same, otherwize false.
 */
bool is_same(packet const& p1, packet const& p2) {
  if (p1.get_address() != p2.get_address()
      || p1.get_code() != p2.get_code()
      || p1.get_error() != p2.get_error()
      || p1.get_host_id() != p2.get_host_id()
      || p1.get_id() != p2.get_id()
      || p1.get_sequence() != p2.get_sequence()
      || p1.get_size() != p2.get_size()
      || p1.get_recv_time() != p2.get_recv_time()
      || p1.get_type() != p2.get_type())
    return (false);
  return (true);
}

/**
 *  Check packet constructor.
 *
 *  @return 0 on success.
 */
int main() {
  int ret(0);
  try {
    unsigned char buf[1024];
    memset(buf, 0x42, sizeof(buf));
    packet ref(buf, sizeof(buf), timestamp::now());
    ref.set_address(123);
    ref.set_type(packet::icmp_echoreply);
    ref.set_code(34);
    ref.set_host_id(98);
    ref.set_id(55);
    ref.set_sequence(852);

    packet p1(ref);
    if (!is_same(ref, p1))
      throw (basic_error() << "copy constructor failed");

    packet p2 = ref;
    if (!is_same(ref, p2))
      throw (basic_error() << "copy operator failed");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    ret = 1;
  }
  return (ret);
}
