/**
* Copyright 2011-2013 Centreon
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* For more information : contact@centreon.com
*/

#include "com/centreon/connector/ssh/orders/options.hh"
#include <sstream>
#include "com/centreon/connector/ssh/options.hh"

using namespace com::centreon::connector::ssh;

// Options descriptions.
static char const* const debug_description =
    "If this flag is specified, print all logs messages.";
static char const* const help_description = "Print help and exit.";
static char const* const version_description =
    "Print software version and exit.";
static char const* const log_file_description =
    "Specifies the log file (default: stderr).";
static char const* const test_file_description =
    "Specifies the file used instead of stdin.";

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
options::~options() noexcept {}

/**
 *  Assignment operator.
 *
 *  @param[in] opts Object to copy.
 *
 *  @return This object.
 */
options& options::operator=(options const& opts) {
  if (this != &opts)
    misc::get_options::operator=(opts);
  return *this;
}

/**
 *  Get the help.
 */
std::string options::help() const {
  std::ostringstream oss;
  oss << "centreon_connector_ssh [args]\n"
      << "  --debug    " << debug_description << "\n"
      << "  --help     " << help_description << "\n"
      << "  --version  " << version_description << "\n"
      << "  --log-file " << log_file_description << "\n"
      << "  --test-file " << test_file_description << "\n"
      << "\n"
      << "Commands must be sent on the connector's standard input.\n"
      << "They must be sent using Centreon Connector protocol version\n"
      << "1.0 and support the following options:\n"
      << "\n"
      << orders::options::help() << "\n"
      << "Check results will be sent back using also the Centreon\n"
      << "Connector protocol version 1.0, on the process' standard\n"
      << "output.";
  return oss.str();
}

/**
 *  Parse command line arguments.
 *
 *  @param[in] argc Arguments count.
 *  @param[in] argv Arguments values.
 */
void options::parse(int argc, char* argv[]) {
  _parse_arguments(argc, argv);
}

/**
 *  Get the program usage.
 */
std::string options::usage() const {
  return help();
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

  // Log file.
  {
    misc::argument& arg(_arguments['l']);
    arg.set_name('l');
    arg.set_long_name("log-file");
    arg.set_description(log_file_description);
    arg.set_has_value(true);
  }
  // test file
  {
    misc::argument& arg(_arguments['x']);
    arg.set_name('x');
    arg.set_long_name("test-file");
    arg.set_description(test_file_description);
    arg.set_has_value(true);
  }
}
