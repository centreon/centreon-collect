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
#include "com/centreon/io/standard_input.hh"

using namespace com::centreon::io;

/**
 *  Check if write failed.
 *
 *  @return True on success, otherwise false.
 */
static bool standard_input_write() {
  try {
    char buf[1024];
    standard_input in;
    in.write(buf, sizeof(buf));
  }
  catch (std::exception const& e) {
    (void)e;
    return (true);
  }
  return (false);
}

/**
 *  Check the standard input read.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    if (!standard_input_write())
      throw (basic_error() << "write failed on the standard input");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
