/**
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

#include "com/centreon/connector/ssh/orders/parser.hh"
#include "fake_listener.hh"

using namespace com::centreon::connector::ssh::orders;

const char ExecuteInvalidTimeout_DATA[] =
    "2\00042\000foo\0000\0localhost root centreon ls\0\0\0\0";
const char ExecuteInvalidTimeout_DATA2[] =
    "2\0\00010\0000\0localhost root centreon ls\0\0\0\0";
const char ExecuteInvalidTimeout_DATA3[] =
    "2\0foo\00010\0000\0localhost root centreon ls\0\0\0\0";

static shared_io_context io_context(std::make_shared<asio::io_context>());

class mock_parser : public parser {
 public:
  mock_parser(
      shared_io_context io_context,
      const std::shared_ptr<com::centreon::connector::policy_interface>& policy)
      : parser(io_context, policy) {}

  void start_read() override {}

  void to_buffer(const std::string& data) {
    if (data.empty()) {
      read_handler(parser::eof_err, data.length());
    } else {
      memcpy(_recv_buff, data.c_str(), data.length());
      read_handler({}, data.length());
    }
  }
  void error_read() {
    read_handler(std::make_error_code(std::errc::argument_list_too_long), -1);
  }

  const std::string& get_buffer() const { return _buffer; }
};

TEST(SSHOrders, Close) {
  // Listener.
  std::shared_ptr<fake_listener> listnr(std::make_shared<fake_listener>());

  // Parser.
  mock_parser pars(io_context, listnr);
  pars.to_buffer("");

  // Listener must have received eof.
  ASSERT_EQ(listnr->get_callbacks().size(), 1u);
  ASSERT_EQ(listnr->get_callbacks().begin()->callback, fake_listener::cb_eof);
}

TEST(SSHOrders, Eof) {
  // Listener.
  std::shared_ptr<fake_listener> listnr(std::make_shared<fake_listener>());

  // Parser.
  mock_parser pars(io_context, listnr);
  pars.to_buffer("");

  // Listener must have received eof.
  ASSERT_EQ(listnr->get_callbacks().size(), 1u);
  ASSERT_EQ(listnr->get_callbacks().begin()->callback, fake_listener::cb_eof);
}

TEST(SSHOrders, Error) {
  // Listener.
  std::shared_ptr<fake_listener> listnr(std::make_shared<fake_listener>());

  // Parser.
  mock_parser pars(io_context, listnr);
  pars.error_read();

  // Listener must have received error.
  ASSERT_EQ(listnr->get_callbacks().size(), 1u);
  ASSERT_EQ(listnr->get_callbacks().begin()->callback, fake_listener::cb_error);
}

TEST(SSHOrders, Execute) {
  // Listener.
  std::shared_ptr<fake_listener> listnr(std::make_shared<fake_listener>());

  // Parser.
  mock_parser pars(io_context, listnr);

  // Create execution order packet.
  const char* s = "2";
  pars.to_buffer(std::string(s, s + 2));
  s = "1478523697531598258";
  pars.to_buffer(std::string(s, s + strlen(s) + 1));
  s = "4242";  // Timeout.
  pars.to_buffer(std::string(s, s + 5));
  s = "4241";  // Start time.
  pars.to_buffer(std::string(s, s + 5));
  pars.to_buffer(
      "check_by_ssh -H localhost -l root -a myverysecretpassword -C \"mycheck "
      "to execute with some args\"");
  s = "\0\0\0\0";
  pars.to_buffer(std::string(s, s + 4));
  pars.to_buffer("");

  // Listener must have received execute and eof.
  ASSERT_EQ(listnr->get_callbacks().size(), 2u);
  fake_listener::callback_info info1, info2;
  time_point comparison_timeout(system_clock::now() +
                                std::chrono::seconds(4242));
  info1 = *listnr->get_callbacks().begin();
  info2 = *++listnr->get_callbacks().begin();
  std::cout << "order ID:   " << fake_listener::cb_execute << std::endl
            << "            " << info1.callback << std::endl
            << "command ID: " << 1478523697531598258ull << std::endl
            << "            " << info1.cmd_id << std::endl
            << "timeout:    " << comparison_timeout.time_since_epoch().count()
            << std::endl
            << "            " << info1.timeout.time_since_epoch().count()
            << std::endl
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
  ASSERT_LT(
      (comparison_timeout.time_since_epoch() - info1.timeout.time_since_epoch())
          .count(),
      100000000);  // 100ms
  ASSERT_EQ(info1.host, "localhost");
  ASSERT_EQ(info1.user, "root");
  ASSERT_EQ(info1.password, "myverysecretpassword");
  ASSERT_EQ(info1.cmds.size(), 1u);
  ASSERT_EQ(info1.cmds.front(), "mycheck to execute with some args");
  ASSERT_EQ(info2.callback, fake_listener::cb_eof);

  // Parser must be empty.
  ASSERT_TRUE(pars.get_buffer().empty());
}

TEST(SSHOrders, ExecuteInvalidId) {
  // Listener.
  std::shared_ptr<fake_listener> listnr(std::make_shared<fake_listener>());

  // Parser.
  mock_parser pars(io_context, listnr);

  pars.to_buffer(std::string(ExecuteInvalidTimeout_DATA2,
                             sizeof(ExecuteInvalidTimeout_DATA2) - 1));
  pars.to_buffer(std::string(ExecuteInvalidTimeout_DATA3,
                             sizeof(ExecuteInvalidTimeout_DATA3) - 1));
  pars.to_buffer("");

  // Listener must have received errors and eof.
  ASSERT_EQ(listnr->get_callbacks().size(), 3u);

  fake_listener::callback_info info1, info2, info3;
  std::list<fake_listener::callback_info>::const_iterator it;
  it = listnr->get_callbacks().begin();
  info1 = *(it++);
  info2 = *(it++);
  info3 = *(it++);
  ASSERT_EQ(info1.callback, fake_listener::cb_error);
  ASSERT_EQ(info2.callback, fake_listener::cb_error);
  ASSERT_EQ(info3.callback, fake_listener::cb_eof);

  // Parser must be empty.
  ASSERT_TRUE(pars.get_buffer().empty());
}

TEST(SSHOrders, ExecuteInvalidStartTime) {
  // Listener.
  std::shared_ptr<fake_listener> listnr(std::make_shared<fake_listener>());

  // Parser.
  mock_parser pars(io_context, listnr);
  pars.to_buffer(std::string(
      ExecuteInvalidTimeout_DATA,
      ExecuteInvalidTimeout_DATA + sizeof(ExecuteInvalidTimeout_DATA) - 1));
  pars.to_buffer("");

  // Listener must have received errors and eof.
  ASSERT_EQ(listnr->get_callbacks().size(), 2u);
  fake_listener::callback_info info1, info2;
  std::list<fake_listener::callback_info>::const_iterator it;
  it = listnr->get_callbacks().begin();
  info1 = *(it++);
  info2 = *(it++);
  ASSERT_EQ(info1.callback, fake_listener::cb_error);
  ASSERT_EQ(info2.callback, fake_listener::cb_eof);

  // Parser must be empty.
  ASSERT_TRUE(pars.get_buffer().empty());
}

TEST(SSHOrders, ExecuteInvalidTimeout) {
  // Listener.
  std::shared_ptr<fake_listener> listnr(std::make_shared<fake_listener>());

  // Parser.
  mock_parser pars(io_context, listnr);
  pars.to_buffer(std::string(
      ExecuteInvalidTimeout_DATA3,
      ExecuteInvalidTimeout_DATA3 + sizeof(ExecuteInvalidTimeout_DATA3) - 1));
  pars.to_buffer("");

  // Listener must have received errors and eof.
  ASSERT_EQ(listnr->get_callbacks().size(), 2u);
  fake_listener::callback_info info1, info2;
  std::list<fake_listener::callback_info>::const_iterator it;
  it = listnr->get_callbacks().begin();
  info1 = *(it++);
  info2 = *(it++);
  ASSERT_EQ(info1.callback, fake_listener::cb_error);
  ASSERT_EQ(info2.callback, fake_listener::cb_eof);

  // Parser must be empty.
  ASSERT_TRUE(pars.get_buffer().empty());
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
  // Listener.
  std::shared_ptr<fake_listener> listnr(std::make_shared<fake_listener>());

  // Parser.
  mock_parser pars(io_context, listnr);

  // Create invalid execute order packet.
  pars.to_buffer(std::string(ExecuteNotEnoughArgs_data1,
                             sizeof(ExecuteNotEnoughArgs_data1) - 1));
  pars.to_buffer(std::string(ExecuteNotEnoughArgs_data2,
                             sizeof(ExecuteNotEnoughArgs_data2) - 1));
  pars.to_buffer(std::string(ExecuteNotEnoughArgs_data3,
                             sizeof(ExecuteNotEnoughArgs_data3) - 1));
  pars.to_buffer(std::string(ExecuteNotEnoughArgs_data4,
                             sizeof(ExecuteNotEnoughArgs_data4) - 1));
  pars.to_buffer(std::string(ExecuteNotEnoughArgs_data5,
                             sizeof(ExecuteNotEnoughArgs_data5) - 1));
  pars.to_buffer(std::string(ExecuteNotEnoughArgs_data6,
                             sizeof(ExecuteNotEnoughArgs_data6) - 1));
  pars.to_buffer(std::string(ExecuteNotEnoughArgs_data7,
                             sizeof(ExecuteNotEnoughArgs_data7) - 1));
  pars.to_buffer(std::string(ExecuteNotEnoughArgs_data8,
                             sizeof(ExecuteNotEnoughArgs_data8) - 1));
  pars.to_buffer(std::string(ExecuteNotEnoughArgs_data9,
                             sizeof(ExecuteNotEnoughArgs_data9) - 1));
  pars.to_buffer(std::string(ExecuteNotEnoughArgs_data10,
                             sizeof(ExecuteNotEnoughArgs_data10) - 1));
  pars.to_buffer(std::string(ExecuteNotEnoughArgs_data11,
                             sizeof(ExecuteNotEnoughArgs_data11) - 1));
  pars.to_buffer(std::string(ExecuteNotEnoughArgs_data12,
                             sizeof(ExecuteNotEnoughArgs_data12) - 1));
  pars.to_buffer(std::string(ExecuteNotEnoughArgs_data13,
                             sizeof(ExecuteNotEnoughArgs_data13) - 1));

  pars.to_buffer("");

  // Listener must have received errors and eof.
  ASSERT_EQ(listnr->get_callbacks().size(), 14u);

  fake_listener::callback_info info[13];
  std::list<fake_listener::callback_info>::const_iterator it;
  it = listnr->get_callbacks().begin();
  for (auto& i : info)
    i = *(it++);
  for (auto& i : info) {
    std::cout << fake_listener::cb_error << ": " << i.callback << std::endl;
    ASSERT_EQ(i.callback, fake_listener::cb_error);
  }

  // Parser must be empty.
  ASSERT_TRUE(pars.get_buffer().empty());
}

TEST(SSHOrders, InvalidCommand) {
  // Listener.
  std::shared_ptr<fake_listener> listnr(std::make_shared<fake_listener>());

  // Parser.
  mock_parser pars(io_context, listnr);
  // Create invalid order packet.
  const char s[] = "42\0\0\0\0";
  pars.to_buffer(std::string(s, s + 6));
  pars.to_buffer("");

  // Listener must have received error.
  ASSERT_EQ(listnr->get_callbacks().size(), 2u);
  fake_listener::callback_info info1, info2;
  info1 = *listnr->get_callbacks().begin();
  info2 = *++listnr->get_callbacks().begin();
  ASSERT_EQ(info1.callback, fake_listener::cb_error);
  ASSERT_EQ(info2.callback, fake_listener::cb_eof);

  // Parser must be empty.
  ASSERT_TRUE(pars.get_buffer().empty());
}

TEST(SSHOrders, Quit) {
  // Listener.
  std::shared_ptr<fake_listener> listnr(std::make_shared<fake_listener>());

  // Parser.
  mock_parser pars(io_context, listnr);
  // Create quit order packet.
  const char s[] = "4\0\0\0\0";
  pars.to_buffer(std::string(s, s + 5));
  pars.to_buffer("");

  // Listener must have received quit and eof.
  ASSERT_EQ(listnr->get_callbacks().size(), 2u);
  fake_listener::callback_info info1, info2;
  info1 = *listnr->get_callbacks().begin();
  info2 = *++listnr->get_callbacks().begin();
  ASSERT_EQ(info1.callback, fake_listener::cb_quit);
  ASSERT_EQ(info2.callback, fake_listener::cb_eof);

  // Parser must be empty.
  ASSERT_TRUE(pars.get_buffer().empty());
}

const char Suite_CMD[] =
    "0\0\0\0\0"
    "2\000147852\0007849\000147852369\0check_by_ssh -H localhost -l root -a "
    "password -6 -C ls\0\0\0\0"
    "2\00036525825445548787\0002258\00001\0check_by_ssh -H www.merethis.com -l "
    "centreon -a iswonderful -C \"rm -rf /\"\0\0\0\0"
    "2\00063\0000\00099999999999999999\000check_by_ssh -H www.centreon.com -p"
    "2222 -l merethis -a rocks -C \"./check_for_updates on website\"\0\0\0\0"
    "4\0\0\0\0";

/**
 *  Classic orders suite: version, executions, quit.
 *
 *  @return 0 on success.
 */
