/*
** Copyright 2012-2013 Centreon
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
#define CMD2 \
  "\0"       \
  "5\0"      \
  "123456789\0"
#define CMD3 "\0\0\0\0"
#define RESULT "Merethis is wonderful\n"

#define COUNT 300

#define SCRIPT                            \
  "#!/usr/bin/perl\n"                     \
  "\n"                                    \
  "print \"Merethis is wonderful\\n\";\n" \
  "exit 2;\n"

/**
 *  Check that connector can execute multiple scripts.
 *
 *  @return 0 on success.
 */
int main() {
  // Write Perl scripts.
  std::string script_paths[10];
  for (unsigned int i = 0; i < sizeof(script_paths) / sizeof(*script_paths);
       ++i) {
    script_paths[i] = io::file_stream::temp_path();
    write_file(script_paths[i].c_str(), SCRIPT, sizeof(SCRIPT) - 1);
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
    for (unsigned int i = 0; i < COUNT; ++i) {
      oss.write(CMD1, sizeof(CMD1) - 1);
      oss << i + 1;
      oss.write(CMD2, sizeof(CMD2) - 1);
      oss << script_paths[i % (sizeof(script_paths) / sizeof(*script_paths))];
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
  } else
    retval = (p.exit_code() != 0);

  // Remove temporary files.
  for (unsigned int i = 0; i < sizeof(script_paths) / sizeof(*script_paths);
       ++i)
    remove(script_paths[i].c_str());

  unsigned int nb_right_output(0);
  for (size_t pos(0); (pos = output.find(RESULT, pos)) != std::string::npos;
       ++nb_right_output, ++pos)
    ;

  try {
    if (nb_right_output != COUNT)
      throw(basic_error() << "invalid output: size=" << output.size()
                          << ", output=" << replace_null(output));
  } catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    retval = 1;
  }

  // Return check result.
  return retval;
}
