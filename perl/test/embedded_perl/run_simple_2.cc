/*
** Copyright 2012-2014 Centreon
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

#include <sys/wait.h>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include "com/centreon/connector/perl/embedded_perl.hh"
#include "com/centreon/io/file_stream.hh"
#include "com/centreon/logging/engine.hh"

using namespace com::centreon;
using namespace com::centreon::connector::perl;

/**
 *  Try executing a very simple Perl script.
 *
 *  @param[in] argc Argument count.
 *  @param[in] argv Argument values.
 *  @param[in] env  Process environment.
 *
 *  @return 0 on success.
 */
int main(int argc, char* argv[], char* env[]) {
  embedded_perl::load(&argc, &argv, &env);

  // Return value.
  int retval(EXIT_FAILURE);

  // Write simple Perl script.
  std::string script_path(com::centreon::io::file_stream::temp_path());
  try {
    com::centreon::io::file_stream fs;
    fs.open(script_path.c_str(), "w");
    char const* data(
        "my $x;\n"
        "my $y = 40;\n"
        "$x = 2;\n"
        "exit $x + $y;\n");
    unsigned int size(strlen(data));
    unsigned int rb(1);
    do {
      rb = fs.write(data, size);
      size -= rb;
      data += rb;
    } while ((rb > 0) && (size > 0));

    // Compile and execute script.
    int fds[3];
    pid_t child(embedded_perl::instance().run(script_path, fds));

    // Wait for child termination.
    int status;
    if (waitpid(child, &status, 0) == child)
      retval = !(WIFEXITED(status) && (WEXITSTATUS(status) == 42));
    if (retval)
      std::cerr << "execute script failed: exit_code=" << retval
                << ", status=" << WEXITSTATUS(status) << std::endl;

    // Remove temporary file.
    remove(script_path.c_str());
  } catch (std::exception const& e) {
    std::cerr << e.what() << std::endl;
  } catch (...) {
    std::cerr << "unknown error" << std::endl;
  }

  // Unload.
  embedded_perl::unload();

  return retval;
}
