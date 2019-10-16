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

#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/io/file_stream.hh"
#include "com/centreon/logging/file.hh"

using namespace com::centreon;
using namespace com::centreon::logging;

static bool check_log_message(std::string const& path, std::string const& msg) {
  std::ifstream stream(path.c_str());
  char buffer[32 * 1024];
  memset(buffer, 0, sizeof(buffer));
  stream.read(buffer, sizeof(buffer));
  return buffer == msg;
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
      tmpref << "[" << std::this_thread::get_id() << "] " << i
             << "\n";
    }
    std::string msg(tmp.str());
    std::string ref(tmpref.str());

    {
      file f(tmpfile, false, false, none, true);
      f.log(1, 0, msg.c_str(), msg.size());
    }
    if (!check_log_message(tmpfile, ref))
      throw(basic_error() << "log message failed");

    retval = 0;
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    retval = 1;
  }
  return retval;
}
