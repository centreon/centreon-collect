/*
** Copyright 2011-2013 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
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

  // Check.
  return (retval);
}
