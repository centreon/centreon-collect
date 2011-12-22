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

#include "com/centreon/connector/ssh/checks/result.hh"

#define ID1 71184
#define ID2 15
#define ID3 741258963148368872ull

/**
 *  Check result's command_id property.
 *
 *  @return 0 on success.
 */
int main() {
  // Object.
  com::centreon::connector::ssh::checks::result r;

  // Checks.
  int retval(0);
  r.set_command_id(ID1);
  for (unsigned int i = 0; i < 100; ++i)
    retval |= (r.get_command_id() != ID1);
  r.set_command_id(ID2);
  retval |= (r.get_command_id() != ID2);
  r.set_command_id(ID3);
  for (unsigned int i = 0; i < 10000; ++i)
    retval |= (r.get_command_id() != ID3);

  // Return check result.
  return (retval);
}
