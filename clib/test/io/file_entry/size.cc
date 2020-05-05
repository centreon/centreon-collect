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
#include "com/centreon/io/file_stream.hh"

unsigned int const DATA_SIZE = 42;

using namespace com::centreon;

/**
 *  Check size is valid.
 *
 *  @return EXIT_SUCCESS on success.
 */
int main() {
  int ret(EXIT_FAILURE);

  std::string temp(io::file_stream::temp_path());
  try {
    {
      io::file_stream fs;
      fs.open(temp, "w");
      fs.close();
    }

    io::file_entry entry(temp);
    if (entry.size())
      throw(basic_error() << "invalid file size: not empty");

    {
      std::string data(DATA_SIZE, ' ');
      io::file_stream fs;
      fs.open(temp, "w");
      fs.write(data.c_str(), data.size());
      fs.close();
    }

    if (entry.size())
      throw(basic_error() << "invalid file size: not empty");

    entry.refresh();

    if (entry.size() != DATA_SIZE)
      throw(basic_error() << "invalid file size: is empty");

    ret = EXIT_SUCCESS;
  }
  catch (std::exception const& e) {
    std::cerr << e.what() << std::endl;
  }

  if (io::file_stream::exists(temp))
    io::file_stream::remove(temp);

  return (ret);
}
