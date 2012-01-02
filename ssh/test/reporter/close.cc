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
#include "com/centreon/connector/ssh/reporter.hh"
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/logging/engine.hh"
#include "test/orders/buffer_handle.hh"

using namespace com::centreon::connector::ssh;

/**
 *  Check that the reporter stop being able to report when handle is
 *  closed.
 *
 *  @return 0 on success.
 */
int main() {
  // Initialization.
  com::centreon::logging::engine::load();

  // Check result.
  checks::result cr;
  cr.set_command_id(42);

  // Buffer handle.
  buffer_handle bh;

  // Reporter.
  reporter r;
  r.send_result(cr);

  // Close reporter.
  int retval;
  try {
    r.close(bh);
    retval = 1;
  }
  catch (com::centreon::exceptions::basic const& e) {
    (void)e;
    retval = 0;
  }

  // Reporter cannot report anymore.
  retval |= r.can_report();

  // Check.
  return (retval);
}
