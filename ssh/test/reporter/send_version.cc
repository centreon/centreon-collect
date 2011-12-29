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

#include <string.h>
#include "com/centreon/connector/ssh/reporter.hh"
#include "com/centreon/logging/engine.hh"
#include "test/orders/buffer_handle.hh"

using namespace com::centreon::connector::ssh;

#define EXPECTED "1\00042\00084\0\0\0\0"

/**
 *  Check that the reporter properly reports check results.
 *
 *  @return 0 on success.
 */
int main() {
  // Initialization.
  com::centreon::logging::engine::load();

  // Reporter.
  reporter r;
  r.send_version(42, 84);
  
  // Buffer handle.
  buffer_handle bh;
  while (r.want_write(bh))
    r.write(bh);

  // Compare what reporter wrote with what is expected.
  bool retval;
  char buffer[sizeof(EXPECTED) - 1];
  if (bh.read(buffer, sizeof(buffer)) != sizeof(buffer))
    retval = 1;
  else
    retval = memcmp(buffer, EXPECTED, sizeof(buffer));
  return (retval);
}
