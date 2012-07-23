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
#include "com/centreon/io/file_stream.hh"

unsigned int const DATA_SIZE = 42;

using namespace com::centreon;

/**
 *  Create temporary file.
 *
 *  @param[in] path  The temporary file path.
 */
static void create_fake_file(std::string const& path) {
  if (!io::file_stream::exists(path)) {
    io::file_stream fs;
    fs.open(path, "w");
    fs.close();
  }
}

/**
 *  Check path information.
 *
 *  @return EXIT_SUCCESS on success.
 */
int main() {
  int ret(EXIT_FAILURE);

  std::string p1("/tmp/test.ext");
  std::string p2("/tmp/.test");
  std::string p3("/tmp/test");

  try {
    create_fake_file(p1);
    create_fake_file(p2);
    create_fake_file(p3);

    io::file_entry e1(p1);
    if (e1.base_name() != "test")
      throw (basic_error() << "invalid base name");
    if (e1.file_name() != "test.ext")
      throw (basic_error() << "invalid file name");
    if (e1.directory_name() != "/tmp")
      throw (basic_error() << "invalid directory name");

    io::file_entry e2(p2);
    if (e2.base_name() != ".test")
      throw (basic_error() << "invalid base name");
    if (e2.file_name() != ".test")
      throw (basic_error() << "invalid file name");
    if (e2.directory_name() != "/tmp")
      throw (basic_error() << "invalid directory name");

    io::file_entry e3(p3);
    if (e3.base_name() != "test")
      throw (basic_error() << "invalid base name");
    if (e3.file_name() != "test")
      throw (basic_error() << "invalid file name");
    if (e3.directory_name() != "/tmp")
      throw (basic_error() << "invalid directory name");

    ret = EXIT_SUCCESS;
  }
  catch (std::exception const& e) {
    std::cerr << e.what() << std::endl;
  }

  io::file_stream::remove(p1);
  io::file_stream::remove(p2);
  io::file_stream::remove(p3);

  return (ret);
}
