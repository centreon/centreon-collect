/*
** Copyright 2011 Merethis
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

#include <errno.h>
#include <iostream>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include "com/centreon/connector/perl/check_terminated.hh"
#include "com/centreon/connector/perl/embedded.hh"
#include "com/centreon/connector/perl/main_io.hh"
#include "com/centreon/connector/perl/multiplex.hh"
#include "com/centreon/connector/perl/processes.hh"

using namespace com::centreon::connector::perl;

// Child event.
static volatile bool child_event(false);

// Original signal mask.
static sigset_t original_sigmask;

// Termination flag.
static volatile bool should_exit(false);

/**
 *  Child event handler.
 *
 *  @param[in] signum Ignored.
 */
static void child_handler(int signum) {
  (void)signum;

  // Backup errno.
  int errno_saved(errno);

  // Set child event flag.
  child_event = true;

  // Set child event handler in case it was reset.
  signal(SIGCHLD, &child_handler);

  // Restore errno.
  errno = errno_saved;

  return ;
}

/**
 *  Restore original signal mask.
 */
static void restore_sigmask() {
  sigprocmask(SIG_SETMASK, &original_sigmask, NULL);
  return ;
}

/**
 *  Termination handler.
 *
 *  @param[in] signum Ignored.
 */
static void term_handler(int signum) {
  (void)signum;

  // Backup errno.
  int errno_saved(errno);

  // Set termination flag.
  should_exit = true;

  // Set termination handler in case it was reset.
  signal(SIGTERM, &term_handler);

  // Restore errno.
  errno = errno_saved;

  return ;
}

/**
 *  Program entry point.
 *
 *  @param[in] argc Argument count.
 *  @param[in] argv Argument values.
 *  @param[in] env  Program environment.
 *
 *  @return 0 on successful execution.
 */
int main(int argc, char** argv, char** env) {
  // Block signals that must be absolutely caught.
  {
    // Empty signal set.
    sigset_t sigs;
    if (sigemptyset(&sigs)) {
      char const* msg(strerror(errno));
      std::cerr << "could not reset signal set: " << msg << std::endl;
      return (EXIT_FAILURE);
    }
    // Add SIGCHLD and SIGTERM.
    if (sigaddset(&sigs, SIGCHLD)
        || sigaddset(&sigs, SIGTERM)) {
      char const* msg(strerror(errno));
      std::cerr << "could not prepare signal mask: "
                << msg << std::endl;
      return (EXIT_FAILURE);
    }
    // Set signal mask.
    if (sigprocmask(SIG_BLOCK, &sigs, &original_sigmask)) {
      char const* msg(strerror(errno));
      std::cerr << "could not set signal mask: " << msg << std::endl;
      return (EXIT_FAILURE);
    }
  }

  // Register sigmask restore routine.
  pthread_atfork(NULL, NULL, &restore_sigmask);

  // Register signal handlers.
  if ((SIG_ERR == signal(SIGCHLD, &child_handler))
      || (SIG_ERR == signal(SIGTERM, &term_handler))) {
    char const* msg(strerror(errno));
    std::cerr << "could not register signal handlers: "
              << msg << std::endl;
    return (EXIT_FAILURE);
  }

  try {
    // Initialize Perl.
    if (embedded::init(&argc, &argv, &env))
      return (EXIT_FAILURE);

    // Multiplexing loop.
    while (!should_exit) {
      // Multiplex call.
      if (multiplex(original_sigmask))
        should_exit = true;

      // Process terminated children.
      if (child_event) {
        child_event = false;
        check_terminated();
      }
    }
  }
  catch (std::exception const& e) {
    std::cerr << "terminating: " << e.what() << std::endl;
  }
  catch (...) {
    std::cerr << "terminating: unknown exception was thrown"
              << std::endl;
  }
  // Terminate running processes.
  while (!processes::instance().empty()) {
    multiplex(original_sigmask, false);
    check_terminated();
  }
  // Flush output.
  main_io& instance(main_io::instance());
  while (instance.write_wanted() && !instance.write())
    ;
  fsync(STDOUT_FILENO);

  return (EXIT_SUCCESS);
}
