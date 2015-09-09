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

#define DATA "2\00042\00010\0foo\0localhost root centreon ls\0\0\0\0"

/**
 *  Check execute orders parsing.
 *
 *  @return 0 on success.
 */
int main() {
  // Initialization.
  com::centreon::logging::engine::load();

  // Create invalid execute order packet.
  buffer_handle bh;
  bh.write(DATA, sizeof(DATA) - 1);

  // Listener.
  fake_listener listnr;

  // Parser.
  parser p;
  p.listen(&listnr);
  while (!bh.empty())
    p.read(bh);
  p.read(bh);

  // Checks.
  int retval(0);

  // Listener must have received errors and eof.
  if (listnr.get_callbacks().size() != 2)
    retval = 1;
  else {
    fake_listener::callback_info info1, info2;
    std::list<fake_listener::callback_info>::const_iterator it;
    it = listnr.get_callbacks().begin();
    info1 = *(it++);
    info2 = *(it++);
    retval |= ((info1.callback != fake_listener::cb_error)
               || (info2.callback != fake_listener::cb_eof));
  }

  // Parser must be empty.
  retval |= !p.get_buffer().empty();

  // Unload.
  com::centreon::logging::engine::unload();

  return (retval);
}
