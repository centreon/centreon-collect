/*
** Copyright 2012-2013 Centreon
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

#include <cstdlib>
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/neb/custom_variable_status.hh"
#include "com/centreon/broker/neb/internal.hh"

using namespace com::centreon::broker;

/**
 *  Check custom_variable_status's default constructor.
 *
 *  @return EXIT_SUCCESS on success.
 */
int main() {
  // Object.
  neb::custom_variable_status cvar_status;

  // Check.
  return (((cvar_status.source_id != 0)
           || (cvar_status.destination_id != 0)
           || (cvar_status.host_id != 0)
           || (cvar_status.modified != true)
           || (cvar_status.name != "")
           || (cvar_status.service_id != 0)
           || (cvar_status.update_time != 0)
           || (cvar_status.value != "")
           || (cvar_status.type()
               != io::events::data_type<
                                io::events::neb,
                                neb::de_custom_variable_status>::value))
          ? EXIT_FAILURE
          : EXIT_SUCCESS);
}
