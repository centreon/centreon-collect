/*
** Copyright 2011-2013 Centreon
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

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#ifdef LIBSSH2_WITH_LIBGCRYPT
#include <gcrypt.h>
#endif  // LIBSSH2_WITH_LIBGCRYPT
#include <libssh2.h>
#include <iostream>
#include "com/centreon/connector/ssh/multiplexer.hh"
#include "com/centreon/connector/ssh/options.hh"
#include "com/centreon/connector/ssh/policy.hh"
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/logging/file.hh"
#include "com/centreon/logging/logger.hh"

using namespace com::centreon;
using namespace com::centreon::connector::ssh;

// Should be defined by build tools.
#ifndef CENTREON_CONNECTOR_SSH_VERSION
#define CENTREON_CONNECTOR_SSH_VERSION "(development version)"
#endif  // !CENTREON_CONNECTOR_SSH_VERSION

// Termination flag.
volatile bool should_exit(false);

#ifdef LIBSSH2_WITH_LIBGCRYPT
// libgcrypt threading structure.
GCRY_THREAD_OPTION_PTHREAD_IMPL;
#endif  // LIBSSH2_WITH_LIBGCRYPT

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
}

/**
 *  Connector entry point.
 *
 *  @param[in] argc Arguments count.
 *  @param[in] argv Arguments values.
 *
 *  @return 0 on successful execution.
 */
int main(int argc, char* argv[]) {
  // Return value.
  int retval(EXIT_FAILURE);

  // Log object.
  logging::file* log_file = NULL;

  try {
    // Initializations.
    logging::engine::load();
    multiplexer::load();

    // Command line parsing.
    options opts;
    try {
      opts.parse(argc - 1, argv + 1);
    } catch (exceptions::basic const& e) {
      std::cout << e.what() << std::endl << opts.usage() << std::endl;
      return (EXIT_FAILURE);
    }
    if (opts.get_argument("help").get_is_set()) {
      std::cout << opts.help() << std::endl;
      retval = EXIT_SUCCESS;
    } else if (opts.get_argument("version").get_is_set()) {
      std::cout << "Centreon SSH Connector " << CENTREON_CONNECTOR_SSH_VERSION
                << std::endl;
      retval = EXIT_SUCCESS;
    } else {
      // Set logging object.
      if (opts.get_argument("log-file").get_is_set()) {
        std::string filename(opts.get_argument("log-file").get_value());
        log_file = new logging::file(filename);
      } else
        log_file = new logging::file(stderr);

      if (opts.get_argument("debug").get_is_set()) {
        log_file->show_pid(true);
        log_file->show_thread_id(true);
        logging::engine::instance().add(
            log_file,
            logging::type_debug | logging::type_info | logging::type_error,
            logging::high);
      } else {
        log_file->show_pid(false);
        log_file->show_thread_id(false);
        logging::engine::instance().add(
            log_file, logging::type_info | logging::type_error, logging::low);
      }
      log_info(logging::low) << "Centreon SSH Connector "
                             << CENTREON_CONNECTOR_SSH_VERSION << " starting";
#if LIBSSH2_VERSION_NUM >= 0x010205
      // Initialize libssh2.
      log_debug(logging::medium) << "initializing libssh2";
#ifdef LIBSSH2_WITH_LIBGCRYPT
      // FIXME DBR: needed or not needed ??
      // Version check should be the very first call because it
      // makes sure that important subsystems are initialized.
      if (!gcry_check_version(GCRYPT_VERSION))
        throw(basic_error() << "libgcrypt version mismatch: ");
      gcry_control(GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread);
#endif  // LIBSSH2_WITH_LIBGCRYPT
      if (libssh2_init(0))
        throw(basic_error() << "libssh2 initialization failed");
      {
        char const* version(libssh2_version(LIBSSH2_VERSION_NUM));
        if (!version)
          throw(basic_error() << "libssh2 version is too old (>= "
                              << LIBSSH2_VERSION << " required)");
        log_info(logging::low)
            << "libssh2 version " << version << " successfully loaded";
      }
#endif /* libssh2 version >= 1.2.5 */

      // Set termination handler.
      log_debug(logging::medium) << "installing termination handler";
      signal(SIGTERM, term_handler);

      // Program policy.
      policy p;
      retval = (p.run() ? EXIT_SUCCESS : EXIT_FAILURE);
    }
  } catch (std::exception const& e) {
    log_error(logging::low) << e.what();
  }

#if LIBSSH2_VERSION_NUM >= 0x010205
  // Deinitialize libssh2.
  libssh2_exit();
#endif /* libssh2 version >= 1.2.5 */

  // Deinitializations.
  multiplexer::unload();
  logging::engine::unload();
  if (log_file)
    delete log_file;

  return (retval);
}
