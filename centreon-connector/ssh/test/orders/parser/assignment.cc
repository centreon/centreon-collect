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

#include "com/centreon/connector/ssh/orders/parser.hh"
#include "com/centreon/logging/engine.hh"
#include "test/orders/buffer_handle.hh"
#include "test/orders/fake_listener.hh"

using namespace com::centreon::connector::ssh::orders;

#define DATA1 "765\0merethis\0Centreon is beautiful"
#define DATA2 "42\0My Centre\0n is fantastic"

/**
 *  Check that the orders parser can be copied.
 *
 *  @return 0 on success.
 */
int main() {
  // Initialization.
  com::centreon::logging::engine::load();

  // Data.
  buffer_handle bh1;
  bh1.write(DATA1, sizeof(DATA1));
  buffer_handle bh2;
  bh2.write(DATA2, sizeof(DATA2));

  // Listeners.
  fake_listener fl1;
  fake_listener fl2;

  // Base object.
  parser p1;
  p1.listen(&fl1);
  p1.read(bh1);

  // Second object.
  parser p2;
  p2.listen(&fl2);
  p2.read(bh2);

  // Copy.
  p2 = p1;

  // Check.
  int retval ((p1.get_buffer() != std::string(DATA1, sizeof(DATA1)))
              || (p1.get_listener() != &fl1)
              || (p2.get_buffer() != std::string(DATA1, sizeof(DATA1)))
              || (p2.get_listener() != &fl1));

  // Unload.
  com::centreon::logging::engine::unload();

  return (retval);
}
