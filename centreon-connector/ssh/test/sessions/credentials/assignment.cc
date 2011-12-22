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

using namespace com::centreon::connector::ssh::sessions;

/**
 *  Check that credentials can be copy constructed.
 *
 *  @return 0 on success.
 */
int main() {
  // Base object.
  credentials creds1;
  creds1.set_host("localhost");
  creds1.set_user("root");
  creds1.set_password("random words");

  // Copy object.
  credentials creds2;
  creds2 = creds1;

  // Reset base object.
  creds1.set_host("centreon.com");
  creds1.set_user("daemon");
  creds1.set_password("please let me in");

  // Check.
  return ((creds1.get_host() != "centreon.com")
          || (creds1.get_user() != "daemon")
          || (creds1.get_password() != "please let me in")
          || (creds2.get_host() != "localhost")
          || (creds2.get_user() != "root")
          || (creds2.get_password() != "random words"));
}
