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
 *  Check size is valid.
 *
 *  @return EXIT_SUCCESS on success.
 */
int main() {
  int ret(EXIT_FAILURE);

  std::string temp(io::file_stream::temp_path());
  try {
    {
      io::file_stream fs;
      fs.open(temp, "w");
      fs.close();
    }

    io::file_entry entry(temp);
    if (entry.size())
      throw (basic_error() << "invalid file size: not empty");

    {
      std::string data(DATA_SIZE, ' ');
      io::file_stream fs;
      fs.open(temp, "w");
      fs.write(data.c_str(), data.size());
      fs.close();
    }

    if (entry.size())
      throw (basic_error() << "invalid file size: not empty");

    entry.refresh();

    if (entry.size() != DATA_SIZE)
      throw (basic_error() << "invalid file size: is empty");


    ret = EXIT_SUCCESS;
  }
  catch (std::exception const& e) {
    std::cerr << e.what() << std::endl;
  }

  if (io::file_stream::exists(temp))
    io::file_stream::remove(temp);

  return (ret);
}
