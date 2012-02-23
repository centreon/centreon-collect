/*
** Copyright 2011-2012 Merethis
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
#include "com/centreon/connector/icmp/icmp_socket.hh"

using namespace com::centreon::connector::icmp;

/**
 *  Delete hosts memory.
 *
 *  @param[in, out] lst  The hosts list.
 */
static void cleanup_hosts(std::list<host*> const& lst) {
  for (std::list<host*>::const_iterator
         it(lst.begin()), end(lst.end());
       it != end;
       ++it)
    delete *it;
}

/**
 *  Check write data.
 *
 *  @param[in] address  The address to write data.
 *
 *  @return True on success, otherwise false.
 */
static bool check_write(unsigned int address) {
  try {
    icmp_socket sock;
    sock.set_address(address);
    char buf[128];
    memset(buf, 42, sizeof(buf));
    unsigned long ret(sock.write(buf, sizeof(buf)));
    if (ret != sizeof(buf))
      return (false);
  }
  catch (std::exception const& e) {
    (void)e;
    return (false);
  }
  return (true);
}


/**
 *  Check write null data.
 *
 *  @param[in] address  The address to write data.
 *
 *  @return True on success, otherwise false.
 */
static bool check_null_data(unsigned int address) {
  try {
    icmp_socket sock;
    sock.set_address(address);
    sock.write(NULL, 1024);
  }
  catch (std::exception const& e) {
    (void)e;
    return (true);
  }
  return (false);
}

/**
 *  Check icmp socket write.
 *
 *  @return 0 on success.
 */
int main() {
  int ret(0);
  try {
    std::list<host*> hosts;
    host::factory("127.0.0.1", hosts);
    if (hosts.empty())
      throw (basic_error() << "impossible to resolve 127.0.0.1");
    unsigned int address(hosts.front()->get_address());
    cleanup_hosts(hosts);
    if (!check_write(address))
      throw (basic_error() << "unable to write data to 127.0.0.1");
    if (!check_null_data(address))
      throw (basic_error() << "try to write null data to 127.0.0.1");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    ret = 1;
  }
  return (ret);
}
