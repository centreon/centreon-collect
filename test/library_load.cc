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
 *  Check load library with a valid file.
 */
void load_library_success() {
  // create library object.
  library lib("./libshared_testing_library.so");
  if (lib.is_loaded())
    throw(basic_error() << "constructor failed");

  // load library.
  lib.load();
  if (!lib.is_loaded())
    throw(basic_error() << "load failed");

  // unload library.
  lib.unload();
  if (lib.is_loaded())
    throw(basic_error() << "unload failed");
}

/**
 *  load library without a valid file.
 */
void load_library_failed() {
  try {
    // create library object.
    library lib("libnot_found.so");
    lib.load();
    throw(basic_error() << "load failed: lib dosn't exist");
  }
  catch (std::exception const& e) {
    (void)e;
  }
}

/**
 *  Check the load symbole.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    load_library_success();
    load_library_failed();
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
