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
#include "com/centreon/connector/icmp/icmp_socket.hh"

using namespace com::centreon::connector::icmp;

/**
 *  Check read data.
 *
 *  @param[in] address  The address to read data.
 *
 *  @return True on success, otherwise false.
 */
static bool check_read(unsigned int address) {
  try {
    icmp_socket sock;
    sock.set_address(address);
    char buf[128];
    unsigned long ret(sock.write(buf, sizeof(buf)));
    if (ret != sizeof(buf))
      return (false);
    ret = sock.read(buf, sizeof(buf));
    if (ret == 0 || ret >= sizeof(buf))
      return (false);
  }
  catch (std::exception const& e) {
    (void)e;
    return (false);
  }
  return (true);
}


/**
 *  Check read null data.
 *
 *  @param[in] address  The address to read data.
 *
 *  @return True on success, otherwise false.
 */
static bool check_null_data(unsigned int address) {
  try {
    icmp_socket sock;
    sock.set_address(address);
    sock.read(NULL, 1024);
  }
  catch (std::exception const& e) {
    (void)e;
    return (true);
  }
  return (false);
}

/**
 *  Check icmp socket read.
 *
 *  @return 0 on success.
 */
int main() {
  int ret(0);
  try {
    std::list<host*> hosts(host::factory("127.0.0.1"));
    if (hosts.empty())
      throw (basic_error() << "impossible to resolve 127.0.0.1");
    unsigned int address(hosts.front()->get_address());
    if (!check_read(address))
      throw (basic_error() << "unable to read data to 127.0.0.1");
    if (!check_null_data(address))
      throw (basic_error() << "try to read null data to 127.0.0.1");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    ret = 1;
  }
  return (ret);
}
