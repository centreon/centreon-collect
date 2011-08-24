/*
** Copyright 2011 Merethis
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
#include <poll.h>
#include <string.h>
#include <time.h>
#include "com/centreon/connector/ssh/array_ptr.hh"
#include "com/centreon/connector/ssh/exception.hh"
#include "com/centreon/connector/ssh/multiplex.hh"
#include "com/centreon/connector/ssh/session.hh"
#include "com/centreon/connector/ssh/sessions.hh"
#include "com/centreon/connector/ssh/std_io.hh"

using namespace com::centreon::connector::ssh;

// Should we check stdin ?
static bool check_in(true);

/**
 *  Multiplex inputs and outputs.
 *
 *  @return true while multiplexing is possible.
 */
bool com::centreon::connector::ssh::multiplex() {
  // Allocate pollfd array.
  unsigned int size(sessions::instance().size() + 2);
  array_ptr<pollfd> fds(new pollfd[size]);
  memset(fds.get(), 0, size * sizeof(pollfd));

  // Timeout.
  time_t now(time(NULL));
  time_t timeout(now + 31 * 24 * 60 * 60);

  // Browse all sessions.
  unsigned int nfds(0);
  for (std::map<credentials, session*>::iterator
         it = sessions::instance().begin(),
         end = sessions::instance().end();
       it != end;
       ++it) {
    // Add socket to FD array.
    int fd(it->second->socket());
    if (fd >= 0) {
      pollfd& pfd(fds[nfds++]);
      pfd.fd = fd;
      if (it->second->read_wanted())
        pfd.events = (POLLIN | POLLPRI);
      if (it->second->write_wanted())
        pfd.events |= POLLOUT;
    }

    // Check timeout.
    time_t sessto(it->second->get_timeout());
    if (sessto < timeout)
      timeout = ((sessto < now) ? now : sessto);
  }

  // Add standard IO objects to array.
  if (check_in) {
    pollfd& pfd(fds[nfds++]);
    pfd.fd = STDIN_FILENO;
    pfd.events = (POLLIN | POLLPRI);
  }
  if (std_io::instance().write_wanted()) {
    pollfd& pfd(fds[nfds++]);
    pfd.fd = STDOUT_FILENO;
    pfd.events = POLLOUT;
  }

  // Multiplex.
  int ret;
  if ((ret = poll(fds.get(), nfds, (timeout - now) * 1000)) < 0) {
    char const* msg(strerror(errno));
    throw (exception() << "multiplexing failed: " << msg);
  }

  // Get current time.
  now = time(NULL);

  // Loop through all sessions.
  nfds = 0;
  for (std::map<credentials, session*>::iterator
         it = sessions::instance().begin(),
         end = sessions::instance().end();
       it != end;) {
    // Check for timed out channels.
    it->second->timeout(now);

    // Get FD.
    int fd(it->second->socket());
    if (fd >= 0) {
      // FD monitoring structure.
      pollfd& pfd(fds[nfds++]);

      // Check for I/O availability.
      bool error(pfd.revents & (POLLERR | POLLNVAL));
      if (!error) {
        try {
          if (it->second->read_wanted()
              && (pfd.revents & (POLLIN | POLLPRI)))
            it->second->read();
          else if (it->second->write_wanted()
                   && (pfd.revents & POLLOUT))
            it->second->write();
        }
        catch (...) {
          error = true;
        }
      }

      // In case of error, delete session.
      if (error || (!check_in && it->second->empty()))
        sessions::instance().erase(it++);
      else
        ++it;
    }
  }

  // Check standard I/O objects.
  if (check_in) {
    pollfd& pfd(fds[nfds++]);
    if (pfd.revents & (POLLERR | POLLNVAL))
      check_in = false;
    else if (pfd.revents & (POLLIN | POLLPRI))
      std_io::instance().read();
  }
  if (std_io::instance().write_wanted()) {
    pollfd& pfd(fds[nfds++]);
    if (pfd.revents & (POLLERR | POLLNVAL))
      throw (exception() << "error on standard output");
    else if (pfd.revents & POLLOUT)
      std_io::instance().write();
  }

  return (check_in || !sessions::instance().empty());
}
