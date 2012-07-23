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
#include "com/centreon/io/directory_entry.hh"

using namespace com::centreon;

/**
 *  Check if filter work.
 *
 *  @return EXIT_SUCCESS on success.
 */
int main() {
  int ret(EXIT_FAILURE);
  try {
    {
      io::directory_entry entry(".");
      std::list<io::file_entry>
        lst_all(entry.entry_list());
      std::list<io::file_entry>
        lst_point(entry.entry_list(".*"));
      std::list<io::file_entry>
        lst_de(entry.entry_list("io_directory_entry*"));

      if (lst_all.size() < lst_point.size() + lst_de.size())
        throw (basic_error()
               << "invalid result size: list all");
      if (lst_point.size() < 2)
        throw (basic_error()
               << "invalid result size: list point");
      if (lst_de.size() < 2)
        throw (basic_error()
               << "invalid result size: list directory entry");
    }

    ret = EXIT_SUCCESS;
  }
  catch (std::exception const& e) {
    std::cerr << e.what() << std::endl;
  }
  return (ret);
}
