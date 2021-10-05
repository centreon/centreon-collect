/*
 * Copyright 2020 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */

#include <gtest/gtest.h>

#include "buffer_handle.hh"
#include "com/centreon/connector/ssh/orders/parser.hh"
#include "fake_listener.hh"

using namespace com::centreon::connector::ssh::orders;

const char ExecuteInvalidTimeout_DATA[] =
    "2\00042\000foo\0000\0localhost root centreon ls\0\0\0\0";
const char ExecuteInvalidTimeout_DATA2[] =
    "2\0\00010\0000\0localhost root centreon ls\0\0\0\0";
const char ExecuteInvalidTimeout_DATA3[] =
    "2\0foo\00010\0000\0localhost root centreon ls\0\0\0\0";

TEST(SSHOrders, Close) {
  // Listener.
  fake_listener listnr;

  // Buffer.
  buffer_handle bh;

  // Parser.
  parser p;
  p.listen(&listnr);
  p.read(bh);

  // Listener must have received eof.
  ASSERT_EQ(listnr.get_callbacks().size(), 1);
  ASSERT_EQ(listnr.get_callbacks().begin()->callback, fake_listener::cb_eof);
}


TEST(SSHOrders, CtorDefault) {
  // Object.
  parser p;

  // Check.
  buffer_handle bh;
  ASSERT_TRUE(p.get_buffer().empty());
  ASSERT_FALSE(p.get_listener());
  ASSERT_TRUE(p.want_read(bh));
  ASSERT_FALSE(p.want_write(bh));
}

TEST(SSHOrders, Eof) {
  // Listener.
  fake_listener listnr;

  // Buffer.
  buffer_handle bh;

  // Parser.
  parser p;
  p.listen(&listnr);
  p.read(bh);

  // Listener must have received eof.
  ASSERT_EQ(listnr.get_callbacks().size(), 1);
  ASSERT_EQ(listnr.get_callbacks().begin()->callback, fake_listener::cb_eof);
}

TEST(SSHOrders, Error) {
  // Listener.
  fake_listener listnr;

  // Buffer.
  buffer_handle bh;

  // Parser.
  parser p;
  p.listen(&listnr);
  p.error(bh);

  // Listener must have received error.
  ASSERT_EQ(listnr.get_callbacks().size(), 1);
  ASSERT_EQ(listnr.get_callbacks().begin()->callback, fake_listener::cb_error);
}

TEST(SSHOrders, Execute) {
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

  // Listener must have received execute and eof.
  ASSERT_EQ(listnr.get_callbacks().size(), 2);
  fake_listener::callback_info info1, info2;
  timestamp comparison_timeout(timestamp::now() + 4242);
  info1 = *listnr.get_callbacks().begin();
  info2 = *++listnr.get_callbacks().begin();
  std::cout << "order ID:   " << fake_listener::cb_execute << std::endl
            << "            " << info1.callback << std::endl
            << "command ID: " << 1478523697531598258ull << std::endl
            << "            " << info1.cmd_id << std::endl
            << "timeout:    " << comparison_timeout.to_seconds() << std::endl
            << "            " << info1.timeout.to_seconds() << std::endl
            << "host:       "
            << "localhost" << std::endl
            << "            " << info1.host << std::endl
            << "user:       "
            << "root" << std::endl
            << "            " << info1.user << std::endl
            << "password:   "
            << "myverysecretpassword" << std::endl
            << "            " << info1.password << std::endl
            << "command:    "
            << "mycheck to execute with some args" << std::endl
            << "            " << info1.cmds.front() << std::endl;
  ASSERT_EQ(info1.callback, fake_listener::cb_execute);
  ASSERT_EQ(info1.cmd_id, 1478523697531598258ull);
  ASSERT_LT(comparison_timeout.to_mseconds() - info1.timeout.to_mseconds(), 1);
  ASSERT_EQ(info1.host, "localhost");
  ASSERT_EQ(info1.user, "root");
  ASSERT_EQ(info1.password, "myverysecretpassword");
  ASSERT_EQ(info1.cmds.size(), 1);
  ASSERT_EQ(info1.cmds.front(), "mycheck to execute with some args");
  ASSERT_EQ(info2.callback, fake_listener::cb_eof);

  // Parser must be empty.
  ASSERT_TRUE(p.get_buffer().empty());
}

