/*
 * Copyright 2019 Centreon (https://www.centreon.com/)
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
#include <spdlog/common.h>
#include <com/centreon/engine/macros.hh>

#include "../timeperiod/utils.hh"
#include "com/centreon/engine/commands/raw_v2.hh"
#include "common/log_v2/log_v2.hh"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::commands;
using com::centreon::common::log_v2::log_v2;

static void CreateFile(const std::string& filename,
                       const std::string& content) {
  std::ofstream oss(filename);
  oss << content;
}

class PbSimpleCommand : public ::testing::Test {
 protected:
  std::shared_ptr<spdlog::logger> logger;

 public:
  void SetUp() override {
    logger = log_v2::instance().get(log_v2::COMMANDS);
    set_time(-1);
    init_config_state();
    pb_config.set_interval_length(1);
  }

  void TearDown() override { deinit_config_state(); }
};

class my_listener : public commands::command_listener {
  mutable std::mutex _mutex;
  commands::result _res;

 public:
  result& get_result() {
    std::lock_guard<std::mutex> guard(_mutex);
    return _res;
  }

  void finished(result const& res) throw() override {
    std::lock_guard<std::mutex> guard(_mutex);
    _res = res;
  }
};

// Given an empty name
// When the add_command method is called with it as argument,
// Then it returns a NULL pointer.
TEST_F(PbSimpleCommand, NewCommandWithNoName) {
  ASSERT_THROW(new commands::raw_v2(g_io_context, "", "bar"), std::exception);
}

// Given a command to store,
// When the add_command method is called with an empty value,
// Then it returns a NULL pointer.
TEST_F(PbSimpleCommand, NewCommandWithNoValue) {
  std::shared_ptr<commands::raw_v2> cmd;
  ASSERT_THROW(cmd.reset(new commands::raw_v2(g_io_context, "foo", "")),
               std::exception);
}

// Given an already existing command
// When the add_command method is called with the same name
// Then it returns a NULL pointer.
TEST_F(PbSimpleCommand, CommandAlreadyExisting) {
  std::shared_ptr<commands::raw_v2> cmd;
  ASSERT_NO_THROW(
      cmd.reset(new commands::raw_v2(g_io_context, "toto", "/bin/ls")));
}

// Given a name and a command line
// When the add_command method is called
// Then a new raw command is built
// When sync executed
// Then we have the output in the result class.
TEST_F(PbSimpleCommand, NewCommandSync) {
  std::shared_ptr<commands::command> cmd{
      new commands::raw_v2(g_io_context, "test", "/bin/echo bonjour")};
  nagios_macros* mac(get_global_macros());
  commands::result res;
  std::string cc(cmd->process_cmd(mac));
  ASSERT_EQ(cc, "/bin/echo bonjour");
  cmd->run(cc, *mac, 2, res);
  ASSERT_EQ(res.output, "bonjour\n");
}

// Given a name and a command line
// When the add_command method is called
// Then a new raw command is built
// When async executed
// Then we have the output in the result class.
TEST_F(PbSimpleCommand, NewCommandAsync) {
  std::unique_ptr<my_listener> lstnr(new my_listener);
  std::shared_ptr<commands::command> cmd{
      new commands::raw_v2(g_io_context, "test", "/bin/echo bonjour")};
  cmd->set_listener(lstnr.get());
  nagios_macros* mac(get_global_macros());
  std::string cc(cmd->process_cmd(mac));
  ASSERT_EQ(cc, "/bin/echo bonjour");
  cmd->run(cc, *mac, 2, std::make_shared<check_result>());
  int timeout{0};
  int max_timeout{3000};
  while (timeout < max_timeout && lstnr->get_result().output == "") {
    usleep(100000);
    ++timeout;
  }
  ASSERT_TRUE(timeout < max_timeout);
  ASSERT_EQ(lstnr->get_result().output, "bonjour\n");
}

TEST_F(PbSimpleCommand, LongCommandAsync) {
  std::unique_ptr<my_listener> lstnr(new my_listener);
  std::shared_ptr<commands::command> cmd{
      new commands::raw_v2(g_io_context, "test", "/bin/sleep 10")};
  cmd->set_listener(lstnr.get());
  nagios_macros* mac(get_global_macros());
  std::string cc(cmd->process_cmd(mac));
  ASSERT_EQ(cc, "/bin/sleep 10");

  // We force the time to be coherent with now because the function gettimeofday
  // that is not simulated.
  set_time(std::time(nullptr));
  cmd->run(cc, *mac, 2, std::make_shared<check_result>());
  int timeout{0};
  int max_timeout{15};
  while (timeout < max_timeout && lstnr->get_result().output == "") {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    set_time(std::time(nullptr) + 1);
    ++timeout;
  }
  ASSERT_EQ(lstnr->get_result().output, "(Process Timeout)");
}

TEST_F(PbSimpleCommand, TooRecentDoubleCommand) {
  logger->set_level(spdlog::level::trace);
  CreateFile("/tmp/TooRecentDoubleCommand.sh",
             "echo -n tutu | tee -a /tmp/TooRecentDoubleCommand;");

  const char* path = "/tmp/TooRecentDoubleCommand";
  ::unlink(path);
  std::unique_ptr<my_listener> lstnr(std::make_unique<my_listener>());
  std::shared_ptr<commands::command> cmd{std::make_unique<commands::raw_v2>(
      g_io_context, "test", "/bin/sh /tmp/TooRecentDoubleCommand.sh")};
  cmd->set_listener(lstnr.get());
  const void* caller[] = {nullptr, path};
  cmd->add_caller_group(caller, caller + 2);
  nagios_macros* mac(get_global_macros());
  std::string cc(cmd->process_cmd(mac));
  time_t now = 10000;
  set_time(now);
  cmd->run(cc, *mac, 2, std::make_shared<check_result>(), caller[0]);
  for (int wait_ind = 0; wait_ind != 50 && lstnr->get_result().output == "";
       ++wait_ind) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  std::this_thread::sleep_for(std::chrono::seconds(1));
  ASSERT_EQ(lstnr->get_result().exit_code, 0);
  ASSERT_EQ(lstnr->get_result().exit_status, process::status::normal);
  ASSERT_EQ(lstnr->get_result().output, "tutu");
  struct stat file_stat;
  ASSERT_EQ(stat(path, &file_stat), 0);
  ASSERT_EQ(file_stat.st_size, 4);
  ++now;
  cmd->run(cc, *mac, 2, std::make_shared<check_result>(), caller[1]);
  for (int wait_ind = 0; wait_ind != 50 && lstnr->get_result().output == "";
       ++wait_ind) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  ASSERT_EQ(lstnr->get_result().exit_code, 0);
  ASSERT_EQ(lstnr->get_result().exit_status, process::status::normal);
  ASSERT_EQ(lstnr->get_result().output, "tutu");
  ASSERT_EQ(stat(path, &file_stat), 0);
  ASSERT_EQ(file_stat.st_size, 4);
}

TEST_F(PbSimpleCommand, SufficientOldDoubleCommand) {
  logger->set_level(spdlog::level::trace);
  CreateFile("/tmp/TooRecentDoubleCommand.sh",
             "echo -n tutu | tee -a /tmp/TooRecentDoubleCommand;");

  const char* path = "/tmp/TooRecentDoubleCommand";
  ::unlink(path);
  std::unique_ptr<my_listener> lstnr(std::make_unique<my_listener>());
  std::shared_ptr<commands::command> cmd{std::make_unique<commands::raw_v2>(
      g_io_context, "test", "/bin/sh /tmp/TooRecentDoubleCommand.sh")};
  cmd->set_listener(lstnr.get());
  const void* caller[] = {nullptr, path};
  cmd->add_caller_group(caller, caller + 2);
  nagios_macros* mac(get_global_macros());
  std::string cc(cmd->process_cmd(mac));
  time_t now = 10000;
  set_time(now);
  cmd->run(cc, *mac, 2, std::make_shared<check_result>(), caller[0]);
  for (int wait_ind = 0; wait_ind != 50 && lstnr->get_result().output == "";
       ++wait_ind) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  std::this_thread::sleep_for(std::chrono::seconds(1));
  ASSERT_EQ(lstnr->get_result().exit_code, 0);
  ASSERT_EQ(lstnr->get_result().exit_status, process::status::normal);
  ASSERT_EQ(lstnr->get_result().output, "tutu");
  struct stat file_stat;
  ASSERT_EQ(stat(path, &file_stat), 0);
  ASSERT_EQ(file_stat.st_size, 4);
  now += 10;
  set_time(now);
  lstnr->get_result().output = "";
  cmd->run(cc, *mac, 2, std::make_shared<check_result>(), caller[1]);
  for (int wait_ind = 0; wait_ind != 50 && lstnr->get_result().output == "";
       ++wait_ind) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  ASSERT_EQ(lstnr->get_result().exit_code, 0);
  ASSERT_EQ(lstnr->get_result().exit_status, process::status::normal);
  ASSERT_EQ(lstnr->get_result().output, "tutu");
  ASSERT_EQ(stat(path, &file_stat), 0);
  ASSERT_EQ(file_stat.st_size, 8);
}

TEST_F(PbSimpleCommand, WithOneArgument) {
  auto lstnr = std::make_unique<my_listener>();
  std::shared_ptr<commands::command> cmd{std::make_unique<commands::raw_v2>(
      g_io_context, "test", "/bin/echo $ARG1$")};
  cmd->set_listener(lstnr.get());
  nagios_macros* mac(get_global_macros());
  mac->argv[0] = "Hello";
  mac->argv[1] = "";
  std::string cc(cmd->process_cmd(mac));
  ASSERT_EQ(cc, "/bin/echo Hello");
  cmd->run(cc, *mac, 2, std::make_shared<check_result>());
  int timeout{0};
  int max_timeout{3000};
  while (timeout < max_timeout && lstnr->get_result().output == "") {
    usleep(100000);
    ++timeout;
  }
  ASSERT_TRUE(timeout < max_timeout);
  ASSERT_EQ(lstnr->get_result().output, "Hello\n");
}
