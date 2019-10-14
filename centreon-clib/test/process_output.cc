/*
** Copyright 2011-2013 Centreon
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
    if (argc != 2 || (strcmp(argv[1], "err") && strcmp(argv[1], "out"))) {
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
        total_write +=
            p.write(buffer_write, sizeof(buffer_write) - total_write);
      if (!strcmp(argv[1], "out"))
        p.read(buffer_read);
      else
        p.read_err(buffer_read);
      total_read += buffer_read.size();
    } while (total_read < sizeof(buffer_write));
    p.enable_stream(process::in, false);
    p.wait();
    if (p.exit_code() != EXIT_SUCCESS)
      throw(basic_error() << "invalid return");
    if (total_write != sizeof(buffer_write))
      throw(basic_error() << "invalid data wirte");
    if (total_write != total_read)
      throw(basic_error() << "invalid data read");
  }
  catch (std::exception const& e) {
    ret = EXIT_FAILURE;
    std::cerr << "error: " << e.what() << std::endl;
  }
  clib::unload();
  return (ret);
}
