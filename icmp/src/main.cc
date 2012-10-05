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

#include <iostream>
#include <memory>
#include <stdlib.h>
#include "com/centreon/connector/icmp/cmd_dispatch.hh"
#include "com/centreon/connector/icmp/cmd_execute.hh"
#include "com/centreon/connector/icmp/cmd_options.hh"
#include "com/centreon/connector/icmp/version.hh"
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/logging/engine.hh"
#include "com/centreon/logging/logger.hh"
#include "com/centreon/logging/file.hh"

using namespace com::centreon::connector::icmp;
using namespace com::centreon::misc;
using namespace com::centreon;

/**
 *  Parse and setup command line options.
 *
 *  @param[in] options  The command line options.
 */
static logging::backend* setup_logger(
                           argument const& debug,
                           argument const& output) {

  logging::backend* backend(NULL);
  if (!output.get_is_set())
    backend = new logging::file("centreon_connector_icmp.log");
  else {
    if (output.get_value().empty() || output.get_value() == "console")
      backend = new logging::file(stdout);
    else
      backend = new logging::file(output.get_value());
  }

  logging::type_flags type(1 | 4);
  if (debug.get_is_set())
    type |= 2;
  logging::engine::instance().add(
                                backend,
                                type,
                                logging::verbosity(logging::high));
  return (backend);
}

/**
 *  Connector entry point.
 *
 *  @param[in] argc  The argument count.
 *  @param[in] argv  The argument array.
 *
 *  @return 0 on success.
 */
int main(int argc, char** argv) {
  int ret(EXIT_SUCCESS);

  // Load the logging engine with basic output.
  logging::engine::load();
  logging::engine& log_engine(logging::engine::instance());
  logging::file out(stdout);
  log_engine.add(
               &out,
               logging::type_flags(1 | 4),
               logging::verbosity(logging::high));
  logging::backend* backend(NULL);

  try {
    // Parse command line connector options.
    cmd_options options(argc, argv);

    // Setup the logger system.
    log_engine.remove(&out);
    backend = setup_logger(
                options.get_argument('d'),
                options.get_argument('o'));

    // Show help.
    if (options.get_argument('h').get_is_set()) {
      options.print_help();
      return (0);
    }

    // Show version.
    if (options.get_argument('v').get_is_set()) {
      std::cout << options.get_appname() << " "
                << version::get_string() << std::endl;
      return (0);
    }

    argument const& arg(options.get_argument('c'));
    // Start command line version.
    if (arg.get_is_set()) {
      cmd_execute cmd(options.get_max_concurrent_checks());
      cmd.execute(arg.get_value());
      std::cout << cmd.get_message() << std::endl;
      ret = cmd.get_status();
    }
    // Start connector version.
    else {
      cmd_dispatch cd(options.get_max_concurrent_checks());
      cd.exec();
      cd.wait();
    }
  }
  catch (std::exception const& e) {
    // Log error.
    log_error(logging::low) << e.what();
    ret = EXIT_FAILURE;
  }

  delete backend;
  // Unload the logging engine.
  logging::engine::unload();
  return (ret);
}
