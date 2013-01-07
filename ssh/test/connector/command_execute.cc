/*
** Copyright 2012-2013 Merethis
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

#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include "com/centreon/clib.hh"
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/process.hh"
#include "test/connector/binary.hh"

using namespace com::centreon;

#define CMD1 "2\0" \
             "4242\0" \
             "5\0" \
             "123456789\0" \
             "check_by_ssh " \
             "-H localhost " \
             " -C 'echo Merethis is wonderful'\0\0\0\0"
#define RESULT "3\0" \
               "4242\0" \
               "1\0" \
               "0\0" \
               " \0" \
               "Merethis is wonderful\n\0\0\0\0"

/**
 *  Replace null char by string "\0".
 *
 *  @param[in, out] str  The string to replace.
 *
 *  @return The replace string.
 */
std::string& replace_null(std::string& str) {
  size_t pos(0);
  while ((pos = str.find('\0', pos)) != std::string::npos)
    str.replace(pos++, 1, "\\0");
  return (str);
}

/**
 *  Check that connector respond properly to version command.
 *
 *  @return 0 on success.
 */
int main() {
  clib::load();
  // Process.
  process p;
  p.enable_stream(process::in, true);
  p.enable_stream(process::out, true);
  p.exec(CONNECTOR_SSH_BINARY);

  // Write command.
  std::ostringstream oss;
  oss.write(CMD1, sizeof(CMD1) - 1);
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

  clib::unload();

  try {
    if (retval)
      throw (basic_error() << "invalid return code: " << retval);
    if (output.size() != (sizeof(RESULT) - 1)
        || memcmp(output.c_str(), RESULT, sizeof(RESULT) - 1))
      throw (basic_error()
             << "invalid output: size=" << output.size()
             << ", output=" << replace_null(output));
  }
  catch (std::exception const& e) {
    retval = 1;
    std::cerr << "error: " << e.what() << std::endl;
  }

  return (retval);
}
