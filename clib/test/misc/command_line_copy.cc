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
static bool compare(command_line const& cmd1, command_line const& cmd2) {
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
      throw(basic_error() << "copy constructor failed");

    command_line cmd2 = ref;
    if (!compare(ref, cmd2))
      throw(basic_error() << "copy operator failed");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
