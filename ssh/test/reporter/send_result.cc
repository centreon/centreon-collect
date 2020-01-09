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

#include <cstring>
#include "com/centreon/connector/ssh/checks/result.hh"
#include "com/centreon/connector/ssh/reporter.hh"
#include "com/centreon/logging/engine.hh"
#include "test/orders/buffer_handle.hh"

using namespace com::centreon::connector::ssh;

#define EXPECTED                                                   \
  "3\00042\0001\0003\0some error might have occurred\0this is my " \
  "output\0\0\0\0"

/**
 *  Check that the reporter properly reports check results.
 *
 *  @return 0 on success.
 */
int main() {
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

  return (retval);
}
