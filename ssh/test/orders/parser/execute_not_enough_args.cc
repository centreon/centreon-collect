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

#define DATA01 "2\0\0\0\0"
#define DATA02 "2\00042\0\0\0\0"
#define DATA03 "2\00042\00010\0\0\0\0"
#define DATA04 "2\00042\00010\0000\0\0\0\0"
#define DATA05 "2\00042\00010\0000\0check_by_ssh\0\0\0\0"
#define DATA06 "2\00042\00010\0000\0check_by_ssh -C 'true'\0\0\0\0"
#define DATA07 "2\00042\00010\0000\0check_by_ssh -C 'true' -H\0\0\0\0"
#define DATA08 "2\00042\00010\0000\0check_by_ssh -C 'true' -H localhost -i\0\0\0\0"
#define DATA09 "2\00042\00010\0000\0check_by_ssh -C 'true' -H localhost -l\0\0\0\0"
#define DATA10 "2\00042\00010\0000\0check_by_ssh -C 'true' -H localhost -p\0\0\0\0"
#define DATA11 "2\00042\00010\0000\0check_by_ssh -C 'true' -H localhost -a\0\0\0\0"
#define DATA12 "2\00042\00010\0000\0check_by_ssh -C 'true' -H localhost -t\0\0\0\0"
#define DATA13 "2\00042\00010\0000\0check_by_ssh -C '' -H localhost\0\0\0\0"

/**
 *  Check execute orders parsing.
 *
 *  @return 0 on success.
 */
#include <iostream>
int main() {
  // Initialization.
  com::centreon::logging::engine::load();

  // Create invalid execute order packet.
  buffer_handle bh;
  bh.write(DATA01, sizeof(DATA01) - 1);
  bh.write(DATA02, sizeof(DATA02) - 1);
  bh.write(DATA03, sizeof(DATA03) - 1);
  bh.write(DATA04, sizeof(DATA04) - 1);
  bh.write(DATA05, sizeof(DATA05) - 1);
  bh.write(DATA06, sizeof(DATA06) - 1);
  bh.write(DATA07, sizeof(DATA07) - 1);
  bh.write(DATA08, sizeof(DATA08) - 1);
  bh.write(DATA09, sizeof(DATA09) - 1);
  bh.write(DATA10, sizeof(DATA10) - 1);
  bh.write(DATA11, sizeof(DATA11) - 1);
  bh.write(DATA12, sizeof(DATA12) - 1);
  bh.write(DATA13, sizeof(DATA13) - 1);

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
  if (listnr.get_callbacks().size() != 14)
    retval = 1;
  else {
    fake_listener::callback_info info[13];
    std::list<fake_listener::callback_info>::const_iterator it;
    it = listnr.get_callbacks().begin();
    for (unsigned int i = 0; i < sizeof(info) / sizeof(*info); ++i)
      info[i] = *(it++);
    for (unsigned int i = 0; i < sizeof(info) / sizeof(*info); ++i) {
      std::cout << fake_listener::cb_error << ": " << info[i].callback << std::endl;
      retval |= (info[i].callback != fake_listener::cb_error);
    }
  }

  // Parser must be empty.
  retval |= !p.get_buffer().empty();

  return (retval);
}
