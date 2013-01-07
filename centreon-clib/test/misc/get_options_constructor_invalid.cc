/*
** Copyright 2011-2013 Merethis
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
#include "com/centreon/misc/get_options.hh"

using namespace com::centreon::misc;

/**
 *  @class my_options
 *  @brief litle implementation of get_options to test it.
 */
class my_options : public get_options {
public:
      my_options(std::vector<std::string> const& args)
        : get_options() {
        _arguments['a'] = argument("arg", 'a', "", true);
        _arguments['t'] = argument("test", 't', "", true);
        _arguments['h'] = argument("help", 'h');
        _arguments['d'] = argument("default",
                                   'd',
                                   "",
                                   true,
                                   true,
                                   "def");
        _parse_arguments(args);
      }
      ~my_options() throw () {}
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
      throw (basic_error() << "constructor failed with unknown option");

    if (!check_require_argument())
      throw (basic_error() << "constructor failed with " \
             "require argument");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
