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
 *  Check credentials' less-than operator.
 *
 *  @return 0 on success.
 */
int main() {
  // Objects.
  credentials creds1("AAA", "GGG", "VVV");
  credentials creds2("BBB", "HHH", "XXX");
  credentials creds3("AAA", "HHH", "WWW");
  credentials creds4("AAA", "GGG", "ZZZ");
  credentials creds5(creds1);

  // Checks.
  return (!(creds1 < creds2)
          || !(creds1 < creds3)
          || !(creds1 < creds4)
          || (creds1 < creds5)
          || (creds5 < creds1));
}
