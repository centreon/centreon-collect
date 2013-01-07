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

#include "com/centreon/connector/ssh/checks/check.hh"
#include "com/centreon/connector/ssh/checks/timeout.hh"
#include "com/centreon/connector/ssh/multiplexer.hh"

using namespace com::centreon::connector::ssh;

/**
 *  Check timeout's copy construction.
 *
 *  @return 0 on success.
 */
int main() {
  // Initialization.
  multiplexer::load();

  // Base object.
  checks::check c1;
  checks::timeout t1(&c1);

  // Copy object.
  checks::timeout t2(t1);

  // Return check result.
  int retval ((t1.get_check() != &c1)
              || (t2.get_check() != &c1));

  // Unload.
  multiplexer::unload();

  return (retval);
}
