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
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/logging/engine.hh"
#include "./backend_test.hh"

using namespace com::centreon::logging;

/**
 *  Check if log message works.
 *
 *  @return 0 on success.
 */
int main() {
  int retval;

  engine::load();
  try {
    engine& e(engine::instance());
    std::auto_ptr<backend_test> obj(new backend_test);

    e.log(1, 0, NULL, 0);

    unsigned int limits(sizeof(unsigned int) * CHAR_BIT);

    for (unsigned int i(0); i < 3; ++i) {
      for (unsigned int j(0); j < limits; ++j) {
        unsigned long id(e.add(obj.get(),
                               1 << j,
                               i));
        for (unsigned int k(0); k < limits; ++k)
          e.log(1 << k, i, "", 0);
        if (!e.remove(id))
          throw (basic_error() << "remove id failed");
      }
    }

    if (obj->get_nb_call() != 3 * limits)
      throw (basic_error() << "invalid number of call log function");
    retval = 0;
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    retval = 1;
  }
  engine::unload();
  return (retval);
}
