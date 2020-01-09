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

/**
 *  Check that handle eof is properly detected.
 *
 *  @return 0 on success.
 */
int main() {
  // Listener.
  fake_listener listnr;

  // Buffer.
  buffer_handle bh;

  // Parser.
  parser p;
  p.listen(&listnr);
  p.read(bh);

  // Checks.
  int retval(0);

  // Listener must have received eof.
  if (listnr.get_callbacks().size() != 1)
    retval = 1;
  else
    retval |=
        (listnr.get_callbacks().begin()->callback != fake_listener::cb_eof);

  return (retval);
}
