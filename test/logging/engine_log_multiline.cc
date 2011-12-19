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
#include <string>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/logging/engine.hh"

using namespace com::centreon::logging;

/**
 *  @class backend_test
 *  @brief litle implementation of backend to test logging engine.
 */
class                backend_test : public backend {
public:
                     backend_test() {}
                     ~backend_test() throw () {}
  void               flush() throw () {}
  void               log(char const* msg, unsigned int size) throw () {
    (void)msg;
    (void)size;
    _data.append(msg, size);
  }
  std::string const& data() const throw () { return (_data); }

private:
  std::string        _data;
};

/**
 *  Check if log message works.
 *
 *  @return 0 on success.
 */
int main() {
  static unsigned int const nb_line(1024);
  int retval;

  engine::load();
  try {
    engine& e(engine::instance());
    std::auto_ptr<backend_test> obj(new backend_test);
    e.add(obj.get(), 1, verbosity(1));

    std::ostringstream msg;
    for (unsigned int i(0); i < nb_line; ++i)
      msg << i << "\n";

    e.log(0, verbosity(1), msg.str().c_str());

    for (unsigned int i(0); i < nb_line; ++i) {
      std::ostringstream oss;
      oss << "] " << i << "\n";
      if (!obj->data().find(oss.str()))
        throw (basic_error() << "write multiline failed on line "
               << i + 1);
    }
    retval = 0;
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    retval = 1;
  }
  engine::unload();
  return (retval);
}
