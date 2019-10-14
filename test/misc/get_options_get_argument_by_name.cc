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
  ~my_options() throw() {}
};

/**
 *  Check with invalid name.
 *
 *  @return True on success, otherwise false.
 */
static bool invalid_name() {
  try {
    std::vector<std::string> args;
    my_options opt(args);
    opt.get_argument('*');
  }
  catch (std::exception const& e) {
    (void)e;
    return (true);
  }
  return (false);
}

/**
 *  Check with valide name.
 *
 *  @return True on success, otherwise false.
 */
static bool valid_name() {
  try {
    std::vector<std::string> args;
    my_options opt(args);
    argument& a1(opt.get_argument('h'));
    argument const& a2(opt.get_argument('h'));
    (void)a1;
    (void)a2;
  }
  catch (std::exception const& e) {
    (void)e;
    return (false);
  }
  return (true);
}

/**
 *  Check the get argument by name.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    if (!invalid_name())
      throw(basic_error() << "get argument with invalid "
                             "name failed");
    if (!valid_name())
      throw(basic_error() << "get argument with valid "
                             "name failed");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
