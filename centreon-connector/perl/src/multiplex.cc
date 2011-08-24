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
#ifdef HAVE_PPOLL
# ifndef _GNU_SOURCE
#  define _GNU_SOURCE // Required to get ppoll() prototype.
# endif /* !_GNU_SOURCE */
# include <poll.h>
#endif /* HAVE_PPOLL */
#include <string.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>
#include "com/centreon/connector/perl/array_ptr.hh"
#include "com/centreon/connector/perl/main_io.hh"
#include "com/centreon/connector/perl/multiplex.hh"
#include "com/centreon/connector/perl/processes.hh"

/**
 *  Multiplex I/O.
 *
 *  @param[in] sigmask    Current signal mask.
 *  @param[in] with_stdin Should we monitor stdin ?
 *
 *  @return 0 on success.
 */
int com::centreon::connector::perl::multiplex(sigset_t sigmask,
                                              bool with_stdin) {
  // FD limit (maximum FD for pselect, number of FD for ppoll.
  int limit;

#ifdef HAVE_PPOLL
  // Set limit.
  limit = processes::instance().size() * 2 + 2;

  // FD set.
  array_ptr<pollfd> fds(new pollfd[limit]);
  memset(fds.get(), 0, sizeof(pollfd) * limit);

  // Add main IO FD.
  if (with_stdin) {
    pollfd& pfd(fds[0]);
    pfd.fd = STDIN_FILENO;
    pfd.events = (POLLIN | POLLPRI);
    limit = 1;
  }
  else
    limit = 0;
  {
    pollfd& pfd(fds[limit]);
    pfd.fd = STDOUT_FILENO;
    if (main_io::instance().write_wanted())
      pfd.events = POLLOUT;
    ++limit;
  }
#else
  // FD sets.
  fd_set efds;
  fd_set rfds;
  fd_set wfds;
  FD_ZERO(&efds);
  FD_ZERO(&rfds);
  FD_ZERO(&wfds);

  // Add main IO FD.
  if (with_stdin) {
    FD_SET(STDIN_FILENO, &efds);
    FD_SET(STDIN_FILENO, &rfds);
    limit = STDIN_FILENO;
  }
  else
    limit = -1;
  FD_SET(STDOUT_FILENO, &efds);
  if (main_io::instance().write_wanted()) {
    FD_SET(STDOUT_FILENO, &wfds);
    limit = STDOUT_FILENO;
  }
#endif /* HAVE_PPOLL */

  // Timeout.
  timespec timeout;
  memset(&timeout, 0, sizeof(timeout));
  // POSIX tells that maximum timeout interval
  // of select should be at least 31 days.
  timeout.tv_sec = 31 * 24 * 60 * 60 - 1;

  // Current time.
  time_t now(time(NULL));

  // Fill sets with processes FD.
  for (std::map<pid_t, process*>::iterator
         it = processes::instance().begin(),
         end = processes::instance().end();
       it != end;
       ++it) {
    // Add process FDs to sets.
    int efd(it->second->read_err_fd());
    int ofd(it->second->read_out_fd());
    if (efd >= 0) {
#ifdef HAVE_PPOLL
      pollfd& pfd(fds[limit]);
      pfd.fd = efd;
      pfd.events = (POLLIN | POLLPRI);
      ++limit;
#else
      FD_SET(efd, &efds);
      FD_SET(efd, &rfds);
      if (efd > limit)
        limit = efd;
#endif /* HAVE_PPOLL */
    }
    if (ofd >= 0) {
#ifdef HAVE_PPOLL
      pollfd& pfd(fds[limit]);
      pfd.fd = ofd;
      pfd.events = (POLLIN | POLLPRI);
      ++limit;
#else
      FD_SET(ofd, &efds);
      FD_SET(ofd, &rfds);
      if (ofd > limit)
        limit = ofd;
#endif /* HAVE_PPOLL */
    }

    // Timeout.
    time_t t(it->second->timeout() - now);
    if (t <= now)
      timeout.tv_sec = 0;
    else {
      t -= now;
      if (t < timeout.tv_sec)
        timeout.tv_sec = t;
    }
  }

  // Unblock SIGTERM and SIGCHLD during pselect() or ppoll().
  if (sigdelset(&sigmask, SIGCHLD) || sigdelset(&sigmask, SIGTERM)) {
    std::cerr << "could not remove SIGCHLD or SIGTERM from signal mask"
              << std::endl;
    return (1);
  }

  // Watch I/O.
#ifdef HAVE_PPOLL
  if (ppoll(fds.get(), limit, &timeout, &sigmask) < 0) {
#else
  if (pselect(limit + 1, &rfds, &wfds, &efds, &timeout, &sigmask) < 0) {
#endif /* HAVE_PPOLL */
    if (errno != EINTR) {
      char const* msg(strerror(errno));
      std::cerr << "multiplexing failed: " << msg << std::endl;
      return (1);
    }
    else
      return (0);
  }

  // Get current time.
  now = time(NULL);

  // Multiplexing.
#ifdef HAVE_PPOLL
  limit = (with_stdin ? 2 : 1);
#endif /* HAVE_PPOLL */
  for (std::map<pid_t, process*>::iterator
         it = processes::instance().begin(),
         end = processes::instance().end();
       it != end;
       ++it) {
    // Get FD.
    int efd(it->second->read_err_fd());
    int ofd(it->second->read_out_fd());

#ifdef HAVE_PPOLL
    // Get stderr pollfd.
    pollfd* polle;
    if (efd >= 0) {
      while (fds[limit].fd != efd)
        ++limit;
      polle = &fds[limit];
      ++limit;
    }
    else
      polle = NULL;

    // Get stdout pollfd.
    pollfd* pollo;
    if (ofd >= 0) {
      while (fds[limit].fd != ofd)
        ++limit;
      pollo = &fds[limit];
      ++limit;
    }
    else
      pollo = NULL;

    // Check for error.
    if ((polle && (polle->revents & POLLERR))
        || (pollo && (pollo->revents & POLLERR))
        || (it->second->timeout() >= now)) {
#else
    // Check for error.
    if (((efd >= 0) && FD_ISSET(efd, &efds))
        || ((ofd >= 0) && FD_ISSET(ofd, &efds))
        || (it->second->timeout() >= now)) {
#endif /* HAVE_PPOLL */
      it->second->close();
      kill(it->first, it->second->signal());
      it->second->timeout(now + 10);
      it->second->signal(SIGKILL);
    }
    // Check for I/O.
#ifdef HAVE_PPOLL
    else if (pollo && (pollo->revents & (POLLIN | POLLPRI)))
      it->second->read_out();
    else if (polle && (polle->revents & (POLLIN | POLLPRI)))
      it->second->read_err();
#else
    else if ((ofd >= 0) && FD_ISSET(ofd, &rfds))
      it->second->read_out();
    else if ((efd >= 0) && FD_ISSET(efd, &rfds))
      it->second->read_err();
#endif /* HAVE_PPOLL */
  }

  // Main IO FD.
  int retval(0);
#ifdef HAVE_PPOLL
  // Check for error.
  if (fds[0].revents & POLLERR) {
    std::cerr << "error on stdin or stdout" << std::endl;
    return (1);
  }
  if (with_stdin) {
    if (fds[1].revents & POLLERR) {
      std::cerr << "error on stdout" << std::endl;
      return (1);
    }
    limit = 1;
  }
  else
    limit = 0;

  // I/O.
  if (with_stdin && (fds[0].revents & (POLLIN | POLLPRI)))
    retval |= main_io::instance().read();
  if (main_io::instance().write_wanted()
      && (fds[limit].revents & POLLOUT))
    retval |= main_io::instance().write();
#else
  // Check for error.
  if ((with_stdin && FD_ISSET(STDIN_FILENO, &efds))
      || FD_ISSET(STDOUT_FILENO, &efds)) {
    std::cerr << "error on stdin or stdout" << std::endl;
    return (1);
  }

  // I/O.
  if (with_stdin && FD_ISSET(STDIN_FILENO, &rfds))
    retval |= main_io::instance().read();
  if (main_io::instance().write_wanted()
      && FD_ISSET(STDOUT_FILENO, &wfds))
    retval |= main_io::instance().write();
#endif /* HAVE_PPOLL */

  return (retval);
}
