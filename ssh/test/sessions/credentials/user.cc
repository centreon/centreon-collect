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

#include "com/centreon/connector/ssh/sessions/credentials.hh"

#define USER1 "root"
#define USER2 "Centreon"
#define USER3 "Merethis"

/**
 *  Check credentials' user property.
 *
 *  @return 0 on success.
 */
int main() {
  // Object.
  com::centreon::connector::ssh::sessions::credentials creds;

  // Checks.
  int retval(0);
  creds.set_user(USER1);
  for (unsigned int i = 0; i < 100; ++i)
    retval |= (creds.get_user() != USER1);
  creds.set_user(USER2);
  retval |= (creds.get_user() != USER2);
  creds.set_user(USER3);
  for (unsigned int i = 0; i < 1000; ++i)
    retval |= (creds.get_user() != USER3);

  // Return check result.
  return (retval);
}
