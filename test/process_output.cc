/*
** Copyright 2011-2012 Merethis
**
** This file is part of Centreon Clib.
**
** Centreon Clib is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Clib is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Clib. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include "com/centreon/clib.hh"
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/process.hh"

using namespace com::centreon;

/**
 *  Check class process (read stdout/stderr).
 *
 *  @return EXIT_SUCCESS on success.
 */
int main(int argc, char** argv) {
  int ret(EXIT_SUCCESS);
  clib::load();
  try {
    if (argc != 2
        || (strcmp(argv[1], "err") && strcmp(argv[1], "out"))) {
      std::cerr << "usage: " << argv[0] << " err|out" << std::endl;
      return (EXIT_FAILURE);
    }

    std::string cmd("./bin_test_process_output check_output ");
    cmd += argv[1];

    process p;
    p.exec(cmd);
    char buffer_write[16 * 1024];
    std::string buffer_read;
    for (unsigned int i(0); i < sizeof(buffer_write); ++i)
      buffer_write[i] = static_cast<char>(i);
    unsigned int total_read(0);
    unsigned int total_write(0);
    do {
      if (total_write < sizeof(buffer_write))
        total_write += p.write(
                         buffer_write,
                         sizeof(buffer_write) - total_write);
      if (!strcmp(argv[1], "out"))
        p.read(buffer_read);
      else
        p.read_err(buffer_read);
      total_read += buffer_read.size();
    } while (total_read < sizeof(buffer_write));
    p.enable_stream(process::in, false);
    p.wait();
    if (p.exit_code() != EXIT_SUCCESS)
      throw (basic_error() << "invalid return");
    if (total_write != sizeof(buffer_write))
      throw (basic_error() << "invalid data wirte");
    if (total_write != total_read)
      throw (basic_error() << "invalid data read");
  }
  catch (std::exception const& e) {
    ret = EXIT_FAILURE;
    std::cerr << "error: " << e.what() << std::endl;
  }
  clib::unload();
  return (ret);
}
