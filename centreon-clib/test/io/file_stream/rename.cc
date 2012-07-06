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

#include <cstdio>
#include <cstring>
#include <iostream>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/io/file_stream.hh"

using namespace com::centreon;

/**
 *  Check that file_stream can be rename file.
 *
 *  @return 0 on success.
 */
int main() {
  int ret(1);

  // Generate temporary file name.
  char const* file_name("./rename_file_stream.test");
  char const* new_file_name("./new_rename_file_stream.test");
  char data[] = "some data";

  try {
    // Write data.
    {
      // Open file.
      io::file_stream file(NULL, true);
      file.open(file_name, "w");

      // Write.
      if (!file.write(data, sizeof(data)))
        throw (basic_error() << "write data failed");
    }

    // Rename file.
    if (!io::file_stream::rename(file_name, new_file_name))
      throw (basic_error() << "rename failed");

    // Read data.
    {
      // Open file.
      io::file_stream file(NULL, true);
      file.open(new_file_name, "r");

      // Read.
      char buffer[64];
      if (file.read(buffer, sizeof(buffer)) != sizeof(data))
        throw (basic_error() << "read failed");
      if (strncmp(buffer, data, sizeof(data)))
        throw (basic_error() << "invalid data");
    }
    ret = 0;
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
  }
  catch (...) {
    std::cerr << "error: catch all" << std::endl;
  }

  // Remove temporary file.
  io::file_stream::remove(file_name);
  io::file_stream::remove(new_file_name);
  return (ret);
}
