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
#include "com/centreon/misc/get_options.hh"

using namespace com::centreon::misc;

/**
 *  @class my_options
 *  @brief litle implementation of get_options to test it.
 */
class my_options : public get_options {
 public:
  my_options(std::vector<std::string> const& args) : get_options() {
    _arguments['a'] = argument("arg", 'a', "", true);
    _arguments['t'] = argument("test", 't', "", true);
    _arguments['h'] = argument("help", 'h');
    _arguments['d'] = argument("default", 'd', "", true, true, "def");
    _parse_arguments(args);
  }
  my_options(my_options const& right) : get_options(right) {}
  ~my_options() throw() {}
  my_options& operator=(my_options const& right) {
    get_options::operator=(right);
    return (*this);
  }
};

/**
 *  Check the get_options copy.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    std::vector<std::string> args;
    my_options ref(args);

    my_options opt1(ref);
    if (!(ref == opt1))
      throw(basic_error() << "copy constructor failed");

    my_options opt2 = ref;
    if (ref != opt2)
      throw(basic_error() << "copy operator failed");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
