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

#define DATA1 "2\0\0\0\0"
#define DATA2 "2\00042\0\0\0\0"
#define DATA3 "2\00042\00010\0\0\0\0"
#define DATA4 "2\00042\00010\0000\0\0\0\0"
#define DATA5 "2\00042\00010\0000\0localhost\0\0\0\0"
#define DATA6 "2\00042\00010\0000\0localhost root\0\0\0\0"
#define DATA7 "2\00042\00010\0000\0localhost root centreon\0\0\0\0"
#define DATA8 "2\00042\00010\0000\0localhost root centreon \0\0\0\0"

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
  bh.write(DATA3, sizeof(DATA3) - 1);
  bh.write(DATA4, sizeof(DATA4) - 1);
  bh.write(DATA5, sizeof(DATA5) - 1);
  bh.write(DATA6, sizeof(DATA6) - 1);
  bh.write(DATA7, sizeof(DATA7) - 1);
  bh.write(DATA8, sizeof(DATA8) - 1);

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
  if (listnr.get_callbacks().size() != 9)
    retval = 1;
  else {
    fake_listener::callback_info
      info1, info2, info3, info4, info5, info6, info7, info8, info9;
    std::list<fake_listener::callback_info>::const_iterator it;
    it = listnr.get_callbacks().begin();
    info1 = *(it++);
    info2 = *(it++);
    info3 = *(it++);
    info4 = *(it++);
    info5 = *(it++);
    info6 = *(it++);
    info7 = *(it++);
    info8 = *(it++);
    info9 = *(it++);
    retval |= ((info1.callback != fake_listener::cb_error)
               || (info2.callback != fake_listener::cb_error)
               || (info3.callback != fake_listener::cb_error)
               || (info4.callback != fake_listener::cb_error)
               || (info5.callback != fake_listener::cb_error)
               || (info6.callback != fake_listener::cb_error)
               || (info7.callback != fake_listener::cb_error)
               || (info8.callback != fake_listener::cb_error)
               || (info9.callback != fake_listener::cb_eof));
  }

  // Parser must be empty.
  retval |= !p.get_buffer().empty();

  return (retval);
}
