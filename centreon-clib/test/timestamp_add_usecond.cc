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
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/timestamp.hh"

using namespace com::centreon;

/**
 *  Check the timestamp add microseconds.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    timestamp t1(1, 42);
    t1.add_usecond(2000000);
    if (t1.to_usecond() != 3000042)
      throw (basic_error() << "add_usecond failed");

    timestamp t2(1, 42);
    t2.add_usecond(-1000000);
    if (t2.to_usecond() != 42)
      throw (basic_error() << "add_usecond failed");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
