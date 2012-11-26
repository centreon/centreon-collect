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
#include <memory>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/logging/engine.hh"
#include "./backend_test.hh"

using namespace com::centreon::logging;

/**
 *  Check remove by backend the null pointer argument.
 *
 *  @return True on success, otherwise false.
 */
static bool null_pointer() {
  try {
    engine& e(engine::instance());
    e.remove(static_cast<backend*>(NULL));
  }
  catch (std::exception const& e) {
    (void)e;
    return (true);
  }
  return (false);
}

/**
 *  Check remove backend on to the logging engine.
 *
 *  @return 0 on success.
 */
int main() {
  int retval;

  engine::load();
  try {
    engine& e(engine::instance());
    if (!null_pointer())
      throw (basic_error() << "try to remove with null pointer");

    std::auto_ptr<backend_test> obj(new backend_test);
    e.add(obj.get(), 1, 0);
    if (e.remove(obj.get()) != 1)
      throw (basic_error() << "remove one backend failed");

    static unsigned int const nb_backend(1000);
    for (unsigned int i(1); i < nb_backend; ++i)
      e.add(obj.get(), i, 0);

    if (e.remove(obj.get()) != nb_backend - 1)
      throw (basic_error() << "remove " << nb_backend
              << " backend failed");
    retval = 0;
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    retval = 1;
  }
  engine::unload();
  return (retval);
}
