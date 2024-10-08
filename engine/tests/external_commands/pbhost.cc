/**
 * Copyright 2005 - 2019 Centreon (https://www.centreon.com/)
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
#include "../timeperiod/utils.hh"
#include "com/centreon/engine/checks/checker.hh"
#include "com/centreon/engine/commands/commands.hh"
#include "com/centreon/engine/configuration/applier/host.hh"
#include "com/centreon/engine/timezone_manager.hh"
#include "common/engine_conf/host_helper.hh"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;

class HostExternalCommand : public ::testing::Test {
 public:
  void SetUp() override { init_config_state(); }

  void TearDown() override { deinit_config_state(); }
};

TEST_F(HostExternalCommand, AddHostDowntime) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);

  hst.set_host_name("test_srv");
  hst.set_address("127.0.0.1");
  hst.set_host_id(1);
  ASSERT_NO_THROW(hst_aply.add_object(hst));

  set_time(20000);
  time_t now = time(nullptr);

  testing::internal::CaptureStdout();

  for (int i = 0; i < 3; i++) {
    now += 300;
    std::string cmd{"test_srv;1;|"};
    set_time(now);
    cmd_process_host_check_result(CMD_PROCESS_HOST_CHECK_RESULT, now,
                                  const_cast<char*>(cmd.c_str()));
    checks::checker::instance().reap();
  }

  std::string const& out{testing::internal::GetCapturedStdout()};
  std::cout << out << std::endl;
  ASSERT_NE(out.find("PASSIVE HOST CHECK"), std::string::npos);
  ASSERT_NE(out.find("HOST ALERT"), std::string::npos);
}

TEST_F(HostExternalCommand, AddHostDowntimeByIpAddress) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);

  hst.set_host_name("test_srv");
  hst.set_address("127.0.0.1");
  hst.set_host_id(1);
  ASSERT_NO_THROW(hst_aply.add_object(hst));

  set_time(20000);
  time_t now = time(nullptr);

  testing::internal::CaptureStdout();
  for (int i = 0; i < 3; i++) {
    now += 300;
    std::string cmd{"127.0.0.1;1;|"};
    set_time(now);
    cmd_process_host_check_result(CMD_PROCESS_HOST_CHECK_RESULT, now,
                                  const_cast<char*>(cmd.c_str()));
    checks::checker::instance().reap();
  }

  std::string const& out{testing::internal::GetCapturedStdout()};

  ASSERT_NE(out.find("PASSIVE HOST CHECK"), std::string::npos);
  ASSERT_NE(out.find("HOST ALERT"), std::string::npos);
}

TEST_F(HostExternalCommand, AddHostComment) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  configuration::Host hst2;
  configuration::host_helper hst2_hlp(&hst2);

  hst.set_host_name("test_srv");
  hst.set_address("127.0.0.1");
  hst.set_host_id(1);
  ASSERT_NO_THROW(hst_aply.add_object(hst));

  hst2.set_host_name("test_srv2");
  hst2.set_address("127.0.0.1");
  hst2.set_host_id(2);
  ASSERT_NO_THROW(hst_aply.add_object(hst2));

  set_time(20000);
  time_t now = time(nullptr);

  std::string cmd_com1{"test_srv;1;user;this is a first comment"};
  std::string cmd_com2{"test_srv;1;user;this is a second comment"};
  std::string cmd_com3{"test_srv;1;user;this is a third comment"};
  std::string cmd_com4{"test_srv;1;user;this is a fourth comment"};
  std::string cmd_com5{"test_srv2;1;user;this is a fifth comment"};
  std::string cmd_del{"1"};
  std::string cmd_del_last{"5"};
  std::string cmd_del_all{"test_srv"};

  cmd_add_comment(CMD_ADD_HOST_COMMENT, now,
                  const_cast<char*>(cmd_com1.c_str()));
  ASSERT_EQ(comment::comments.size(), 1u);
  cmd_add_comment(CMD_ADD_HOST_COMMENT, now,
                  const_cast<char*>(cmd_com2.c_str()));
  ASSERT_EQ(comment::comments.size(), 2u);
  cmd_add_comment(CMD_ADD_HOST_COMMENT, now,
                  const_cast<char*>(cmd_com3.c_str()));
  ASSERT_EQ(comment::comments.size(), 3u);
  cmd_add_comment(CMD_ADD_HOST_COMMENT, now,
                  const_cast<char*>(cmd_com4.c_str()));
  ASSERT_EQ(comment::comments.size(), 4u);
  cmd_add_comment(CMD_ADD_HOST_COMMENT, now,
                  const_cast<char*>(cmd_com5.c_str()));
  ASSERT_EQ(comment::comments.size(), 5u);
  cmd_delete_comment(CMD_DEL_HOST_COMMENT, const_cast<char*>(cmd_del.c_str()));
  ASSERT_EQ(comment::comments.size(), 4u);
  cmd_delete_all_comments(CMD_DEL_ALL_HOST_COMMENTS,
                          const_cast<char*>(cmd_del_all.c_str()));
  ASSERT_EQ(comment::comments.size(), 1u);
  cmd_delete_comment(CMD_DEL_HOST_COMMENT,
                     const_cast<char*>(cmd_del_last.c_str()));
  ASSERT_EQ(comment::comments.size(), 0u);
}
