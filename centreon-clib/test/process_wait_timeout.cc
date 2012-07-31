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
#include "com/centreon/clib.hh"
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/process.hh"

using namespace com::centreon;

/**
 *  Check class process (wait timeout).
 *
 *  @return EXIT_SUCCESS on success.
 */
int main() {
  int ret(EXIT_SUCCESS);
  clib::load();
  try {
    process p;
    p.exec("./bin_test_process_output check_sleep 1");
    if (p.wait(500) == true)
      throw (basic_error() << "wait timeout failed: "
             "waiting more than necessary");
    if (p.wait(1000) == false)
      throw (basic_error() << "wait timeout failed: "
             "waiting less than necessary");
  }
  catch (std::exception const& e) {
    ret = EXIT_FAILURE;
    std::cerr << "error: " << e.what() << std::endl;
  }
  clib::unload();
  return (ret);
}
