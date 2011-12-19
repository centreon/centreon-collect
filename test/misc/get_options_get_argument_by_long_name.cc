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
#include "com/centreon/exception/basic.hh"
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
        _arguments['a'] = argument("arg", 'a', true);
        _arguments['t'] = argument("test", 't', true);
        _arguments['h'] = argument("help", 'h');
        _arguments['d'] = argument("default", 'd', true, true, "def");
        _parse_arguments(args);
      }
      ~my_options() throw () {}
};

/**
 *  Check with invalid long name.
 *
 *  @return True on success, otherwise false.
 */
static bool invalid_long_name() {
  try {
    std::vector<std::string> args;
    my_options opt(args);
    opt.get_argument("unknown");
  }
  catch (std::exception const& e) {
    (void)e;
    return (true);
  }
  return (false);
}

/**
 *  Check with valide long name.
 *
 *  @return True on success, otherwise false.
 */
static bool valid_long_name() {
  try {
    std::vector<std::string> args;
    my_options opt(args);
    argument& a1(opt.get_argument("help"));
    argument const& a2(opt.get_argument("help"));
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
 *  Check the get argument by long name.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    if (!invalid_long_name())
      throw (basic_error() << "get argument with invalid " \
             "long name failed");
    if (!valid_long_name())
      throw (basic_error() << "get argument with valid " \
             "long name failed");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
