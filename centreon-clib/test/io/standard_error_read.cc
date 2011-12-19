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
#include "com/centreon/io/standard_error.hh"

using namespace com::centreon::io;

/**
 *  Check if read failed.
 *
 *  @return True on success, otherwise false.
 */
static bool standard_error_read() {
  try {
    char buf[1024];
    standard_error err;
    err.read(buf, sizeof(buf));
  }
  catch (std::exception const& e) {
    (void)e;
    return (true);
  }
  return (false);
}

/**
 *  Check the standard error read.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    if (!standard_error_read())
      throw (basic_error() << "read failed on the standard error");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
