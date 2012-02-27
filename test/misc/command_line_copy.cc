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
#include <string.h>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/misc/command_line.hh"

using namespace com::centreon::misc;

/**
 *  Compare command line.
 *
 *  @param[in] cmd1  Command line.
 *  @param[in] cmd2  Command line.
 *
 *  @return True on success, otherwise false.
 */
static bool compare(
              command_line const& cmd1,
              command_line const& cmd2) {
  if (cmd1.get_argc() != cmd2.get_argc())
    return (false);
  char** argv1(cmd1.get_argv());
  char** argv2(cmd2.get_argv());
  for (int i(0), end(cmd1.get_argc()); i < end; ++i)
    if (strcmp(argv1[i], argv2[i]))
      return (false);
  if (argv1[cmd1.get_argc()] != argv2[cmd2.get_argc()])
    return (false);
  return (true);
}

/**
 *  Check the command line not equal operator.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    std::string cmdline("1 2 3 4 5");
    command_line ref(cmdline);

    command_line cmd1(ref);
    if (!compare(ref, cmd1))
      throw (basic_error() << "copy constructor failed");

    command_line cmd2 = ref;
    if (!compare(ref, cmd2))
      throw (basic_error() << "copy operator failed");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