TEST(SSHOrders, ExecuteInvalidId) {
  // Create invalid execute order packet.
  buffer_handle bh;
  bh.write(ExecuteInvalidTimeout_DATA2,
           sizeof(ExecuteInvalidTimeout_DATA2) - 1);
  bh.write(ExecuteInvalidTimeout_DATA3,
           sizeof(ExecuteInvalidTimeout_DATA3) - 1);

  // Listener.
  fake_listener listnr;

  // Parser.
  parser p;
  p.listen(&listnr);
  while (!bh.empty())
    p.read(bh);
  p.read(bh);

  // Listener must have received errors and eof.
  ASSERT_EQ(listnr.get_callbacks().size(), 3);

  fake_listener::callback_info info1, info2, info3;
  std::list<fake_listener::callback_info>::const_iterator it;
  it = listnr.get_callbacks().begin();
  info1 = *(it++);
  info2 = *(it++);
  info3 = *(it++);
  ASSERT_EQ(info1.callback, fake_listener::cb_error);
  ASSERT_EQ(info2.callback, fake_listener::cb_error);
  ASSERT_EQ(info3.callback, fake_listener::cb_eof);

  // Parser must be empty.
  ASSERT_TRUE(p.get_buffer().empty());
}

TEST(SSHOrders, ExecuteInvalidStartTime) {
  // Create invalid execute order packet.
  buffer_handle bh;
  bh.write(ExecuteInvalidTimeout_DATA, sizeof(ExecuteInvalidTimeout_DATA) - 1);

  // Listener.
  fake_listener listnr;

  // Parser.
  parser p;
  p.listen(&listnr);
  while (!bh.empty())
    p.read(bh);
  p.read(bh);

  // Listener must have received errors and eof.
  ASSERT_EQ(listnr.get_callbacks().size(), 2);
  fake_listener::callback_info info1, info2;
  std::list<fake_listener::callback_info>::const_iterator it;
  it = listnr.get_callbacks().begin();
  info1 = *(it++);
  info2 = *(it++);
  ASSERT_EQ(info1.callback, fake_listener::cb_error);
  ASSERT_EQ(info2.callback, fake_listener::cb_eof);

  // Parser must be empty.
  ASSERT_TRUE(p.get_buffer().empty());
}

TEST(SSHOrders, ExecuteInvalidTimeout) {
  // Create invalid execute order packet.
  buffer_handle bh;
  bh.write(ExecuteInvalidTimeout_DATA, sizeof(ExecuteInvalidTimeout_DATA) - 1);

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
  ASSERT_EQ(listnr.get_callbacks().size(), 2);
  fake_listener::callback_info info1, info2;
  std::list<fake_listener::callback_info>::const_iterator it;
  it = listnr.get_callbacks().begin();
  info1 = *(it++);
  info2 = *(it++);
  ASSERT_EQ(info1.callback, fake_listener::cb_error);
  ASSERT_EQ(info2.callback, fake_listener::cb_eof);

  // Parser must be empty.
  ASSERT_TRUE(p.get_buffer().empty());
}

const char ExecuteNotEnoughArgs_data1[] = "2\0\0\0\0";
const char ExecuteNotEnoughArgs_data2[] = "2\00042\0\0\0\0";
const char ExecuteNotEnoughArgs_data3[] = "2\00042\00010\0\0\0\0";
const char ExecuteNotEnoughArgs_data4[] = "2\00042\00010\0000\0\0\0\0";
const char ExecuteNotEnoughArgs_data5[] =
    "2\00042\00010\0000\0check_by_ssh\0\0\0\0";
const char ExecuteNotEnoughArgs_data6[] =
    "2\00042\00010\0000\0check_by_ssh -C 'true'\0\0\0\0";
const char ExecuteNotEnoughArgs_data7[] =
    "2\00042\00010\0000\0check_by_ssh -C 'true' -H\0\0\0\0";
const char ExecuteNotEnoughArgs_data8[] =
    "2\00042\00010\0000\0check_by_ssh -C 'true' -H localhost -i\0\0\0\0";
