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
 *  Check if backend is equal.
 *
 *  @param[in] b1  first backend to compare.
 *  @param[in] b2  second backend to compare.
 *
 *  @return True if equal, otherwise false.
 */
static bool is_same(backend const& b1, backend const& b2) {
  return (b1.enable_sync() == b2.enable_sync() &&
          b1.show_pid() == b2.show_pid() &&
          b1.show_timestamp() == b2.show_timestamp() &&
          b1.show_thread_id() == b2.show_thread_id());
}

/**
 *  Check copy backend.
 *
 *  @return 0 on success.
 */
int main() {
  int retval(0);

  engine::load();
  try {
    backend_test ref(false, true, none, false);

    backend_test c1(ref);
    if (!is_same(ref, c1))
      throw(basic_error() << "invalid copy constructor");

    backend_test c2 = ref;
    if (!is_same(ref, c2))
      throw(basic_error() << "invalid copy operator");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    retval = 1;
  }
  engine::unload();
  return (retval);
}
