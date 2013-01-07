/*
** Copyright 2011-2013 Merethis
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
#include "com/centreon/misc/stringifier.hh"

using namespace com::centreon::misc;

/**
 *  Check the stringifier insert int.
 *
 *  @return 0 on success.
 */
int main() {
  stringifier buffer;
  buffer << static_cast<int>(INT_MIN);
  buffer << static_cast<int>(INT_MAX);

  std::ostringstream oss;
  oss << INT_MIN << INT_MAX;
  return (strcmp(buffer.data(), oss.str().c_str()));
}
