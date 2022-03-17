/*
 * Copyright 2020 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */
#include "com/centreon/library.hh"
#include <gtest/gtest.h>
#include <iostream>
#include "com/centreon/exceptions/basic.hh"

using namespace com::centreon;
using namespace com::centreon::exceptions;

void load_library_success() {
  // create library object.
  library lib("./tests/libshared_testing_library.so");
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

void load_library_failed() {
  try {
    // create library object.
    library lib("libnot_found.so");
    lib.load();
    throw(basic_error() << "load failed: lib dosn't exist");
  } catch (std::exception const& e) {
    (void)e;
  }
}

TEST(ClibLibrary, Load) {
  ASSERT_NO_THROW(load_library_success());
  ASSERT_NO_THROW(load_library_failed());
}

TEST(ClibLibrary, Resolve) {
  // create library object.
  library lib("./tests/libshared_testing_library.so");

  // load library.
  lib.load();
  ASSERT_TRUE(lib.is_loaded());

  // load a "add" symbole.
  int (*my_add)(int, int);
  my_add = (int (*)(int, int))lib.resolve_proc("add");
  ASSERT_EQ((*my_add)(21, 21), 42);

  // load a "export_lib_name" symbole.
  char const* lib_name(*(char**)lib.resolve("export_lib_name"));
  ASSERT_EQ(lib_name, std::string("shared_testing_library"));

  // load a "export_lib_version" symbole.
  int lib_version(*(int*)lib.resolve("export_lib_version"));
  ASSERT_EQ(lib_version, 42);

  // unload library.
  lib.unload();
  ASSERT_FALSE(lib.is_loaded());
}
