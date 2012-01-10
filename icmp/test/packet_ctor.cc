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
#include "com/centreon/connector/icmp/packet.hh"

using namespace com::centreon::connector::icmp;
using namespace com::centreon;

/**
 *  Create packet.
 *
 *  @param[in] size The packet size.
 *
 *  @return True on success, otherwise false.
 */
static bool create_packet(unsigned short size) {
  try {
    packet pkt(size);
  }
  catch (std::exception const& e) {
    (void)e;
    return (false);
  }
  return (true);
}

/**
 *  Create packet with data.
 *
 *  @param[in] size The packet size.
 *
 *  @return True on success, otherwise false.
 */
static bool create_packet(
              unsigned char const* data,
              unsigned short size) {
  try {
    packet pkt(data, size, timestamp::now());
  }
  catch (std::exception const& e) {
    (void)e;
    return (false);
  }
  return (true);
}

/**
 *  Check packet constructor.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    if (!create_packet(1024))
      throw (basic_error() << "create packet failed");
    if (!create_packet(0))
      throw (basic_error() << "create packet failed");
    if (!create_packet(10))
      throw (basic_error() << "create packet failed");
    if (create_packet(USHRT_MAX - 10))
      throw (basic_error() << "invalid size");

    unsigned char buf[1024];
    if (create_packet(0, sizeof(buf)))
      throw (basic_error() << "try to create invalid packet");
    if (!create_packet(buf, sizeof(buf)))
      throw (basic_error() << "create packet failed with data");
    if (create_packet(buf, 10))
      throw (basic_error() << "try to create packet failed with invalid size");
    if (create_packet(buf, 0))
      throw (basic_error() << "try to create packet failed with null size");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
