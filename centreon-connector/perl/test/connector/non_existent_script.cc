/*
** Copyright 2012 Merethis
**
** This file is part of Centreon Connector Perl.
**
** Centreon Connector Perl is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector Perl is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector Perl. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <sstream>
#include <string>
#include <string.h>
#include "com/centreon/process.hh"
#include "test/connector/binary.hh"

using namespace com::centreon;

#define CMD "2\0" \
            "4242\0" \
            "3\0" \
            "123456789\0" \
            "/non/existent/script.pl\0\0\0\0"
#define RESULT "3\0" \
               "4242\0" \
               "0\0" \
               "-1\0" \
               " \0" \
               " \0\0\0\0"

/**
 *  Check that connector respond properly when running a non existent
 *  script.
 *
 *  @return 0 on success.
 */
int main() {
  // Process.
  process p;
  p.with_stdin(true);
  p.with_stdout(true);
  p.exec(CONNECTOR_PERL_BINARY);

  // Write command.
  std::ostringstream oss;
  oss.write(CMD, sizeof(CMD) - 1);
  std::string cmd(oss.str());
  char const* ptr(cmd.c_str());
  unsigned int size(cmd.size());
  while (size > 0) {
    unsigned int rb(p.write(ptr, size));
    size -= rb;
    ptr += rb;
  }
  p.with_stdin(false);

  // Read reply.
  char buffer[1024];
  size = 0;
  {
    unsigned int rb(0);
    rb = p.read(buffer, sizeof(buffer));
    while ((rb > 0) && (size < sizeof(buffer))) {
      size += rb;
      rb = p.read(buffer + size, sizeof(buffer) - size);
    }
  }

  // Wait for process termination.
  int retval;
  int exitcode;
  if (!p.wait(4000, &exitcode)) {
    p.terminate();
    p.wait();
    retval = 1;
  }
  else
    retval = (exitcode != 0);

  // Compare results.
  return (retval
          || (size != (sizeof(RESULT) - 1))
          || memcmp(buffer, RESULT, sizeof(RESULT) - 1));
}
