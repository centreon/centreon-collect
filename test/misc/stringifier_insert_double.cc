/*
** Copyright 2011-2013 Centreon
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
  return (ptr && !*ptr && (fabs(d - converted)
                           // Roughly 0.1% error margin.
                           <= (fabs(d / 1000) + 2 * DBL_EPSILON)));
}

/**
 *  Check the stringifier insert double.
 *
 *  @return 0 on success.
 */
int main() {
  return (!check_double(DBL_MIN) || !check_double(DBL_MAX) ||
          !check_double(0.0) || !check_double(1.1) ||
          !check_double(-1.456657563));
}
