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
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/library.hh"

using namespace com::centreon;

/**
 *  Check the resolve symbole.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    // create library object.
    library lib("./libshared_testing_library.so");

    // load library.
    lib.load();
    if (!lib.is_loaded())
      throw(basic_error() << "load failed");

    // load a "add" symbole.
    int (*my_add)(int, int);
    my_add = (int (*)(int, int))lib.resolve_proc("add");
    if ((*my_add)(21, 21) != 42)
      throw(basic_error() << "resolve failed: invalid call result");

    // load a "export_lib_name" symbole.
    char const* lib_name(*(char**)lib.resolve("export_lib_name"));
    if (lib_name != std::string("shared_testing_library"))
      throw(basic_error() << "resolve failed: invalid string");

    // load a "export_lib_version" symbole.
    int lib_version(*(int*)lib.resolve("export_lib_version"));
    if (lib_version != 42)
      throw(basic_error() << "resolve failed: invalid value");

    // unload library.
    lib.unload();
    if (lib.is_loaded())
      throw(basic_error() << "unload failed");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
