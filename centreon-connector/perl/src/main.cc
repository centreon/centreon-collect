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

#include <iostream>
#include <stdlib.h>
#include "com/centreon/connector/perl/multiplexer.hh"
#include "com/centreon/connector/perl/options.hh"
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/logging/file.hh"
#include "com/centreon/logging/logger.hh"

using namespace com::centreon;
using namespace com::centreon::connector::perl;

// Should be defined by build tools.
#ifndef CENTREON_CONNECTOR_PERL_VERSION
#  define CENTREON_CONNECTOR_PERL_VERSION "(development version)"
#endif // !CENTREON_CONNECTOR_PERL_VERSION

/**
 *  Program entry point.
 *
 *  @param[in] argc Argument count.
 *  @param[in] argv Argument values.
 *
 *  @return 0 on successful execution.
 */
int main(int argc, char** argv) {
  // Return value.
  int retval(EXIT_FAILURE);

  // Log object.
  logging::file log_file(stderr);

  try {
    // Initializations.
    logging::engine::load();
    multiplexer::load();

    // Command line parsing.
    options opts;
    try {
      opts.parse(argc - 1, argv + 1);
    }
    catch (exceptions::basic const& e) {
      std::cout << e.what() << std::endl << opts.usage() << std::endl;
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
      if (opts.get_argument("version").get_is_set()) {
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
      logging::info(logging::low) << "Centreon Connector Perl "
        << CENTREON_CONNECTOR_PERL_VERSION << " starting";
      // XXX: to implement
    }
  }
  catch (std::exception const& e) {
    logging::error(logging::low) << e.what();
  }

  // Deinitializations.
  multiplexer::unload();
  logging::engine::unload();

  return (retval);
}
