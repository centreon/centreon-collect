/*
** Copyright 2011-2013 Merethis
**
** This file is part of Centreon SSH Connector.
**
** Centreon SSH Connector is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon SSH Connector is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon SSH Connector. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include "com/centreon/connector/ssh/checks/result.hh"

#define CODE1 71184
#define CODE2 3
#define CODE3 -47829

/**
 *  Check result's exit_code property.
 *
 *  @return 0 on success.
 */
int main() {
  // Object.
  com::centreon::connector::ssh::checks::result r;

  // Checks.
  int retval(0);
  r.set_exit_code(CODE1);
  for (unsigned int i = 0; i < 100; ++i)
    retval |= (r.get_exit_code() != CODE1);
  r.set_exit_code(CODE2);
  retval |= (r.get_exit_code() != CODE2);
  r.set_exit_code(CODE3);
  for (unsigned int i = 0; i < 10000; ++i)
    retval |= (r.get_exit_code() != CODE3);

  // Return check result.
  return (retval);
}
