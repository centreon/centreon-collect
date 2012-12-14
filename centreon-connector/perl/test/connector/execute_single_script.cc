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

#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include "com/centreon/clib.hh"
#include "com/centreon/io/file_stream.hh"
#include "com/centreon/process.hh"
#include "com/centreon/exceptions/basic.hh"
#include "test/connector/misc.hh"
#include "test/connector/paths.hh"

using namespace com::centreon;

#define CMD1 "2\0" \
             "4242\0" \
             "5\0" \
             "123456789\0"
#define CMD2 "\0\0\0\0"
#define RESULT "3\0" \
               "4242\0" \
               "1\0" \
               "0\0" \
               " \0" \
               "Merethis is wonderful\n\0\0\0\0"

/**
 *  Check that connector can execute a script.
 *
 *  @return 0 on success.
 */
int main() {
  clib::load();
  // Write Perl script.
  std::string script_path(io::file_stream::temp_path());
  write_file(
    script_path.c_str(),
    "#!/usr/bin/perl\n" \
    "\n" \
    "print \"Merethis is wonderful\\n\";\n" \
    "exit 0;\n");

  // Process.
  process p;
  p.enable_stream(process::in, true);
  p.enable_stream(process::out, true);
  p.exec(CONNECTOR_PERL_BINARY);

  // Write command.
  std::ostringstream oss;
  oss.write(CMD1, sizeof(CMD1) - 1);
  oss << script_path;
  oss.write(CMD2, sizeof(CMD2) - 1);
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
      break;
    output.append(buffer);
  }

  // Wait for process termination.
  int retval(1);
  if (!p.wait(5000)) {
    p.terminate();
    p.wait();
  }
  else
    retval = (p.exit_code() != 0);

  // Remove temporary files.
  remove(script_path.c_str());

  clib::unload();

  try {
    if (retval)
      throw (basic_error() << "invalid return code: " << retval);
    if (output.size() != (sizeof(RESULT) - 1))
      throw (basic_error()
             << "invalid output size: " << output.size()
             << ", output: " << output);
    if (memcmp(output.c_str(), RESULT, sizeof(RESULT) - 1))
      throw (basic_error() << "invalid output: " << output);
  }
  catch (std::exception const& e) {
    retval = 1;
    std::cerr << "error: " << e.what() << std::endl;
  }

  return (retval);
}
