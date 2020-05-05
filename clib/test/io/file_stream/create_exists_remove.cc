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

#include <cstdlib>
#include <cstring>
#include <iostream>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/io/file_stream.hh"

using namespace com::centreon;

/**
 *  Check that file can be created, checked for and removed properly.
 *
 *  @return EXIT_SUCCESS on success.
 */
int main() {
  // Return value.
  int retval(EXIT_FAILURE);

  try {
    // Temporary path.
    char* path(io::file_stream::temp_path());

    // Remove old file.
    io::file_stream::remove(path);

    // File must not exists.
    if (io::file_stream::exists(path))
      throw(basic_error() << "file '" << path
                          << "' exists whereas it should not");

    // Create file.
    {
      io::file_stream fs;
      fs.open(path, "w");
      fs.close();
    }

    // File must exists.
    if (!io::file_stream::exists(path))
      throw(basic_error() << "file '" << path
                          << "' does not exist whereas it should");

    // Remove file.

    // File must not exists.
    if (!io::file_stream::remove(path) || io::file_stream::exists(path))
      throw(basic_error() << "file '" << path
                          << "' exists whereas it should not");

    // Success.
    retval = EXIT_SUCCESS;
  }
  catch (std::exception const& e) {
    std::cout << e.what() << std::endl;
  }
  return (retval);
}
