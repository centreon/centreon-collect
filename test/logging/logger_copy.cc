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

#include <iostream>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/logging/logger.hh"

using namespace com::centreon::logging;

/**
 *  Check logger copy.
 *
 *  @return 0 on success.
 */
int main() {
  int retval;

  engine::load();
  try {
    logger ref(com::centreon::logging::type_info, "info");

    logger l1(ref);
    if (l1.get_prefix() != ref.get_prefix()
        || l1.get_type() != ref.get_type())
      throw (basic_error() << "copy constructor failed");


    logger l2(ref);
    if (l2.get_prefix() != ref.get_prefix()
        || l2.get_type() != ref.get_type())
      throw (basic_error() << "copy operator failed");

    retval = 0;
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    retval = 1;
  }
  engine::unload();
  return (retval);
}
