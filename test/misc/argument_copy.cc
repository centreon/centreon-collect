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
#include <string>
#include "com/centreon/exceptions/basic.hh"
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
    throw(basic_error() << "invalid long name");
  if (a1.get_description() != a2.get_description())
    throw(basic_error() << "invalid description");
  if (a1.get_name() != a2.get_name())
    throw(basic_error() << "invalid name");
  if (a1.get_has_value() != a2.get_has_value())
    throw(basic_error() << "invalid has value");
  if (a1.get_is_set() != a2.get_is_set())
    throw(basic_error() << "invalid is set");
  if (a1.get_value() != a2.get_value())
    throw(basic_error() << "invalid value");
}

/**
 *  Check the argument copy.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    argument ref(
        "help", 'c', "this help", true, true, "help:\n --help, -h  this help");

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
