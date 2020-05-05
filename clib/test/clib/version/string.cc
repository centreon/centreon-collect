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

#include <iostream>
#include <string.h>
#include "com/centreon/clib/version.hh"

using namespace com::centreon::clib;

/**
 *  Check that the version string returned by the library matches the
 *  header.
 *
 *  @return 0 on success.
 */
int main() {
  // Check.
  int retval(strcmp(version::get_string(), version::string) ||
             strcmp(version::string, CENTREON_CLIB_VERSION_STRING));

  // Message.
  if (retval)
    std::cout << "Version string mismatch" << std::endl << "  library returned "
              << version::get_string() << std::endl << "  header returned  "
              << version::string << std::endl << "  macro returned   "
              << CENTREON_CLIB_VERSION_STRING << std::endl;
  else
    std::cout << "Version string is consistent (" << version::string << ")"
              << std::endl;

  // Return check result.
  return (retval);
}
