/*
** Copyright 2011 Merethis
**
** This file is part of Centreon Connector ICMP.
**
** Centreon Connector ICMP is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector ICMP is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector ICMP. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <libgen.h>
#include <sstream>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/connector/icmp/cmd_options.hh"

using namespace com::centreon::connector::icmp;
using namespace com::centreon::misc;

/**
 *  Default constructor.
 *
 *  @param[in] argc  The argument count.
 *  @param[in] argv  The argument array.
 */
cmd_options::cmd_options(int argc, char** argv)
  : get_options() {
  if (argc <= 0)
    throw (basic_error() << "invalid argument:argc " \
           "less or equal to zero");

  _arguments['c'] = argument(
                      "command",
                      'c',
                      "Execute command line.",
                      true);
  _arguments['d'] = argument(
                      "debug",
                      'd',
                      "Enable debug output.");
  _arguments['h'] = argument(
                      "help",
                      'h',
                      "Display this information.",
                      false);
  _arguments['m'] = argument(
                      "max-concurrent-checks",
                      'm',
                      "Max concurrent checks",
                      true,
                      true,
                      "1");
  _arguments['o'] = argument(
                      "output",
                      'o',
                      "Logging output (console, file).",
                      true);
  _arguments['v'] = argument(
                      "version",
                      'v',
                      "Display the version of the connector.",
                      false);

  _appname = basename(argv[0]);
  _parse_arguments(argc - 1, argv + 1);

  std::istringstream iss(_arguments['m'].get_value());
  if ((iss >> _max_concurrent_checks) == 0)
    throw (basic_error() << "invalid option 'm'");
}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
cmd_options::cmd_options(cmd_options const& right)
  : get_options(right) {
  _internal_copy(right);
}

/**
 *  Default destructor.
 */
cmd_options::~cmd_options() throw () {

}

/**
 *  Default copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
cmd_options& cmd_options::operator=(cmd_options const& right) {
  return (_internal_copy(right));
}

/**
 *  Get the current application name.
 *
 *  @return The current application name.
 */
std::string const& cmd_options::get_appname() const throw () {
  return (_appname);
}

/**
 *  Get the maximum concurrent checks.
 *
 *  @return The maximum concurrent checks.
 */
unsigned int cmd_options::get_max_concurrent_checks() const throw() {
  return (_max_concurrent_checks);
}


/**
 *  Get the help string.
 *
 *  @return The help string.
 */
std::string cmd_options::help() const {
  std::string help(get_options::help());
  return ("help: " + _appname + "\n" + help);
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
cmd_options& cmd_options::_internal_copy(cmd_options const& right) {
  if (this != &right) {
    get_options::_internal_copy(right);
    _appname = right._appname;
  }
  return (*this);
}
