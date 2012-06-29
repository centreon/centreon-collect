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
#include <cstring>
#include <iostream>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/io/file_stream.hh"

using namespace com::centreon;

/**
 *  Check that file can be created, checked for and removed properly.
 *
 *  @return EXIT_SUCCESS on success.
 */
int main() {
  // Return value.
  int retval(EXIT_FAILURE);

  try {
    // Temporary path.
    char* path(io::file_stream::temp_path());

    // Remove old file.
    io::file_stream::remove(path);

    // File must not exists.
    if (io::file_stream::exists(path))
      throw (basic_error() << "file '" << path
             << "' exists whereas it should not");

    // Create file.
    {
      io::file_stream fs;
      fs.open(path, "w");
      fs.close();
    }

    // File must exists.
    if (!io::file_stream::exists(path))
      throw (basic_error() << "file '" << path
             << "' does not exist whereas it should");

    // Remove file.

    // File must not exists.
    if (!io::file_stream::remove(path)
        || io::file_stream::exists(path))
      throw (basic_error() << "file '" << path
             << "' exists whereas it should not");

    // Success.
    retval = EXIT_SUCCESS;
  }
  catch (std::exception const& e) {
    std::cout << e.what() << std::endl;
  }
  return (retval);
}
