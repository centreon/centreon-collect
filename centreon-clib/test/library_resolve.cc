/*
** Copyright 2011-2012 Merethis
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
#include "com/centreon/library.hh"

using namespace com::centreon;

/**
 *  Check the resolve symbole.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    // create library object.
    library lib("./libshared_testing_library.so");

    // load library.
    lib.load();
    if (!lib.is_loaded())
      throw (basic_error() << "load failed");

    // load a "add" symbole.
    int (*my_add)(int, int);
    my_add = (int (*)(int, int))lib.resolve("add");
    if ((*my_add)(21, 21) != 42)
      throw (basic_error() << "resolve failed: invalid call result");

    // load a "export_lib_name" symbole.
    char const* lib_name(*(char**)lib.resolve("export_lib_name"));
    if (lib_name != std::string("shared_testing_library"))
      throw (basic_error() << "resolve failed: invalid string");

    // load a "export_lib_version" symbole.
    int lib_version(*(int*)lib.resolve("export_lib_version"));
    if (lib_version != 42)
      throw (basic_error() << "resolve failed: invalid value");

    // unload library.
    lib.unload();
    if (lib.is_loaded())
      throw (basic_error() << "unload failed");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
