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

#include <iostream>
#include <string.h>
#include <time.h>
#include "com/centreon/connector/ssh/orders/parser.hh"
#include "com/centreon/logging/engine.hh"
#include "test/orders/buffer_handle.hh"
#include "test/orders/fake_listener.hh"

using namespace com::centreon::connector::ssh::orders;

/**
 *  Check that version orders are properly parsed.
 *
 *  @return 0 on success.
 */
int main() {
  // Initialization.
  com::centreon::logging::engine::load();

  // Create execution order packet.
  buffer_handle bh;
  char const* str;
  str = "2"; // Order ID.
  bh.write(str, strlen(str) + 1);
  str = "1478523697531598258";
  bh.write(str, strlen(str) + 1);
  str = "4242"; // Timeout.
  bh.write(str, strlen(str) + 1);
  str = "0"; // Start time.
  bh.write(str, strlen(str) + 1);
  str = "localhost root myverysecretpassword mycheck to execute with some args";
  bh.write(str, strlen(str));
  bh.write("\0\0\0\0", 4);

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

  // Listener must have received execute and eof.
  if (listnr.get_callbacks().size() != 2)
    retval = 1;
  else {
    fake_listener::callback_info info1, info2;
    time_t comparison_timeout(time(NULL) + 4242);
    info1 = *listnr.get_callbacks().begin();
    info2 = *++listnr.get_callbacks().begin();
    std::cout << "order ID:   " << fake_listener::cb_execute
              << std::endl
              << "            " << info1.callback << std::endl
              << "command ID: " << 1478523697531598258ull << std::endl
              << "            " << info1.cmd_id << std::endl
              << "timeout:    " << comparison_timeout << std::endl
              << "            " << info1.timeout << std::endl
              << "host:       " << "localhost" << std::endl
              << "            " << info1.host << std::endl
              << "user:       " << "root" << std::endl
              << "            " << info1.user << std::endl
              << "password:   " << "myverysecretpassword" << std::endl
              << "            " << info1.password << std::endl
              << "command:    " << "mycheck to execute with some args"
              << std::endl
              << "            " << info1.cmd << std::endl;
    retval |= ((info1.callback != fake_listener::cb_execute)
               || (info1.cmd_id != 1478523697531598258ull)
               || ((comparison_timeout - info1.timeout) > 1)
               || (info1.host != "localhost")
               || (info1.user != "root")
               || (info1.password != "myverysecretpassword")
               || (info1.cmd != "mycheck to execute with some args")
               || (info2.callback != fake_listener::cb_eof));
  }

  // Parser must be empty.
  retval |= !p.get_buffer().empty();

  return (retval);
}
