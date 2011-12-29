/*
** Copyright 2011 Merethis
**
** This file is part of Centreon Connector SSH.
**
** Centreon Connector SSH is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector SSH is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector SSH. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include "com/centreon/connector/ssh/orders/parser.hh"
#include "com/centreon/logging/engine.hh"
#include "test/orders/buffer_handle.hh"
#include "test/orders/fake_listener.hh"

using namespace com::centreon::connector::ssh::orders;

#define DATA1 "2\0\00010\0000\0localhost root centreon ls\0\0\0\0"
#define DATA2 "2\0foo\00010\0000\0localhost root centreon ls\0\0\0\0"

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
  bh.write(DATA1, sizeof(DATA1) - 1);
  bh.write(DATA2, sizeof(DATA2) - 1);

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
  if (listnr.get_callbacks().size() != 3)
    retval = 1;
  else {
    fake_listener::callback_info info1, info2, info3;
    std::list<fake_listener::callback_info>::const_iterator it;
    it = listnr.get_callbacks().begin();
    info1 = *(it++);
    info2 = *(it++);
    info3 = *(it++);
    retval |= ((info1.callback != fake_listener::cb_error)
               || (info2.callback != fake_listener::cb_error)
               || (info3.callback != fake_listener::cb_eof));
  }

  // Parser must be empty.
  retval |= !p.get_buffer().empty();

  return (retval);
}
