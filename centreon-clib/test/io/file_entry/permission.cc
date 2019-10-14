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
#include <iostream>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/io/file_entry.hh"

using namespace com::centreon;

/**
 *  Check permission is valid.
 *
 *  @return EXIT_SUCCESS on success.
 */
int main(int argc, char* argv[]) {
  (void)argc;

  int ret(EXIT_FAILURE);
  try {
    io::file_entry entry(argv[0]);
    if (entry.is_directory())
      throw(basic_error() << "permission failed: is not a directory");
    if (entry.is_link())
      throw(basic_error() << "permission failed: is not a link");
    if (!entry.is_regular())
      throw(basic_error() << "permission failed: is a regular file");

    ret = EXIT_SUCCESS;
  }
  catch (std::exception const& e) {
    std::cerr << e.what() << std::endl;
  }
  return (ret);
}
