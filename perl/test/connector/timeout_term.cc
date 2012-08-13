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

#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include "com/centreon/clib.hh"
#include "com/centreon/process.hh"
#include "test/connector/paths.hh"

using namespace com::centreon;

#define CMD "2\0" \
            "4242\0" \
            "3\0" \
            "123456789\0" \
            CONNECTOR_TEST_DIR "/timeout_term.pl\0\0\0\0"
#define RESULT "3\0" \
               "4242\0" \
               "1\0" \
               "-1\0" \
               " \0" \
               " \0\0\0\0"

/**
 *  Check that connector properly kills timeouting processes.
 *
 *  @return 0 on success.
 */
int main() {
  // Initialization.
  clib::load();

  // Process.
  process p;
  p.enable_stream(process::in, true);
  p.enable_stream(process::out, true);
  p.enable_stream(process::err, false);
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
  p.enable_stream(process::in, false);

  // Read reply.
  std::string output;
  while (true) {
    std::string buffer;
    p.read(buffer);
    if (buffer.empty())
      break ;
    output.append(buffer);
  }

  // Wait for process termination.
  int retval(1);
  if (!p.wait(1000)) {
    p.terminate();
    p.wait();
  }
  else {
    retval = (p.exit_code() != 0);
    std::cout << "connector exit code: " << p.exit_code() << std::endl;
  }

  // Cleanup.
  clib::unload();

  // Data.
  std::cout << "output size: " << output.size() << " (expected "
            << (sizeof (RESULT) - 1) << ")" << std::endl;

  // Compare results.
  return (retval
          || (output.size() != (sizeof (RESULT) - 1))
          || (memcmp(output.c_str(), RESULT, sizeof (RESULT) - 1)));
}
