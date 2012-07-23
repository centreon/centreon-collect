/*
** Copyright 2012 Merethis
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

#include <cstdlib>
#include <iostream>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/io/file_entry.hh"

using namespace com::centreon;

/**
 *  Check permission is valid.
 *
 *  @return EXIT_SUCCESS on success.
 */
int main(int argc, char* argv[]) {
  (void)argc;

  int ret(EXIT_FAILURE);
  try {
    io::file_entry entry(argv[0]);
    if (entry.is_directory())
      throw (basic_error() << "permission failed: is not a directory");
    if (entry.is_link())
      throw (basic_error() << "permission failed: is not a link");
    if (!entry.is_regular())
      throw (basic_error() << "permission failed: is a regular file");

    ret = EXIT_SUCCESS;
  }
  catch (std::exception const& e) {
    std::cerr << e.what() << std::endl;
  }
  return (ret);
}
