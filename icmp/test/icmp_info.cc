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
#include "com/centreon/connector/icmp/icmp_info.hh"

using namespace com::centreon::connector::icmp;
using namespace com::centreon;

/**
 *  Check check constructor.
 *
 *  @return 0 on success.
 */
int main() {
  int ret(0);
  try {
    check c;
    host h("localhost", 42);
    packet p;

    icmp_info ii(&c, &h, &p);
    if (ii.chk != &c || ii.hst != &h || ii.pkt != &p)
      throw (basic_error() << "invalid constructor");

    icmp_info i1(ii);
    if (i1.chk != &c || i1.hst != &h || i1.pkt != &p)
      throw (basic_error() << "invalid copy constructor");

    icmp_info i2 = ii;
    if (i2.chk != &c || i2.hst != &h || i2.pkt != &p)
      throw (basic_error() << "invalid copy operator");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    ret = 1;
  }
  return (ret);
}
