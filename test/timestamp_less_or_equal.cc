/*
** Copyright 2011 Merethis
**
** This file is part of Centreon Clib.
**
** Centreon Clib is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Clib is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Clib. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include "com/centreon/exception/basic.hh"
#include "com/centreon/timestamp.hh"

using namespace com::centreon;

/**
 *  Check the timestamp less or equal operator.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    timestamp t1(2, 0);

    timestamp t2(1, 0);
    if (t1 <= t2)
      throw (basic_error() << "operator<= failed");

    timestamp t3(3, -1000);
    if (t3 <= t2)
      throw (basic_error() << "operator<= failed");

    timestamp t4(1, -1);
    if (t2 <= t4)
      throw (basic_error() << "operator<= failed");

    timestamp t5(-1, 0);
    if (t1 <= t5)
      throw (basic_error() << "operator<= failed");

    timestamp t6(0, 1000000);
    if (!(t6 <= t2))
      throw (basic_error() << "operator<= failed");

    timestamp t7(1, 0);
    if (!(t7 <= t2))
      throw (basic_error() << "operator<= failed");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
