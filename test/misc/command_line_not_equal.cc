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
#include "com/centreon/misc/command_line.hh"

using namespace com::centreon::misc;

/**
 *  Check the command line equal operator.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    std::string cmdline(" 1 2 3 4 5 6 7 8 9 0 ");
    command_line cmd1(cmdline);
    command_line cmd2(cmdline);
    if (cmd1 != cmd2)
      throw(basic_error() << "operator not equal failed");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