TEST(SSHOrders, Suite) {
  // Listener.
  std::shared_ptr<fake_listener> listnr(std::make_shared<fake_listener>());

  // Parser.
  mock_parser pars(io_context, listnr);
  pars.to_buffer(std::string(Suite_CMD, sizeof(Suite_CMD) - 1));
  pars.to_buffer("");

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
    execute.timeout = system_clock::now() + std::chrono::seconds(7849);
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
    execute.timeout = system_clock::now() + std::chrono::seconds(2258);
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
    execute.timeout = system_clock::now();
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
  ASSERT_EQ(expected, listnr->get_callbacks());
  ASSERT_TRUE(pars.get_buffer().empty());
}

TEST(SSHOrders, Version) {
  // Listener.
  std::shared_ptr<fake_listener> listnr(std::make_shared<fake_listener>());

  // Parser.
  mock_parser pars(io_context, listnr);
  pars.to_buffer(std::string("0\0\0\0\0", sizeof("0\0\0\0\0") - 1));
  pars.to_buffer("");

  // Listener must have received version and eof.
  ASSERT_EQ(listnr->get_callbacks().size(), 2u);
  fake_listener::callback_info info1, info2;
  info1 = *listnr->get_callbacks().begin();
  info2 = *++listnr->get_callbacks().begin();
  ASSERT_EQ(info1.callback, fake_listener::cb_version);
  ASSERT_EQ(info2.callback, fake_listener::cb_eof);

  // Parser must be empty.
  ASSERT_TRUE(pars.get_buffer().empty());
}
