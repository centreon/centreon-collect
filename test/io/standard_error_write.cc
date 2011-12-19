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
 *  Check null pointer parameter.
 *
 *  @return True on success, otherwise false.
 */
static bool null_pointer() {
  try {
    standard_error out;
    out.write(NULL, 0);
  }
  catch (std::exception const& e) {
    (void)e;
    return (true);
  }
  return (false);
}

/**
 *  Check write data.
 *
 *  @return True on success, otherwise false.
 */
static bool write_data() {
  char buf[] = "write test\n";
  standard_error out;
  return (out.write(buf, sizeof(buf) - 1) == sizeof(buf) - 1);
}

/**
 *  Check the standard error read.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    if (!null_pointer())
      throw (basic_error() << "try to write null pointer");

    if (!write_data())
      throw (basic_error() << "write on the standard error failed");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
