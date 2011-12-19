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
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/misc/argument.hh"

using namespace com::centreon::misc;

/**
 *  Check the argument constructor.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    std::string long_name("help");
    char name('h');
    bool has_value(true);
    bool is_set(true);
    std::string value("help:\n --help, -h  this help");
    argument arg(long_name,
                 name,
                 has_value,
                 is_set,
                 value);

    if (arg.get_long_name() != long_name)
      throw (basic_error() << "invalid long name");
    if (arg.get_name() != name)
      throw (basic_error() << "invalid name");
    if (arg.get_has_value() != has_value)
      throw (basic_error() << "invalid has value");
    if (arg.get_is_set() != is_set)
      throw (basic_error() << "invalid is set");
    if (arg.get_value() != value)
      throw (basic_error() << "invalid value");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
