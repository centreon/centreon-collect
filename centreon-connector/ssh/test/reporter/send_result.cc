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
#include "com/centreon/connector/ssh/checks/result.hh"
#include "com/centreon/connector/ssh/reporter.hh"
#include "com/centreon/logging/engine.hh"
#include "test/orders/buffer_handle.hh"

using namespace com::centreon::connector::ssh;

#define EXPECTED "3\00042\0001\0003\0some error might have occurred\0this is my output\0\0\0\0"

/**
 *  Check that the reporter properly reports check results.
 *
 *  @return 0 on success.
 */
int main() {
  // Initialization.
  com::centreon::logging::engine::load();

  bool retval;
  {
    // Check result.
    checks::result cr;
    cr.set_command_id(42);
    cr.set_executed(true);
    cr.set_exit_code(3);
    cr.set_output("this is my output");
    cr.set_error("some error might have occurred");

    // Reporter.
    reporter r;
    r.send_result(cr);

    // Buffer handle.
    buffer_handle bh;
    while (r.want_write(bh))
      r.write(bh);

    // Compare what reporter wrote with what is expected.
    char buffer[sizeof(EXPECTED) - 1];
    if (bh.read(buffer, sizeof(buffer)) != sizeof(buffer))
      retval = true;
    else
      retval = memcmp(buffer, EXPECTED, sizeof(buffer));
  }

  // Unload.
  com::centreon::logging::engine::unload();

  return (retval);
}
