/*
** Copyright 2011-2013 Merethis
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
#include <memory>
#include <sstream>
#include <ctype.h>
#include <string.h>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/logging/engine.hh"
#include "./backend_test.hh"

using namespace com::centreon::logging;

/**
 *  Check if backend is equal.
 *
 *  @param[in] b1  first backend to compare.
 *  @param[in] b2  second backend to compare.
 *
 *  @return True if equal, otherwise false.
 */
static bool is_same(backend const& b1, backend const& b2) {
  return (b1.enable_sync() == b2.enable_sync()
          && b1.show_pid() == b2.show_pid()
          && b1.show_timestamp() == b2.show_timestamp()
          && b1.show_thread_id() == b2.show_thread_id());
}

/**
 *  Check copy backend.
 *
 *  @return 0 on success.
 */
int main() {
  int retval(0);

  engine::load();
  try {
    backend_test ref(false, true, none, false);

    backend_test c1(ref);
    if (!is_same(ref, c1))
      throw (basic_error() << "invalid copy constructor");

    backend_test c2 = ref;
    if (!is_same(ref, c2))
      throw (basic_error() << "invalid copy operator");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    retval = 1;
  }
  engine::unload();
  return (retval);
}
