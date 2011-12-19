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
#include <iostream>
#include <signal.h>
#include <stdlib.h>
#include "com/centreon/exceptions/basic.hh"

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
  should_exit = true;
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

  // Set termination handler.
  signal(SIGTERM, term_handler);

  try {
#if LIBSSH2_VERSION_NUM >= 0x010205
    // Initialize libssh2.
    if (libssh2_init(0))
      throw (basic_error() << "libssh2 initialization failed");
#endif /* libssh2 version >= 1.2.5 */

    // Multiplexing loop.
    /*while (!should_exit && multiplex())
      ;*/

    // Set return value.
    retval = EXIT_SUCCESS;
  }
  catch (std::exception const& e) {
    std::cerr << e.what() << std::endl;
  }

#if LIBSSH2_VERSION_NUM >= 0x010205
  // Deinitialize libssh2.
  libssh2_exit();
#endif /* libssh2 version >= 1.2.5 */

  return (retval);
}
