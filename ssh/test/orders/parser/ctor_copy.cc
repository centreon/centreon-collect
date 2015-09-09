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

#define DATA "765\0merethis\0Centreon is beautiful"

/**
 *  Check that the orders parser can be copy-constructed.
 *
 *  @return 0 on success.
 */
int main() {
  // Initialization.
  com::centreon::logging::engine::load();

  // Data.
  buffer_handle bh;
  bh.write(DATA, sizeof(DATA));

  // Listener.
  fake_listener fl;

  // Base object.
  parser p1;
  p1.listen(&fl);
  p1.read(bh);

  // Copy.
  parser p2(p1);

  // Check.
  int retval ((p1.get_buffer() != std::string(DATA, sizeof(DATA)))
              || (p1.get_listener() != &fl)
              || (p2.get_buffer() != std::string(DATA, sizeof(DATA)))
              || (p2.get_listener() != &fl));

  // Unload.
  com::centreon::logging::engine::unload();

  return (retval);
}
