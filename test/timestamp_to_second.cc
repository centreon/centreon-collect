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
 *  Check the timestamp to seconds.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    timestamp t1(1, 42);
    if (t1.to_second() != 1)
      throw (basic_error() << "to_second failed");

    timestamp t2(-1, 0);
    if (t2.to_second() != -1)
      throw (basic_error() << "to_second failed");

    timestamp t3(0, -42);
    if (t3.to_second() != -1)
      throw (basic_error() << "to_second failed");

    timestamp t4(-1, -42);
    if (t4.to_second() != -2)
      throw (basic_error() << "to_second failed");

    timestamp t5(1, -42);
    if (t5.to_second() != 0)
      throw (basic_error() << "to_second failed");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
