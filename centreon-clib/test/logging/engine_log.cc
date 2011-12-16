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
class          backend_test : public backend {
public:
               backend_test() : _nb_call(0) {}
               ~backend_test() throw () {}
  void         flush() throw () {}
  void         log(char const* msg, unsigned int size) throw () {
    (void)msg;
    (void)size;
    ++_nb_call;
  }
  unsigned int get_nb_call() const throw () { return (_nb_call); }

private:
  unsigned int _nb_call;
};

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

    e.log(1, verbosity(1), NULL);

    for (unsigned int i(1); i < 4; ++i) {
      for (unsigned int j(0); j < sizeof(type_flags) * CHAR_BIT; ++j) {
        verbosity verbose(i);
        unsigned long id(e.add(obj.get(),
                               (static_cast<type_flags>(1) << j),
                               verbose));
        for (unsigned int k(0); k < sizeof(type_flags) * CHAR_BIT; ++k)
          e.log(k, verbose, "");
        if (!e.remove(id))
          throw (basic_error() << "remove id failed");
      }
    }

    if (obj->get_nb_call() != 3 * sizeof(type_flags) * CHAR_BIT)
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
