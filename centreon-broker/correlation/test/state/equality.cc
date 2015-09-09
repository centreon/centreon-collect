/*
** Copyright 2011 Centreon
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

#include "com/centreon/broker/correlation/state.hh"

using namespace com::centreon::broker;

/**
 *  Check that service_state can be checked for equality.
 *
 *  @return 0 on success.
 */
int main() {
  // First object.
  correlation::state ss1;
  ss1.ack_time = 6762;
  ss1.current_state = 2;
  ss1.end_time = 7456987;
  ss1.host_id = 21;
  ss1.in_downtime = true;
  ss1.service_id = 42;
  ss1.start_time = 123456789;

  // Second object.
  correlation::state ss2(ss1);

  // Reset first object.
  correlation::state ss3;
  ss3.ack_time = 4787985;
  ss3.current_state = 1;
  ss3.end_time = 5478963;
  ss3.host_id = 983;
  ss3.in_downtime = false;
  ss3.service_id = 211;
  ss3.start_time = 456887;

  // Check.
  return (!(ss1 == ss2)
          || (ss1 == ss3)
          || (ss2 == ss3)
          || !(ss1 == ss1)
          || !(ss2 == ss2)
          || !(ss3 == ss3));
}
