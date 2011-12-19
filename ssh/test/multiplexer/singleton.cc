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

#include "com/centreon/connector/ssh/multiplexer.hh"

using namespace com::centreon::connector::ssh;

/**
 *  Check that the multiplexer singleton works properly.
 *
 *  @return 0 on success.
 */
int main() {
  // Return value.
  int retval(0);

  // By default, multiplexer is not loaded.
  retval |= (&multiplexer::instance() != NULL);

  // Load multiplexer.
  multiplexer::load();
  retval |= (&multiplexer::instance() == NULL);

  // Unload multiplexer.
  multiplexer::unload();
  retval |= (&multiplexer::instance() != NULL);

  // Return check result.
  return (retval);
}
