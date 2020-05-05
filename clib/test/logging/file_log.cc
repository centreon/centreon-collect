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

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/io/file_stream.hh"
#include "com/centreon/logging/file.hh"

using namespace com::centreon::logging;

static bool check_log_message(std::string const& path, std::string const& msg) {
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
      throw(basic_error() << "log message failed");

    {
      FILE* out(NULL);
      if (!(out = fopen(tmp, "w")))
        throw(basic_error() << "failed to open file \"" << tmp
                            << "\":" << strerror(errno));
      file f(out, false, false, none, false);
      f.log(1, 0, msg, sizeof(msg));
    }
    if (!check_log_message(tmp, msg))
      throw(basic_error() << "log message failed");
    retval = 0;
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    retval = 1;
  }
  return (retval);
}
