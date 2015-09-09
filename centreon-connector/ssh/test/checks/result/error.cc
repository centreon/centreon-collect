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

#include "com/centreon/connector/ssh/checks/result.hh"

#define STR1 "this is the first string"
#define STR2 "this string might be longer"
#define STR3 "this is the last string that makes Centreon SSH Connector rocks !"

/**
 *  Check result's error property.
 *
 *  @return 0 on success.
 */
int main() {
  // Object.
  com::centreon::connector::ssh::checks::result r;

  // Checks.
  int retval(0);
  r.set_error(STR1);
  for (unsigned int i = 0; i < 100; ++i)
    retval |= (r.get_error() != STR1);
  r.set_error(STR2);
  retval |= (r.get_error() != STR2);
  r.set_error(STR3);
  for (unsigned int i = 0; i < 10000; ++i)
    retval |= (r.get_error() != STR3);

  // Return check result.
  return (retval);
}
