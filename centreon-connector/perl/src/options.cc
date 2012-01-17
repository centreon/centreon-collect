/*
** Copyright 2011-2012 Merethis
**
** This file is part of Centreon Connector Perl.
**
** Centreon Connector Perl is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector Perl is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector Perl. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <sstream>
#include "com/centreon/connector/perl/options.hh"

using namespace com::centreon::connector::perl;

// Options descriptions.
static char const* const debug_description
  = "If this flag is specified, print all logs messages.";
static char const* const help_description
  = "Print help and exit.";
static char const* const version_description
  = "Print software version and exit.";

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Default constructor.
 */
options::options() {
  _init();
}

/**
 *  Copy constructor.
 *
 *  @param[in] opts Object to copy.
 */
options::options(options const& opts) : misc::get_options(opts) {
  _init();
}

/**
 *  Destructor.
 */
options::~options() throw () {}

/**
 *  Assignment operator.
 *
 *  @param[in] opts Object to copy.
 *
 *  @return This object.
 */
options& options::operator=(options const& opts) {
  if (this != & opts)
    misc::get_options::operator=(opts);
  return (*this);
}

/**
 *  Get the help.
 */
std::string options::help() const {
  std::ostringstream oss;
  oss << "centreon_connector_perl [args]\n"
      << "  --debug    " << debug_description << "\n"
      << "  --help     " << help_description << "\n"
      << "  --version  " << version_description << "\n"
      << "\n"
      << "Commands must be sent on the connector's standard input.\n"
      << "They must be sent using Centreon Connector protocol version\n"
      << "1.0 and formatted as such:\n"
      << "\n"
      << "  <host> <user> <password> <command> [arg1] [arg2]\n"
      << "Check results will be sent back using also the Centreon\n"
      << "Connector protocol version 1.0, on the process' standard\n"
      << "output.";
  return (oss.str());
}

/**
 *  Parse command line arguments.
 *
 *  @param[in] argc Arguments count.
 *  @param[in] argv Arguments values.
 */
void options::parse(int argc, char* argv[]) {
  _parse_arguments(argc, argv);
  return ;
}

/**
 *  Get the program usage.
 */
std::string options::usage() const {
  return (help());
}

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Init argument table.
 */
void options::_init() {
  // Debug.
  {
    misc::argument& arg(_arguments['d']);
    arg.set_name('d');
    arg.set_long_name("debug");
    arg.set_description(debug_description);
  }

  // Help.
  {
    misc::argument& arg(_arguments['h']);
    arg.set_name('h');
    arg.set_long_name("help");
    arg.set_description(help_description);
  }

  // Version.
  {
    misc::argument& arg(_arguments['v']);
    arg.set_name('v');
    arg.set_long_name("version");
    arg.set_description(version_description);
  }

  return ;
}
