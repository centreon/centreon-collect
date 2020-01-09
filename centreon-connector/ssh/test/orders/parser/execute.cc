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

#include <cstring>
#include <ctime>
#include <iostream>
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
  // Create execution order packet.
  buffer_handle bh;
  char const* str;
  str = "2";  // Order ID.
  bh.write(str, strlen(str) + 1);
  str = "1478523697531598258";
  bh.write(str, strlen(str) + 1);
  str = "4242";  // Timeout.
  bh.write(str, strlen(str) + 1);
  str = "4241";  // Start time.
  bh.write(str, strlen(str) + 1);
  str =
      "check_by_ssh -H localhost -l root -a myverysecretpassword -C \"mycheck "
      "to execute with some args\"";
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
    std::cout << "order ID:   " << fake_listener::cb_execute << std::endl
              << "            " << info1.callback << std::endl
              << "command ID: " << 1478523697531598258ull << std::endl
              << "            " << info1.cmd_id << std::endl
              << "timeout:    " << comparison_timeout << std::endl
              << "            " << info1.timeout << std::endl << "host:       "
              << "localhost" << std::endl << "            " << info1.host
              << std::endl << "user:       "
              << "root" << std::endl << "            " << info1.user
              << std::endl << "password:   "
              << "myverysecretpassword" << std::endl << "            "
              << info1.password << std::endl << "command:    "
              << "mycheck to execute with some args" << std::endl
              << "            " << info1.cmds.front() << std::endl;
    retval |= ((info1.callback != fake_listener::cb_execute) ||
               (info1.cmd_id != 1478523697531598258ull) ||
               ((comparison_timeout - info1.timeout) > 1) ||
               (info1.host != "localhost") || (info1.user != "root") ||
               (info1.password != "myverysecretpassword") ||
               (info1.cmds.size() != 1) ||
               (info1.cmds.front() != "mycheck to execute with some args") ||
               (info2.callback != fake_listener::cb_eof));
  }

  // Parser must be empty.
  retval |= !p.get_buffer().empty();

  return (retval);
}
