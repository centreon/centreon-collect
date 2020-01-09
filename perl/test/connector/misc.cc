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

#include "test/connector/misc.hh"
#include <cstdio>
#include <cstring>
#include <string>
#include "com/centreon/exceptions/basic.hh"

using namespace com::centreon;

/**
 *  Replace null char by string "\0".
 *
 *  @param[in, out] str  The string to replace.
 *
 *  @return The replace string.
 */
std::string& replace_null(std::string& str) {
  size_t pos(0);
  while ((pos = str.find('\0', pos)) != std::string::npos)
    str.replace(pos++, 1, "\\0");
  return (str);
}

/**
 *  Write a file.
 *
 *  @param[in] filename Path to the file.
 *  @param[in] content  File content
 *  @param[in] size     Content size.
 */
void write_file(char const* filename, char const* content, unsigned int size) {
  // Check size.
  if (!size)
    size = strlen(content);

  // Open file.
  FILE* f(fopen(filename, "w"));
  if (!f)
    throw(basic_error() << "could not open file " << filename);

  // Write content.
  while (size > 0) {
    size_t wb(fwrite(content, sizeof(*content), size, f));
    if (ferror(f)) {
      fclose(f);
      throw(basic_error() << "error while writing file " << filename);
    }
    size -= wb;
  }

  // Close handle.
  fclose(f);

  return;
}
