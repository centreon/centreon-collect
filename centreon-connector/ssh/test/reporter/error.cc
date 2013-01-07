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
#include "com/centreon/connector/ssh/reporter.hh"
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/logging/engine.hh"
#include "test/orders/buffer_handle.hh"

using namespace com::centreon::connector::ssh;

/**
 *  Check that the reporter stop being able to report when handle has
 *  some error.
 *
 *  @return 0 on success.
 */
int main() {
  // Initialization.
  com::centreon::logging::engine::load();

  int retval;
  {
    // Check result.
    checks::result cr;
    cr.set_command_id(42);

    // Buffer handle.
    buffer_handle bh;

    // Reporter.
    reporter r;
    r.send_result(cr);

    // Notify of error.
    try {
      r.error(bh);
      retval = 1;
    }
    catch (com::centreon::exceptions::basic const& e) {
      (void)e;
      retval = 0;
    }

    // Reporter cannot report anymore.
    retval |= r.can_report();
  }

  // Unload.
  com::centreon::logging::engine::unload();

  // Check.
  return (retval);
}
