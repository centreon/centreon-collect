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

using namespace com::centreon::connector::ssh::orders;

/**
 *  Check that the orders parser is properly default constructed.
 *
 *  @return 0 on success.
 */
int main() {
  // Object.
  parser p;

  // Check.
  buffer_handle bh;
  int retval(!p.get_buffer().empty() || p.get_listener() || !p.want_read(bh) ||
             p.want_write(bh));

  return (retval);
}
