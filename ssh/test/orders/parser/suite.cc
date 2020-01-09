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

#include <ctime>
#include "com/centreon/connector/ssh/orders/parser.hh"
#include "com/centreon/logging/engine.hh"
#include "test/orders/buffer_handle.hh"
#include "test/orders/fake_listener.hh"

using namespace com::centreon::connector::ssh::orders;

#define CMD                                                                    \
  "0\0\0\0\0"                                                                  \
  "2\000147852\0007849\000147852369\0check_by_ssh -H localhost -l root -a "    \
  "password -6 -C ls\0\0\0\0"                                                  \
  "2\00036525825445548787\0002258\00001\0check_by_ssh -H www.merethis.com -l " \
  "centreon -a iswonderful -C \"rm -rf /\"\0\0\0\0"                            \
  "2\00063\0000\00099999999999999999\000check_by_ssh -H www.centreon.com -p "  \
  "2222 -l merethis -a rocks -C \"./check_for_updates on website\"\0\0\0\0"    \
  "4\0\0\0\0"

/**
 *  Classic orders suite: version, executions, quit.
 *
 *  @return 0 on success.
 */
int main() {
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
  {  // Version.
    fake_listener::callback_info version;
    version.callback = fake_listener::cb_version;
    expected.push_back(version);
  }
  {  // First execution order.
    fake_listener::callback_info execute;
    execute.callback = fake_listener::cb_execute;
    execute.cmd_id = 147852;
    execute.timeout = 7849 + time(NULL);
    execute.host = "localhost";
    execute.port = 22;
    execute.user = "root";
    execute.password = "password";
    execute.cmds.push_back("ls");
    execute.skip_stdout = -1;
    execute.skip_stderr = -1;
    execute.is_ipv6 = true;
    expected.push_back(execute);
  }
  {  // Second execution order.
    fake_listener::callback_info execute;
    execute.callback = fake_listener::cb_execute;
    execute.cmd_id = 36525825445548787ull;
    execute.timeout = 2258 + time(NULL);
    execute.host = "www.merethis.com";
    execute.port = 22;
    execute.user = "centreon";
    execute.password = "iswonderful";
    execute.cmds.push_back("rm -rf /");
    execute.skip_stdout = -1;
    execute.skip_stderr = -1;
    execute.is_ipv6 = false;
    expected.push_back(execute);
  }
  {  // Third execution order.
    fake_listener::callback_info execute;
    execute.callback = fake_listener::cb_execute;
    execute.cmd_id = 63;
    execute.timeout = time(NULL);
    execute.host = "www.centreon.com";
    execute.port = 2222;
    execute.user = "merethis";
    execute.password = "rocks";
    execute.cmds.push_back("./check_for_updates on website");
    execute.skip_stdout = -1;
    execute.skip_stderr = -1;
    execute.is_ipv6 = false;
    expected.push_back(execute);
  }
  {  // Quit.
    fake_listener::callback_info quit;
    quit.callback = fake_listener::cb_quit;
    expected.push_back(quit);
  }
  {  // EOF.
    fake_listener::callback_info eof;
    eof.callback = fake_listener::cb_eof;
    expected.push_back(eof);
  }

  // Compare parsed result with expected result.
  int retval((expected != listnr.get_callbacks()) || (!p.get_buffer().empty()));

  return (retval);
}
