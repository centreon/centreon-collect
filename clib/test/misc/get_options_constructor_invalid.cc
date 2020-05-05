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
 *  Check the get options with unknown option.
 *
 *  @return True on success, otherwise false.
 */
static bool check_unknown_option() {
  try {
    std::vector<std::string> args;
    args.push_back("--test=1");
    args.push_back("-h");
    args.push_back("--arg");
    args.push_back("2");
    args.push_back("--failed=42");
    args.push_back("param1");
    args.push_back("param2");
    args.push_back("param3");

    my_options opt(args);
  }
  catch (std::exception const& e) {
    (void)e;
    return (true);
  }
  return (false);
}

/**
 *  Check the get options without require argument.
 *
 *  @return True on success, otherwise false.
 */
static bool check_require_argument() {
  try {
    std::vector<std::string> args;
    args.push_back("-h");
    args.push_back("--arg");
    args.push_back("2");
    args.push_back("--test");

    my_options opt(args);
  }
  catch (std::exception const& e) {
    (void)e;
    return (true);
  }
  return (false);
}

/**
 *  Check the get_options constructor.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    if (!check_unknown_option())
      throw(basic_error() << "constructor failed with unknown option");

    if (!check_require_argument())
      throw(basic_error() << "constructor failed with "
                             "require argument");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