const char ExecuteNotEnoughArgs_data9[] =
    "2\00042\00010\0000\0check_by_ssh -C 'true' -H localhost -l\0\0\0\0";
const char ExecuteNotEnoughArgs_data10[] =
    "2\00042\00010\0000\0check_by_ssh -C 'true' -H localhost -p\0\0\0\0";
const char ExecuteNotEnoughArgs_data11[] =
    "2\00042\00010\0000\0check_by_ssh -C 'true' -H localhost -a\0\0\0\0";
const char ExecuteNotEnoughArgs_data12[] =
    "2\00042\00010\0000\0check_by_ssh -C 'true' -H localhost -t\0\0\0\0";
const char ExecuteNotEnoughArgs_data13[] =
    "2\00042\00010\0000\0check_by_ssh -C '' -H localhost\0\0\0\0";

TEST(SSHOrders, ExecuteNotEnoughArgs) {
  // Create invalid execute order packet.
  buffer_handle bh;
  bh.write(ExecuteNotEnoughArgs_data1, sizeof(ExecuteNotEnoughArgs_data1) - 1);
  bh.write(ExecuteNotEnoughArgs_data2, sizeof(ExecuteNotEnoughArgs_data2) - 1);
  bh.write(ExecuteNotEnoughArgs_data3, sizeof(ExecuteNotEnoughArgs_data3) - 1);
  bh.write(ExecuteNotEnoughArgs_data4, sizeof(ExecuteNotEnoughArgs_data4) - 1);
  bh.write(ExecuteNotEnoughArgs_data5, sizeof(ExecuteNotEnoughArgs_data5) - 1);
  bh.write(ExecuteNotEnoughArgs_data6, sizeof(ExecuteNotEnoughArgs_data6) - 1);
  bh.write(ExecuteNotEnoughArgs_data7, sizeof(ExecuteNotEnoughArgs_data7) - 1);
  bh.write(ExecuteNotEnoughArgs_data8, sizeof(ExecuteNotEnoughArgs_data8) - 1);
  bh.write(ExecuteNotEnoughArgs_data9, sizeof(ExecuteNotEnoughArgs_data9) - 1);
  bh.write(ExecuteNotEnoughArgs_data10,
           sizeof(ExecuteNotEnoughArgs_data10) - 1);
  bh.write(ExecuteNotEnoughArgs_data11,
           sizeof(ExecuteNotEnoughArgs_data11) - 1);
  bh.write(ExecuteNotEnoughArgs_data12,
           sizeof(ExecuteNotEnoughArgs_data12) - 1);
  bh.write(ExecuteNotEnoughArgs_data13,
           sizeof(ExecuteNotEnoughArgs_data13) - 1);

  // Listener.
  fake_listener listnr;

  // Parser.
  parser p;
  p.listen(&listnr);
  while (!bh.empty())
    p.read(bh);
  p.read(bh);

  // Listener must have received errors and eof.
  ASSERT_EQ(listnr.get_callbacks().size(), 14);

  fake_listener::callback_info info[13];
  std::list<fake_listener::callback_info>::const_iterator it;
  it = listnr.get_callbacks().begin();
  for (auto & i : info)
    i = *(it++);
  for (auto & i : info) {
    std::cout << fake_listener::cb_error << ": " << i.callback
              << std::endl;
    ASSERT_EQ(i.callback, fake_listener::cb_error);
  }

  // Parser must be empty.
  ASSERT_TRUE(p.get_buffer().empty());
}

TEST(SSHOrders, InvalidCommand) {
  // Create invalid order packet.
  buffer_handle bh;
  bh.write("42\0\0\0\0", 6);

  // Listener.
  fake_listener listnr;

  // Parser.
  parser p;
  p.listen(&listnr);
  while (!bh.empty())
    p.read(bh);
  p.read(bh);

  // Listener must have received error.
  ASSERT_EQ(listnr.get_callbacks().size(), 2);
  fake_listener::callback_info info1, info2;
  info1 = *listnr.get_callbacks().begin();
  info2 = *++listnr.get_callbacks().begin();
  ASSERT_EQ(info1.callback, fake_listener::cb_error);
  ASSERT_EQ(info2.callback, fake_listener::cb_eof);

  // Parser must be empty.
  ASSERT_TRUE(p.get_buffer().empty());
}

