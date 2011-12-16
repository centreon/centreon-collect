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
#include <string>
#include "com/centreon/exception/basic.hh"
#include "com/centreon/misc/argument.hh"

using namespace com::centreon::misc;

/**
 * Compare two argument.
 *
 *  @param[in] a1  The first argument to check.
 *  @param[in] a2  The second argument to check.
 */
static void check_argument(argument const& a1, argument const& a2) {
  if (a1.get_long_name() != a2.get_long_name())
    throw (basic_error() << "invalid long name");
  if (a1.get_name() != a2.get_name())
    throw (basic_error() << "invalid name");
  if (a1.get_has_value() != a2.get_has_value())
    throw (basic_error() << "invalid has value");
  if (a1.get_is_set() != a2.get_is_set())
    throw (basic_error() << "invalid is set");
  if (a1.get_value() != a2.get_value())
    throw (basic_error() << "invalid value");
}

/**
 *  Check the argument copy.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    argument ref("help",
                 'c',
                 true,
                 true,
                 "help:\n --help, -h  this help");

    argument arg1(ref);
    check_argument(ref, arg1);

    argument arg2 = ref;
    check_argument(ref, arg1);
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
