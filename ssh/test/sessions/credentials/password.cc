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

#include "com/centreon/connector/ssh/sessions/credentials.hh"

#define PASSWORD1 "mysimplepassword"
#define PASSWORD2 "thisis_42kinda*complicat3d"
#define PASSWORD3 "a/very{secure%p4ssw0rd;"

/**
 *  Check credentials' password property.
 *
 *  @return 0 on success.
 */
int main() {
  // Object.
  com::centreon::connector::ssh::sessions::credentials creds;

  // Checks.
  int retval(0);
  creds.set_password(PASSWORD1);
  for (unsigned int i = 0; i < 100; ++i)
    retval |= (creds.get_password() != PASSWORD1);
  creds.set_password(PASSWORD2);
  retval |= (creds.get_password() != PASSWORD2);
  creds.set_password(PASSWORD3);
  for (unsigned int i = 0; i < 1000; ++i)
    retval |= (creds.get_password() != PASSWORD3);

  // Return check result.
  return (retval);
}
