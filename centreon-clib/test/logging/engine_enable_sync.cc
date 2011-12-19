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
#include <sstream>
#include <ctype.h>
#include <string.h>
#include "com/centreon/exception/basic.hh"
#include "com/centreon/logging/engine.hh"

using namespace com::centreon::logging;

/**
 *  @class backend_test
 *  @brief litle implementation of backend to test logging engine.
 */
class  backend_test : public backend {
public:
       backend_test() : _is_flush(false) {}
       ~backend_test() throw () {}
  void flush() throw () { _is_flush = true; }
  bool is_flush() const throw () { return (_is_flush); }
  void log(char const* msg, unsigned int size) throw () {
    (void)msg;
    (void)size;
  }

private:
  bool _is_flush;
};

/**
 *  Check if engine flush all data.
 *
 *  @return 0 on success.
 */
int main() {
  static char msg[] = "Centreon Clib test";
  int retval;

  engine::load();
  try {
    engine& e(engine::instance());
    e.set_enable_sync(true);

    std::auto_ptr<backend_test> obj(new backend_test);
    e.add(obj.get(), 1, verbosity(1));

    e.log(0, verbosity(1), msg);
    if (obj->is_flush() == false)
      throw (basic_error() << "data wasn't flush");
    retval = 0;
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    retval = 1;
  }
  engine::unload();
  return (retval);
}
