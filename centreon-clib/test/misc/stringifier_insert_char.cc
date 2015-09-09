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

#include <string.h>
#include <limits.h>
#include "com/centreon/misc/stringifier.hh"

using namespace com::centreon::misc;

/**
 *  Check the stringifier insert char.
 *
 *  @return 0 on success.
 */
int main() {
  stringifier buffer;
  buffer << static_cast<char>(CHAR_MIN);
  buffer << static_cast<char>(CHAR_MAX);

  char ref[] = { CHAR_MIN, CHAR_MAX, 0 };
  return (strcmp(buffer.data(), ref));
}
