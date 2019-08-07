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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/logging/engine.hh"
#include "./backend_test.hh"

using namespace com::centreon::logging;

/**
 *  Check thread id.
 *
 *  @return True on success, otherwise false.
 */
static bool check_thread_id(std::string const& data, char const* msg) {
  void* ptr(NULL);
  char message[1024];

  int ret(sscanf(
            data.c_str(),
            "[%p] %s\n",
            &ptr,
            message));
  return (ret == 2 && !strncmp(msg, message, strlen(msg)));
}

/**
 *  Check add backend on to the logging engine.
 *
 *  @return 0 on success.
 */
int main() {
  static char msg[] = "Centreon_Clib_test";
  int retval;

  engine::load();
  try {
    engine& e(engine::instance());
    std::unique_ptr<backend_test> obj(new backend_test(
                                          false,
                                          false,
                                          none,
                                          true));
    e.add(obj.get(), 1, 0);
    e.log(1, 0, msg, sizeof(msg));
    if (!check_thread_id(obj->data(), msg))
      throw (basic_error() << "log with thread id failed");
    retval = 0;
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    retval = 1;
  }
  engine::unload();
  return (retval);
}
