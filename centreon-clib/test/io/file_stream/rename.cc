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
#include <cstring>
#include <iostream>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/io/file_stream.hh"

using namespace com::centreon;

/**
 *  Check that file_stream can be rename file.
 *
 *  @return 0 on success.
 */
int main() {
  int ret(1);

  // Generate temporary file name.
  char const* file_name("./rename_file_stream.test");
  char const* new_file_name("./new_rename_file_stream.test");
  char data[] = "some data";

  try {
    // Write data.
    {
      // Open file.
      io::file_stream file(NULL, true);
      file.open(file_name, "w");

      // Write.
      if (!file.write(data, sizeof(data)))
        throw(basic_error() << "write data failed");
    }

    // Rename file.
    if (!io::file_stream::rename(file_name, new_file_name))
      throw(basic_error() << "rename failed");

    // Read data.
    {
      // Open file.
      io::file_stream file(NULL, true);
      file.open(new_file_name, "r");

      // Read.
      char buffer[64];
      if (file.read(buffer, sizeof(buffer)) != sizeof(data))
        throw(basic_error() << "read failed");
      if (strncmp(buffer, data, sizeof(data)))
        throw(basic_error() << "invalid data");
    }
    ret = 0;
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
  }
  catch (...) {
    std::cerr << "error: catch all" << std::endl;
  }

  // Remove temporary file.
  io::file_stream::remove(file_name);
  io::file_stream::remove(new_file_name);
  return (ret);
}
