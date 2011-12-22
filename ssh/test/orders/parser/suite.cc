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

#include <time.h>
#include "com/centreon/connector/ssh/orders/parser.hh"
#include "com/centreon/logging/engine.hh"
#include "test/orders/buffer_handle.hh"
#include "test/orders/fake_listener.hh"

using namespace com::centreon::connector::ssh::orders;

#define CMD "0\0\0\0\0" \
            "2\000147852\0007849\000147852369\0localhost root password ls\0\0\0\0" \
            "2\00036525825445548787\0002258\0000\0www.merethis.com centreon iswonderful rm -rf /\0\0\0\0" \
            "2\00063\0000\00099999999999999999\000www.centreon.com merethis rocks ./check_for_updates on website\0\0\0\0" \
            "4\0\0\0\0"

/**
 *  Classic orders suite: version, executions, quit.
 *
 *  @return 0 on success.
 */
int main() {
  // Initialization.
  com::centreon::logging::engine::load();

  // Listener.
  fake_listener listnr;

  // Buffer.
  buffer_handle bh;
  bh.write(CMD, sizeof(CMD) - 1);

  // Parser.
  parser p;
  p.listen(&listnr);
  while (!bh.empty())
    p.read(bh);
  p.read(bh);

  // Checks.
  std::list<fake_listener::callback_info> expected;
  { // Version.
    fake_listener::callback_info version;
    version.callback = fake_listener::cb_version;
    expected.push_back(version);
  }
  { // First execution order.
    fake_listener::callback_info execute;
    execute.callback = fake_listener::cb_execute;
    execute.cmd_id = 147852;
    execute.timeout = 7849 + time(NULL);
    execute.host = "localhost";
    execute.user = "root";
    execute.password = "password";
    execute.cmd= "ls";
    expected.push_back(execute);
  }
  { // Second execution order.
    fake_listener::callback_info execute;
    execute.callback = fake_listener::cb_execute;
    execute.cmd_id = 36525825445548787ull;
    execute.timeout = 2258 + time(NULL);
    execute.host = "www.merethis.com";
    execute.user = "centreon";
    execute.password = "iswonderful";
    execute.cmd= "rm -rf /";
    expected.push_back(execute);
  }
  { // Third execution order.
    fake_listener::callback_info execute;
    execute.callback = fake_listener::cb_execute;
    execute.cmd_id = 63;
    execute.timeout = time(NULL);
    execute.host = "www.centreon.com";
    execute.user = "merethis";
    execute.password = "rocks";
    execute.cmd= "./check_for_updates on website";
    expected.push_back(execute);
  }
  { // Quit.
    fake_listener::callback_info quit;
    quit.callback = fake_listener::cb_quit;
    expected.push_back(quit);
  }
  { // EOF.
    fake_listener::callback_info eof;
    eof.callback = fake_listener::cb_eof;
    expected.push_back(eof);
  }

  // Compare parsed result with expected result.
  return ((expected != listnr.get_callbacks())
          || (!p.get_buffer().empty()));
}
