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

using namespace com::centreon::logging;

/**
 *  @class backend_test
 *  @brief litle implementation of backend to test logging engine.
 */
class  backend_test : public backend {
public:
       backend_test() {}
       ~backend_test() throw () {}
  void flush() throw () {}
  void log(char const* msg, unsigned int size) throw () {
    (void)msg;
    (void)size;
  }
};

/**
 *  Check add backend on to the logging engine.
 *
 *  @return 0 on success.
 */
int main() {
  // Return value.
  int retval;

  engine::load();

  try {
    engine& e(engine::instance());
    if (e.remove(1) || e.remove(42))
      throw (basic_error() << "try to remove invalid id");

    std::auto_ptr<backend_test> obj(new backend_test);
    unsigned long id(e.add(obj.get(), 0, verbosity()));

    if (!e.remove(id))
      throw (basic_error() << "remove id failed");
    retval = 0;
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    retval = 1;
  }
  catch (...) {
    std::cerr << "unknown error" << std::endl;
    retval = 1;
  }
  engine::unload();
  return (retval);
}
