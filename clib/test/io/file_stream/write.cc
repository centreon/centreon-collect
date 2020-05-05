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
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/io/file_stream.hh"

using namespace com::centreon;

/**
 *  Check that file_stream can be written to.
 *
 *  @return 0 on success.
 */
int main() {
  // Generate temporary file name.
  char const* tmp_file_name(io::file_stream::temp_path());

  // Open temporary file.
  io::file_stream tmp_file_stream;
  tmp_file_stream.open(tmp_file_name, "w");

  // NULL write.
  try {
    tmp_file_stream.write(NULL, 1);
  }
  catch (exceptions::basic const& e) {
    (void)e;
  }

  // Real write.
  char const* data("some data");
  return (tmp_file_stream.write(data,
                                static_cast<unsigned long>(strlen(data))) == 0);
}
