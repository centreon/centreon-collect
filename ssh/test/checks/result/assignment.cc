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

using namespace com::centreon::connector::ssh::checks;

/**
 *  Check that result's copy constructor works properly.
 *
 *  @return 0 on success.
 */
int main() {
  // Base object.
  result r1;
  r1.set_command_id(14598753ull);
  r1.set_error("a random error string");
  r1.set_executed(true);
  r1.set_exit_code(-46582);
  r1.set_output("another random string, but for the output property");

  // Copied object.
  result r2;
  r2 = r1;

  // Reset base object.
  r1.set_command_id(42);
  r1.set_error("foo bar");
  r1.set_executed(false);
  r1.set_exit_code(7536);
  r1.set_output("baz qux");

  // Check content.
  return ((r1.get_command_id() != 42) || (r1.get_error() != "foo bar") ||
          r1.get_executed() || (r1.get_exit_code() != 7536) ||
          (r1.get_output() != "baz qux") ||
          (r2.get_command_id() != 14598753ull) ||
          (r2.get_error() != "a random error string") || !r2.get_executed() ||
          (r2.get_exit_code() != -46582) ||
          (r2.get_output() !=
           "another random string, but for the output property"));
}
