/*
** Copyright 2012 Merethis
**
** This file is part of Centreon Connector Perl.
**
** Centreon Connector Perl is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector Perl is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector Perl. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <string.h>
#include "com/centreon/exceptions/basic.hh"
#include "test/connector/misc.hh"

using namespace com::centreon;

/**
 *  Write a file.
 *
 *  @param[in] filename Path to the file.
 *  @param[in] content  File content
 *  @param[in] size     Content size.
 */
void write_file(
       char const* filename,
       char const* content,
       unsigned int size) {
  // Check size.
  if (!size)
    size = strlen(content);

  // Open file.
  FILE* f(fopen(filename, "w"));
  if (!f)
    throw (basic_error() << "could not open file " << filename);

  // Write content.
  while (size > 0) {
    size_t wb(fwrite(content, sizeof(*content), size, f));
    if (ferror(f)) {
      fclose(f);
      throw (basic_error() << "error while writing file " << filename);
    }
    size -= wb;
  }

  // Close handle.
  fclose(f);

  return ;
}
