/*
** Copyright 2011-2014 Centreon
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

#include <csignal>
#include <cstdlib>
#include <iostream>

#include "com/centreon/connector/log.hh"
#include "com/centreon/connector/perl/embedded_perl.hh"
#include "com/centreon/connector/perl/multiplexer.hh"
#include "com/centreon/connector/perl/options.hh"
#include "com/centreon/connector/perl/policy.hh"
#include "com/centreon/exceptions/basic.hh"

using namespace com::centreon;
using namespace com::centreon::connector;
using namespace com::centreon::connector::perl;

// Should be defined by build tools.
#ifndef CENTREON_CONNECTOR_VERSION
#define CENTREON_CONNECTOR_VERSION "(development version)"
#endif  // !CENTREON_CONNECTOR_VERSION

// Termination flag.
volatile bool should_exit(false);

/**
 *  Termination handler.
 *
 *  @param[in] signum Unused.
 */
static void term_handler([[maybe_unused]] int signum) {
  int old_errno(errno);
  should_exit = true;
  signal(SIGTERM, SIG_DFL);
  errno = old_errno;
}

/**
 *  Program entry point.
 *
 *  @param[in] argc Argument count.
 *  @param[in] argv Argument values.
 *  @param[in] env  Environment.
 *
 *  @return 0 on successful execution.
 */
int main(int argc, char** argv, char** env) {
  // Return value.
  int retval(EXIT_FAILURE);

  PERL_SYS_INIT3(&argc, &argv, &env);

  try {
    // Initializations.
    multiplexer::load();

    // Command line parsing.
    options opts;
    try {
      opts.parse(argc - 1, argv + 1);
    } catch (exceptions::basic const& e) {
      std::cout << e.what() << std::endl << opts.usage() << std::endl;
      multiplexer::unload();
      return EXIT_FAILURE;
    }
    if (opts.get_argument("help").get_is_set()) {
      std::cout << opts.help() << std::endl;
      retval = EXIT_SUCCESS;
    } else if (opts.get_argument("version").get_is_set()) {
      std::cout << "Centreon Perl Connector " << CENTREON_CONNECTOR_VERSION
                << std::endl;
      retval = EXIT_SUCCESS;
    } else {
      // Set logging object.
      if (opts.get_argument("log-file").get_is_set()) {
        std::string filename(opts.get_argument("log-file").get_value());
        log::instance().switch_to_file(filename);
      } else
        log::instance().switch_to_stdout();

      if (opts.get_argument("debug").get_is_set()) {
        log::instance().set_level(spdlog::level::trace);
      } else {
        log::instance().set_level(spdlog::level::info);
      }
      log::core()->info("Centreon Perl Connector {} starting",
                           CENTREON_CONNECTOR_VERSION);

      // Set termination handler.
      log::core()->debug("installing termination handler");

      signal(SIGTERM, term_handler);

      // Load Embedded Perl.
      embedded_perl::load(&argc, &argv, &env,
                          (opts.get_argument("code").get_is_set()
                               ? opts.get_argument("code").get_value().c_str()
                               : nullptr));

      // Program policy.
      policy p;
      retval = (p.run() ? EXIT_SUCCESS : EXIT_FAILURE);
    }
  } catch (std::exception const& e) {
    log::core()->error(e.what());
  }

  // Deinitializations.
  embedded_perl::unload();
  multiplexer::unload();

  PERL_SYS_TERM();
  return retval;
}
