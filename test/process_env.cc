/*
** Copyright 2011-2012 Merethis
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

#include <cstdlib>
#include <iostream>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/process.hh"

using namespace com::centreon;

/**
 *  Check class process (environement).
 *
 *  @return EXIT_SUCCESS on success.
 */
int main() {
  int ret(EXIT_SUCCESS);
  try {
    process p;
    char* env[] = {
      (char*)"key1=value1",
      (char*)"key2=value2",
      (char*)"key3=value3",
      NULL
    };
    p.exec("./bin_test_process_output check_env "
           "key1=value1 key2=value2 key3=value3",
           env);
    p.wait();
    if (p.exit_code()!= EXIT_SUCCESS)
      throw (basic_error() << "check environment failed");
  }
  catch (std::exception const& e) {
    ret = EXIT_FAILURE;
    std::cerr << "error: " << e.what() << std::endl;
  }
  return (ret);
}
