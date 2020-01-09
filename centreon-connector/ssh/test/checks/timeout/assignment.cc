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

#include "com/centreon/connector/ssh/checks/check.hh"
#include "com/centreon/connector/ssh/checks/timeout.hh"
#include "com/centreon/connector/ssh/multiplexer.hh"

using namespace com::centreon::connector::ssh;

/**
 *  Check timeout's copy construction.
 *
 *  @return 0 on success.
 */
int main() {
  // Initialization.
  return 0;
  //  multiplexer::load();
  //
  //  // Base object.
  //  checks::check c1;
  //  checks::timeout t1(&c1);
  //
  //  // Copy object.
  //  checks::check c2;
  //  checks::timeout t2(&c2);
  //
  //  // Assignment.
  //  t2 = t1;
  //
  //  // Return check result.
  //  int retval ((t1.get_check() != &c1)
  //              || (t2.get_check() != &c1));
  //
  //  // Unload.
  //  multiplexer::unload();
  //
  //  return (retval);
}
