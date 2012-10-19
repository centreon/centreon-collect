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
#include "com/centreon/logging/temp_logger.hh"

using namespace com::centreon::logging;

/**
 *  @class backend_test
 *  @brief litle implementation of backend to test logging engine.
 */
class          backend_test : public backend {
public:
               backend_test() : _nb_call(0) {}
               ~backend_test() throw () {}
  void         close() throw () {}
  void         flush() throw () {}
  void         log(char const* msg, unsigned int size) throw () {
    (void)msg;
    (void)size;
    ++_nb_call;
  }
  unsigned int get_nb_call() const throw () { return (_nb_call); }
  void         open() {}
  void         reopen() {}

private:
  unsigned int _nb_call;
};

/**
 *  Check temp logger with nothing to do.
 *
 *  @return 0 on success.
 */
int main() {
  int retval;

  engine::load();
  try {
    engine& e(engine::instance());
    std::auto_ptr<backend_test> obj(new backend_test);
    e.add(obj.get(), 2, verbosity(1));

    temp_logger(1, verbosity(1)) << "Centreon Clib test";
    if (obj->get_nb_call() != 1)
      throw (basic_error() << "invalid number of call log");
    retval = 0;
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    retval = 1;
  }
  engine::unload();
  return (retval);
}