TEST(SSHOrders, Quit) {
  // Create quit order packet.
  buffer_handle bh;
  bh.write("4\0\0\0\0", 5);

  // Listener.
  fake_listener listnr;

  // Parser.
  parser p;
  p.listen(&listnr);
  while (!bh.empty())
    p.read(bh);
  p.read(bh);

  // Listener must have received quit and eof.
  ASSERT_EQ(listnr.get_callbacks().size(), 2);
  fake_listener::callback_info info1, info2;
  info1 = *listnr.get_callbacks().begin();
  info2 = *++listnr.get_callbacks().begin();
  ASSERT_EQ(info1.callback, fake_listener::cb_quit);
  ASSERT_EQ(info2.callback, fake_listener::cb_eof);

  // Parser must be empty.
  ASSERT_TRUE(p.get_buffer().empty());
}

const char Suite_CMD[] =
    "0\0\0\0\0"
    "2\000147852\0007849\000147852369\0check_by_ssh -H localhost -l root -a "
    "password -6 -C ls\0\0\0\0"
    "2\00036525825445548787\0002258\00001\0check_by_ssh -H www.merethis.com -l "
    "centreon -a iswonderful -C \"rm -rf /\"\0\0\0\0"
    "2\00063\0000\00099999999999999999\000check_by_ssh -H www.centreon.com -p "
    "2222 -l merethis -a rocks -C \"./check_for_updates on website\"\0\0\0\0"
    "4\0\0\0\0";

/**
 *  Classic orders suite: version, executions, quit.
 *
 *  @return 0 on success.
 */
TEST(SSHOrders, Suite) {
  // Listener.
  fake_listener listnr;

  // Buffer.
  buffer_handle bh;
  bh.write(Suite_CMD, sizeof(Suite_CMD) - 1);

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
    execute.timeout = 7849 + time(nullptr);
    execute.host = "localhost";
    execute.port = 22;
    execute.user = "root";
    execute.password = "password";
    execute.cmds.emplace_back("ls");
    execute.skip_stdout = -1;
    execute.skip_stderr = -1;
    execute.is_ipv6 = true;
    expected.push_back(execute);
  }
  {  // Second execution order.
    fake_listener::callback_info execute;
    execute.callback = fake_listener::cb_execute;
    execute.cmd_id = 36525825445548787ull;
    execute.timeout = 2258 + time(nullptr);
    execute.host = "www.merethis.com";
    execute.port = 22;
    execute.user = "centreon";
    execute.password = "iswonderful";
    execute.cmds.emplace_back("rm -rf /");
    execute.skip_stdout = -1;
    execute.skip_stderr = -1;
    execute.is_ipv6 = false;
    expected.push_back(execute);
  }
  {  // Third execution order.
    fake_listener::callback_info execute;
    execute.callback = fake_listener::cb_execute;
    execute.cmd_id = 63;
    execute.timeout = time(nullptr);
    execute.host = "www.centreon.com";
    execute.port = 2222;
    execute.user = "merethis";
    execute.password = "rocks";
    execute.cmds.emplace_back("./check_for_updates on website");
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
  ASSERT_EQ(expected, listnr.get_callbacks());
  ASSERT_TRUE(p.get_buffer().empty());
}

TEST(SSHOrders, Version) {
  // Create version order packet.
  buffer_handle bh;
  bh.write("0\0\0\0\0", 5);

  // Listener.
  fake_listener listnr;

  // Parser.
  parser p;
  p.listen(&listnr);
  while (!bh.empty())
    p.read(bh);
  p.read(bh);

  // Listener must have received version and eof.
  ASSERT_EQ(listnr.get_callbacks().size(), 2);
  fake_listener::callback_info info1, info2;
  info1 = *listnr.get_callbacks().begin();
  info2 = *++listnr.get_callbacks().begin();
  ASSERT_EQ(info1.callback, fake_listener::cb_version);
  ASSERT_EQ(info2.callback, fake_listener::cb_eof);

  // Parser must be empty.
  ASSERT_TRUE(p.get_buffer().empty());
}
