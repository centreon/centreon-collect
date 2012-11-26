/*
** Copyright 2011 Merethis
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

#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include "com/centreon/concurrency/thread.hh"
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/io/file_stream.hh"
#include "com/centreon/logging/file.hh"

using namespace com::centreon;
using namespace com::centreon::logging;

static bool check_log_message(
              std::string const& path,
              std::string const& msg) {
  std::ifstream stream(path.c_str());
  char buffer[32 * 1024];
  memset(buffer, 0, sizeof(buffer));
  stream.read(buffer, sizeof(buffer));
  return (buffer == msg);
}

/**
 *  Check if log message works.
 *
 *  @return 0 on success.
 */
int main() {
  static unsigned int const nb_line(1024);
  int retval;

  try {
    char* tmpfile(com::centreon::io::file_stream::temp_path());

    std::ostringstream tmp;
    std::ostringstream tmpref;
    for (unsigned int i(0); i < nb_line; ++i) {
      tmp << i << "\n";
      tmpref << "[" << concurrency::thread::get_current_id()
             << "] " << i << "\n";
    }
    std::string msg(tmp.str());
    std::string ref(tmpref.str());

    {
      file f(tmpfile, false, false, none, true);
      f.log(1, 0, msg.c_str(), msg.size());
    }
    if (!check_log_message(tmpfile, ref))
      throw (basic_error() << "log message failed");

    retval = 0;
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    retval = 1;
  }
  return (retval);
}
