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

#include <csignal>
#include <cstdlib>
#include <iostream>
#include "com/centreon/clib.hh"
#include "com/centreon/connector/perl/embedded_perl.hh"
#include "com/centreon/connector/perl/multiplexer.hh"
#include "com/centreon/connector/perl/options.hh"
#include "com/centreon/connector/perl/policy.hh"
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/logging/file.hh"
#include "com/centreon/logging/logger.hh"

using namespace com::centreon;
using namespace com::centreon::connector::perl;

// Should be defined by build tools.
#ifndef CENTREON_CONNECTOR_PERL_VERSION
#  define CENTREON_CONNECTOR_PERL_VERSION "(development version)"
#endif // !CENTREON_CONNECTOR_PERL_VERSION

// Termination flag.
volatile bool should_exit(false);

/**
 *  Termination handler.
 *
 *  @param[in] signum Unused.
 */
static void term_handler(int signum) {
  (void)signum;
  int old_errno(errno);
  log_info(logging::high) << "termination request received";
  should_exit = true;
  log_info(logging::high) << "reseting termination handler";
  signal(SIGTERM, SIG_DFL);
  errno = old_errno;
  return ;
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

  // Log object.
  logging::file log_file(stderr);

  try {
    // Initializations.
    clib::load(clib::with_logging_engine);
    multiplexer::load();

    // Command line parsing.
    options opts;
    try {
      opts.parse(argc - 1, argv + 1);
    }
    catch (exceptions::basic const& e) {
      std::cout << e.what() << std::endl << opts.usage() << std::endl;
      multiplexer::unload();
      clib::unload();
      return (EXIT_FAILURE);
    }
    if (opts.get_argument("help").get_is_set()) {
      std::cout << opts.help() << std::endl;
      retval = EXIT_SUCCESS;
    }
    else if (opts.get_argument("version").get_is_set()) {
      std::cout << "Centreon Connector Perl "
        << CENTREON_CONNECTOR_PERL_VERSION << std::endl;
      retval = EXIT_SUCCESS;
    }
    else {
      // Set logging object.
      if (opts.get_argument("debug").get_is_set()) {
        logging::engine::instance().set_show_pid(true);
        logging::engine::instance().set_show_thread_id(true);
        logging::engine::instance().add(
          &log_file,
          (1ull << logging::type_debug)
          | (1ull << logging::type_info)
          | (1ull << logging::type_error),
          logging::high);
      }
      else {
        logging::engine::instance().set_show_pid(false);
        logging::engine::instance().set_show_thread_id(false);
        logging::engine::instance().add(
          &log_file,
          (1ull << logging::type_info) | (1ull << logging::type_error),
          logging::low);
      }
      log_info(logging::low) << "Centreon Connector Perl "
        << CENTREON_CONNECTOR_PERL_VERSION << " starting";

      // Set termination handler.
      log_debug(logging::medium)
        << "installing termination handler";
      signal(SIGTERM, term_handler);

      // Load Embedded Perl.
      embedded_perl::load(&argc, &argv, &env);

      // Program policy.
      policy p;
      retval = (p.run() ? EXIT_SUCCESS : EXIT_FAILURE);
    }
  }
  catch (std::exception const& e) {
    log_error(logging::low) << e.what();
  }

  // Deinitializations.
  embedded_perl::unload();
  multiplexer::unload();
  clib::unload();

  return (retval);
}
