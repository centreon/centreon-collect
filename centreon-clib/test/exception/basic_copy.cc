/*
** Copyright 2011 Merethis
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

#include <sstream>
#include <string.h>
#include "com/centreon/exception/basic.hh"

using namespace com::centreon::exception;

/**
 *  Check the basic copy.
 *
 *  @return 0 on success.
 */
int main() {
  static char const message[] = "Centreon Clib";
  const unsigned int line = __LINE__;

  basic ex(__FILE__, __func__, line);
  ex << message;

  std::ostringstream oss;
  oss << "[" << __FILE__ << ":" << line << "(" << __func__ << ")] "
      << message;
  return (strcmp(ex.what(), oss.str().c_str()));
}
