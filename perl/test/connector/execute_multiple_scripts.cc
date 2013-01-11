/*
** Copyright 2012-2013 Merethis
**
** This file is part of Centreon Perl Connector.
**
** Centreon Perl Connector is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Perl Connector is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Perl Connector. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <cstdio>
#include <iostream>
#include <list>
#include <sstream>
#include "com/centreon/clib.hh"
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/io/file_stream.hh"
#include "com/centreon/process.hh"
#include "test/connector/misc.hh"
#include "test/connector/paths.hh"

using namespace com::centreon;

#define CMD1 "2\0"
#define CMD2 "\0" \
             "5\0" \
             "123456789\0"
#define CMD3 "\0\0\0\0"
#define RESULT "Merethis is wonderful\n"

#define COUNT 300

#define SCRIPT "#!/usr/bin/perl\n" \
               "\n" \
               "print \"Merethis is wonderful\\n\";\n" \
               "exit 2;\n"


/**
 *  Check that connector can execute multiple scripts.
 *
 *  @return 0 on success.
 */
int main() {
  clib::load();
  // Write Perl scripts.
  std::string script_paths[10];
  for (unsigned int i = 0;
       i < sizeof(script_paths) / sizeof(*script_paths);
       ++i) {
    script_paths[i] = io::file_stream::temp_path();
    write_file(
      script_paths[i].c_str(),
      SCRIPT,
      sizeof(SCRIPT) - 1);
  }

  // Process.
  process p;
  p.enable_stream(process::in, true);
  p.enable_stream(process::out, true);
  p.exec(CONNECTOR_PERL_BINARY);

  // Generate command string.
  std::string cmd;
  {
    std::ostringstream oss;
    for (unsigned int i = 0;
         i < COUNT;
         ++i) {
      oss.write(CMD1, sizeof(CMD1) - 1);
      oss << i + 1;
      oss.write(CMD2, sizeof(CMD2) - 1);
      oss << script_paths
        [i % (sizeof(script_paths) / sizeof(*script_paths))];
      oss.write(CMD3, sizeof(CMD3) - 1);
    }
    cmd = oss.str();
  }
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
  for (unsigned int i = 0;
       i < sizeof(script_paths) / sizeof(*script_paths);
       ++i)
    remove(script_paths[i].c_str());

  unsigned int nb_right_output(0);
  for (size_t pos(0);
       (pos = output.find(RESULT, pos)) != std::string::npos;
       ++nb_right_output, ++pos)
    ;

  try {
    if (nb_right_output != COUNT)
      throw (basic_error()
             << "invalid output: size=" << output.size()
             << ", output=" << replace_null(output));
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    retval = 1;
  }

  clib::unload();
  // Return check result.
  return (retval);
}
