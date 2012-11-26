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
    std::auto_ptr<backend_test> obj(new backend_test(
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
