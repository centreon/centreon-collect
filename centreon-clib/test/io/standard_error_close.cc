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
#include "com/centreon/io/standard_error.hh"

using namespace com::centreon::io;

/**
 *  Check if close works correctly.
 *
 *  @return True on success, otherwise false.
 */
static bool close_standard_error() {
  try {
    standard_error err;
    char buf[1024];
    err.close();
    err.write(buf, sizeof(buf));
  }
  catch (std::exception const& e) {
    (void)e;
    return (true);
  }
  return (false);
}

/**
 *  Check the standard error close.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    if (!close_standard_error())
      throw (basic_error() << "close on the standard error failed");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
