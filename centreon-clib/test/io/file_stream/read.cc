/*
** Copyright 2012 Merethis
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

#include <stdio.h>
#include <string.h>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/io/file_stream.hh"

using namespace com::centreon;

/**
 *  Check that file_stream can be read from.
 *
 *  @return 0 on success.
 */
int main() {
  // Generate temporary file name.
  char const* tmp_file_name(tmpnam(static_cast<char*>(NULL)));

  // Open temporary file.
  io::file_stream tmp_file_stream;
  tmp_file_stream.open(tmp_file_name, "w");

  // Return value.
  int retval(0);

  // Write.
  char const* data("some data");
  if (tmp_file_stream.write(data, strlen(data)) == 0)
    retval = 1;
  else {
    // NULL-read.
    try {
      tmp_file_stream.read(NULL, 1);
      retval = 1;
    }
    catch (exceptions::basic const& e) {
      (void)e;
    }
    // Real read.
    char buffer[1024];
    tmp_file_stream.close();
    tmp_file_stream.open(tmp_file_name, "r");
    retval |= (tmp_file_stream.read(buffer, sizeof(buffer)) == 0);
  }

  return (retval);
}
