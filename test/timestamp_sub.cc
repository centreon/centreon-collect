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
 *  Check the timestamp substract operator.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    timestamp t1(1, 42);
    timestamp t2(2, 24);

    timestamp t3(t2 - t1);
    if (t3.to_usecond() != 999982)
      throw (basic_error() << "operator- failed");

    timestamp t4(-1, -24);
    timestamp t5(t2 - t4);
    if (t5.to_usecond() != 3000048)
      throw (basic_error() << "operator- failed");

    timestamp t6(2, 24);
    t6 -= t1;
    if (t6.to_usecond() != 999982)
      throw (basic_error() << "operator-= failed");

    timestamp t7(2, 24);
    t7 -= t4;
    if (t7.to_usecond() != 3000048)
      throw (basic_error() << "operator-= failed");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
