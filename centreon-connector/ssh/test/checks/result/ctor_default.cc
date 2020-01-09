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

/**
 *  Check that result is properly default constructed.
 *
 *  @return 0 on success.
 */
int main() {
  // Object.
  com::centreon::connector::ssh::checks::result r;

  // Check.
  return ((r.get_command_id() != 0) || !r.get_error().empty() ||
          r.get_executed() || (r.get_exit_code() != -1) ||
          !r.get_output().empty());
}
