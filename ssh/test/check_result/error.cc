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

#include "com/centreon/connector/ssh/check_result.hh"

#define STR1 "this is the first string"
#define STR2 "this string might be longer"
#define STR3 "this is the last string that makes Centreon Connector SSH rocks !"

/**
 *  Check check_result's error property.
 *
 *  @return 0 on success.
 */
int main() {
  // Object.
  com::centreon::connector::ssh::check_result cr;

  // Checks.
  int retval(0);
  cr.set_error(STR1);
  for (unsigned int i = 0; i < 100; ++i)
    retval |= (cr.get_error() != STR1);
  cr.set_error(STR2);
  retval |= (cr.get_error() != STR2);
  cr.set_error(STR3);
  for (unsigned int i = 0; i < 10000; ++i)
    retval |= (cr.get_error() != STR3);

  // Return check result.
  return (retval);
}
