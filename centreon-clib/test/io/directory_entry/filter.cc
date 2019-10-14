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
#include "com/centreon/io/directory_entry.hh"

using namespace com::centreon;

/**
 *  Check if filter work.
 *
 *  @return EXIT_SUCCESS on success.
 */
int main() {
  int ret(EXIT_FAILURE);
  try {
    {
      io::directory_entry entry(".");
      std::list<io::file_entry> lst_all(entry.entry_list());
      std::list<io::file_entry> lst_point(entry.entry_list(".*"));
      std::list<io::file_entry> lst_de(entry.entry_list("io_directory_entry*"));

      if (lst_all.size() < lst_point.size() + lst_de.size())
        throw(basic_error() << "invalid result size: list all");
      if (lst_point.size() < 2)
        throw(basic_error() << "invalid result size: list point");
      if (lst_de.size() < 2)
        throw(basic_error() << "invalid result size: list directory entry");
    }

    ret = EXIT_SUCCESS;
  }
  catch (std::exception const& e) {
    std::cerr << e.what() << std::endl;
  }
  return (ret);
}
