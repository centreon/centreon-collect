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
 *  Check load library with a valid file.
 */
void load_library_success() {
  // create library object.
  library lib("./libshared_testing_library.so");
  if (lib.is_loaded())
    throw (basic_error() << "constructor failed");

  // load library.
  lib.load();
  if (!lib.is_loaded())
    throw (basic_error() << "load failed");

  // unload library.
  lib.unload();
  if (lib.is_loaded())
    throw (basic_error() << "unload failed");
}

/**
 *  load library without a valid file.
 */
void load_library_failed() {
  try {
    // create library object.
    library lib("libnot_found.so");
    lib.load();
    throw (basic_error() << "load failed: lib dosn't exist");
  }
  catch (std::exception const& e) {
    (void)e;
  }
}

/**
 *  Check the load symbole.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    load_library_success();
    load_library_failed();
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
