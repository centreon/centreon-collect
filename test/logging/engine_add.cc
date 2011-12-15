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
#include "com/centreon/exception/basic.hh"
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
 *  Check add backend with null pointer.
 *
 *  @return True on success, otherwise false.
 */
static bool null_pointer() {
  try {
    engine& e(engine::instance());
    e.add(NULL, 0, verbosity());
  }
  catch (std::exception const& e) {
    (void)e;
    return (true);
  }
  return (false);
}

/**
 *  Check add backend on to the logging engine.
 *
 *  @return 0 on success.
 */
int main() {
  engine::load();
  try {
    engine& e(engine::instance());
    if (!null_pointer())
      throw (basic_error() << "try to add null pointer");

    std::auto_ptr<backend_test> obj(new backend_test);
    unsigned long id(e.add(obj.get(), 0, verbosity()));
    if (!id)
      throw (basic_error() << "add backend failed, invalid id");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  engine::unload();
  return (0);
}
