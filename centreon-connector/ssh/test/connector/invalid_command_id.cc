/*
** Copyright 2012 Merethis
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

#include "com/centreon/clib.hh"
#include "com/centreon/process.hh"
#include "test/connector/binary.hh"

using namespace com::centreon;

#define CMD "2\0" \
            "0\0" \
            "10\0" \
            "123456789\0" \
            "check_by_ssh " \
            "-H localhost -l user -a password " \
            "-C 'echo Merethis'\0\0\0\0"

/**
 *  Check that connector exits when receiving an invalid command ID (0).
 *
 *  @return 0 on success.
 */
int main() {
  clib::load();
  // Process.
  process p;
  p.enable_stream(process::in, true);
  p.exec(CONNECTOR_SSH_BINARY);

  // Write command.
  char const* ptr(CMD);
  unsigned int size(sizeof(CMD) - 1);
  while (size > 0) {
    unsigned int rb(p.write(ptr, size));
    size -= rb;
    ptr += rb;
  }

  // Wait for process termination.
  int retval(1);
  if (!p.wait(5000)) {
    p.terminate();
    p.wait();
  }
  else
    retval = (p.exit_code() != 0);

  clib::unload();

  return (retval);
}
