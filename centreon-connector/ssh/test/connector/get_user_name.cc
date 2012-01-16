/*
** Copyright 2012 Merethis
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

#ifdef _WIN32
#  include <windows.h>
#else
#  include <errno.h>
#  include <pwd.h>
#  include <string.h>
#  include <unistd.h>
#endif // Windows or POSIX.
#include "com/centreon/exceptions/basic.hh"
#include "test/connector/get_user_name.hh"

/**
 *  Get the user name.
 *
 *  @return Current user name.
 */
std::string get_user_name() {
#ifdef _WIN32
  char buffer[32665];
  if (!GetUserName(buffer, sizeof(buffer) - 1)) {
    int errcode(GetLastError());
    throw (basic_error() << "cannot get current user name (error "
           << errcode << ")");
  }
  return (buffer);
#else
  errno = 0;
  passwd* pwd(getpwuid(getuid()));
  if (!pwd || !pwd->pw_name) {
    char const* msg(strerror(errno));
    throw (basic_error() << "cannot get current user name: " << msg);
  }
  return (pwd->pw_name);
#endif // Windows or POSIX.
}
