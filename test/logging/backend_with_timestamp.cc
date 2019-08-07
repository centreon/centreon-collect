/*
** Copyright 2011-2013 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
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
 *  Check time.
 *
 *  @return True on success, otherwise false.
 */
static bool check_time(std::string const& data, char const* msg) {
  if (data[0] != '[' || data.size() < 4)
    return (false);
  unsigned int time_size(
    static_cast<unsigned int>(data.size() - strlen(msg) - 1 - 3));
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

    std::unique_ptr<backend_test> obj(new backend_test(
                                          false,
                                          false,
                                          none,
                                          false));
    e.add(obj.get(), 1, 0);
    obj->show_timestamp(second);
    e.log(1, 0, msg, sizeof(msg));
    if (!check_time(obj->data(), msg))
      throw (basic_error() << "log with timestamp in second failed");
    obj->reset();

    obj->show_timestamp(millisecond);
    e.log(1, 0, msg, sizeof(msg));
    if (!check_time(obj->data(), msg))
      throw (basic_error() << "log with timestamp in millisecond " \
             "failed");
    obj->reset();

    obj->show_timestamp(microsecond);
    e.log(1, 0, msg, sizeof(msg));
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
