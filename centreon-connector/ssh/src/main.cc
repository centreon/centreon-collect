/*
** Copyright 2011 Merethis
**
** This file is part of Centreon Connector SSH.
**
** Centreon Connector SSH is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector SSH is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector SSH. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include "com/centreon/connector/ssh/commander.hh"
#include "com/centreon/connector/ssh/multiplexer.hh"
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/logging/logger.hh"

using namespace com::centreon;
using namespace com::centreon::connector::ssh;

// Termination flag.
static volatile bool should_exit(false);

/**
 *  Termination handler.
 *
 *  @param[in] signum Unused.
 */
static void term_handler(int signum) {
  (void)signum;
  int old_errno(errno);
  logging::info(logging::high) << "termination request received";
  should_exit = true;
  logging::info(logging::low) << "reseting termination handler";
  signal(SIGTERM, SIG_DFL);
  errno = old_errno;
  return ;
}

/**
 *  Connector entry point.
 *
 *  @return 0 on successful execution.
 */
int main() {
  // Return value.
  int retval(EXIT_FAILURE);

  try {
    // Initializations.
    logging::engine::load();
    multiplexer::load();
    commander::load();

#if LIBSSH2_VERSION_NUM >= 0x010205
    // Initialize libssh2.
    logging::info(logging::medium) << "initializing libssh2";
    if (libssh2_init(0))
      throw (basic_error() << "libssh2 initialization failed");
#endif /* libssh2 version >= 1.2.5 */

    // Set termination handler.
    logging::info(logging::medium) << "installing termination handler";
    signal(SIGTERM, term_handler);

    // Listener of commands.
    commander::instance().reg();

    // Multiplexing loop.
    logging::info(logging::medium) << "starting multiplexing loop";
    while (!should_exit)
      multiplexer::instance().multiplex();
    logging::info(logging::medium) << "multiplexing loop terminated";

    // Remove command listener on input.
    logging::debug(logging::high)
      << "commander will stop listening on input";
    commander::instance().unreg(false);

    // Wait for remaining sessions.
    // XXX : multiplexer.remaining() > 1 || multiplexer.want_write()

    // Remove command listener totally.
    logging::debug(logging::high) << "removing command listener";
    commander::instance().unreg();

    // Set return value.
    retval = EXIT_SUCCESS;
  }
  catch (std::exception const& e) {
    logging::error(logging::high) << e.what();
  }

#if LIBSSH2_VERSION_NUM >= 0x010205
  // Deinitialize libssh2.
  libssh2_exit();
#endif /* libssh2 version >= 1.2.5 */

  // Deinitializations.
  commander::unload();
  multiplexer::unload();
  logging::engine::unload();

  return (retval);
}
