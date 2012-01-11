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

#include <float.h>
#include <math.h>
#include <sstream>
#include <stdlib.h>
#include "com/centreon/misc/stringifier.hh"

using namespace com::centreon::misc;

/**
 *  Check that double are properly written by stringifier.
 *
 *  @param[in] d Double to write and test.
 *
 *  @return true if check succeed.
 */
static bool check_double(double d) {
  stringifier buffer;
  buffer << d;
  char* ptr(NULL);
  double converted(strtod(buffer.data(), &ptr));
  return (ptr
          && !*ptr
          && (fabs(d - converted)
              // Roughly 0.1% error margin.
              <= (fabs(d / 1000) + 2 * DBL_EPSILON)));
}

/**
 *  Check the stringifier insert double.
 *
 *  @return 0 on success.
 */
int main() {
  return (!check_double(DBL_MIN)
          || !check_double(DBL_MAX)
          || !check_double(0.0)
          || !check_double(1.1)
          || !check_double(-1.456657563));
}
