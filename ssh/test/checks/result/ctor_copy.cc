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
  result r2(r1);

  // Reset base object.
  r1.set_command_id(42);
  r1.set_error("foo bar");
  r1.set_executed(false);
  r1.set_exit_code(7536);
  r1.set_output("baz qux");

  // Check content.
  return ((r1.get_command_id() != 42)
          || (r1.get_error() != "foo bar")
          || r1.get_executed()
          || (r1.get_exit_code() != 7536)
          || (r1.get_output() != "baz qux")
          || (r2.get_command_id() != 14598753ull)
          || (r2.get_error() != "a random error string")
          || !r2.get_executed()
          || (r2.get_exit_code() != -46582)
          || (r2.get_output()
              != "another random string, but for the output property"));
}
