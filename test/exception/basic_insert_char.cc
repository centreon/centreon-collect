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

#include <string.h>
#include <limits.h>
#include "com/centreon/exception/basic.hh"

using namespace com::centreon::exception;

/**
 *  Check the basic insert char.
 *
 *  @return 0 on success.
 */
int main() {
  basic ex;
  ex << static_cast<char>(CHAR_MIN);
  ex << static_cast<char>(CHAR_MAX);

  char ref[] = { CHAR_MIN, CHAR_MAX, 0 };
  return (strcmp(ex.what(), ref));
}
