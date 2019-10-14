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
 *  Check the argument constructor.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    std::string long_name("help");
    char name('h');
    std::string description("this help");
    bool has_value(true);
    bool is_set(true);
    std::string value("help:\n --help, -h  this help");
    argument arg(long_name, name, description, has_value, is_set, value);

    if (arg.get_long_name() != long_name)
      throw(basic_error() << "invalid long name");
    if (arg.get_name() != name)
      throw(basic_error() << "invalid name");
    if (arg.get_description() != description)
      throw(basic_error() << "invalid description");
    if (arg.get_has_value() != has_value)
      throw(basic_error() << "invalid has value");
    if (arg.get_is_set() != is_set)
      throw(basic_error() << "invalid is set");
    if (arg.get_value() != value)
      throw(basic_error() << "invalid value");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
