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
class                backend_test : public backend {
public:
                     backend_test() {}
                     ~backend_test() throw () {}
  std::string const& data() const throw () { return (_buffer); }
  void               flush() throw () {}
  void               log(char const* msg, unsigned int size) throw () {
    _buffer.append(msg, size);
  }
  void               reset() throw () { _buffer.clear(); }

private:
  std::string _buffer;
};

/**
 *  Check time.
 *
 *  @return True on success, otherwise false.
 */
static bool check_time(std::string const& data, char const* msg) {
  if (data[0] != '[' || data.size() < 4)
    return (false);
  unsigned int time_size(data.size() - strlen(msg) - 1 - 3);
  for (unsigned int i(1); i < time_size; ++i)
    if (!isdigit(data[i]))
      return (false);
  if (data.compare(3 + time_size, strlen(msg), msg))
    return (false);
  return (true);
}

/**
 *  Check add backend on to the logging engine.
 *
 *  @return 0 on success.
 */
int main() {
  static char msg[] = "Centreon Clib test";
  int retval;

  engine::load();
  try {
    engine& e(engine::instance());
    e.set_show_pid(false);
    e.set_show_thread_id(false);

    std::auto_ptr<backend_test> obj(new backend_test);
    e.add(obj.get(), 1, verbosity(1));

    e.set_show_timestamp(engine::second);
    e.log(0, verbosity(1), msg);
    if (!check_time(obj->data(), msg))
      throw (basic_error() << "log with timestamp in second failed");
    obj->reset();

    e.set_show_timestamp(engine::millisecond);
    e.log(0, verbosity(1), msg);
    if (!check_time(obj->data(), msg))
      throw (basic_error() << "log with timestamp in millisecond " \
             "failed");
    obj->reset();

    e.set_show_timestamp(engine::microsecond);
    e.log(0, verbosity(1), msg);
    if (!check_time(obj->data(), msg))
      throw (basic_error() << "log with timestamp in microsecond " \
             "failed");
    obj->reset();
    retval = 0;
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    retval = 1;
  }
  engine::unload();
  return (retval);
}
