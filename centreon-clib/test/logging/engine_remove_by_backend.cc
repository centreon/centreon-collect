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

    std::unique_ptr<backend_test> obj(new backend_test);
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
