/**
* Copyright 2012-2013 Centreon
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* For more information : contact@centreon.com
*/

#include "com/centreon/process.hh"
#include "test/connector/binary.hh"

using namespace com::centreon;

/**
 *  Check that connector exits when stdin is closed.
 *
 *  @return 0 on success.
 */
int main() {
  // Process.
  process p;
  p.enable_stream(process::in, true);
  p.exec(CONNECTOR_SSH_BINARY);
  p.enable_stream(process::in, false);

  // Wait for process termination.
  int retval(1);
  if (!p.wait(5000)) {
    p.terminate();
    p.wait();
  } else
    retval = (p.exit_code() != 0);

  return retval;
}
