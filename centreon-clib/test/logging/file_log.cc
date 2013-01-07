/*
** Copyright 2011-2013 Merethis
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

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/io/file_stream.hh"
#include "com/centreon/logging/file.hh"

using namespace com::centreon::logging;

static bool check_log_message(
              std::string const& path,
              std::string const& msg) {
  std::ifstream stream(path.c_str());
  char buffer[1024];
  stream.get(buffer, sizeof(buffer));
  return (buffer == msg);
}

/**
 *  Check file constructor.
 *
 *  @return 0 on success.
 */
int main() {
  static char msg[] = "Centreon Clib test";
  int retval;

  try {
    char* tmp(com::centreon::io::file_stream::temp_path());

    {
      file f(tmp, false, false, none, false);
      f.log(1, 0, msg, sizeof(msg));
    }
    if (!check_log_message(tmp, msg))
      throw (basic_error() << "log message failed");

    {
      FILE* out(NULL);
      if (!(out = fopen(tmp, "w")))
        throw (basic_error() << "failed to open file \"" << tmp << "\":"
               << strerror(errno));
      file f(out, false, false, none, false);
      f.log(1, 0, msg, sizeof(msg));
    }
    if (!check_log_message(tmp, msg))
      throw (basic_error() << "log message failed");
    retval = 0;
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    retval = 1;
  }
  return (retval);
}
