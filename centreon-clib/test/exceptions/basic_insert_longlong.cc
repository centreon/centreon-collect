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
#include <limits.h>
#include "com/centreon/exceptions/basic.hh"

using namespace com::centreon::exceptions;

/**
 *  Check the basic insert long long.
 *
 *  @return 0 on success.
 */
int main() {
  basic ex;
  ex << static_cast<long long>(LLONG_MIN);
  ex << static_cast<long long>(LLONG_MAX);

  std::ostringstream oss;
  oss << LLONG_MIN << LLONG_MAX;
  return (strcmp(ex.what(), oss.str().c_str()));
}
