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

#include <iostream>
#include <string>
#include <string.h>
#include <sys/wait.h>
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
  // Initialization.
  logging::engine::load();
  embedded_perl::load(&argc, &argv, &env);

  // Return value.
  int retval(EXIT_FAILURE);

  // Write simple Perl script.
  std::string script_path(com::centreon::io::file_stream::tmpnam());
  try {
    com::centreon::io::file_stream fs;
    fs.open(script_path.c_str(), "w");
    char const* data("exit 42;\n");
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
  }
  catch (std::exception const& e) {
    std::cerr << e.what() << std::endl;
  }
  catch (...) {
    std::cerr << "unknown error" << std::endl;
  }

  return (retval);
}
