/**
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
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/macros.hh"

#include "../timeperiod/utils.hh"
#include "com/centreon/engine/commands/raw.hh"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::commands;

using com::centreon::common::log_v2::log_v2;

void CreateFile(std::string const& filename, std::string const& content) {
  std::ofstream oss(filename);
  oss << content;
}

class SimpleCommand : public ::testing::Test {
 protected:
  std::unique_ptr<configuration::state_helper> _state_hlp;

 public:
  void SetUp() override {
    set_time(-1);
    _state_hlp = init_config_state();
    config->interval_length(1);
  }

  void TearDown() override { deinit_config_state(); }
};

class my_listener : public commands::command_listener {
 public:
  result& get_result() {
    std::lock_guard<std::mutex> guard(_mutex);
    return _res;
  }

  void finished(result const& res) throw() override {
    std::lock_guard<std::mutex> guard(_mutex);
    _res = res;
  }

 private:
  mutable std::mutex _mutex;
  commands::result _res;
};

// Given an empty name
// When the add_command method is called with it as argument,
// Then it returns a NULL pointer.
TEST_F(SimpleCommand, NewCommandWithNoName) {
  ASSERT_THROW(new commands::raw("", "bar"), std::exception);
}

// Given a command to store,
// When the add_command method is called with an empty value,
// Then it returns a NULL pointer.
TEST_F(SimpleCommand, NewCommandWithNoValue) {
  std::unique_ptr<commands::raw> cmd;
  ASSERT_THROW(cmd.reset(new commands::raw("foo", "")), std::exception);
}

// Given an already existing command
// When the add_command method is called with the same name
// Then it returns a NULL pointer.
TEST_F(SimpleCommand, CommandAlreadyExisting) {
  std::unique_ptr<commands::raw> cmd;
  ASSERT_NO_THROW(cmd.reset(new commands::raw("toto", "/bin/ls")));
}

// Given a name and a command line
// When the add_command method is called
// Then a new raw command is built
// When sync executed
// Then we have the output in the result class.
TEST_F(SimpleCommand, NewCommandSync) {
  std::unique_ptr<commands::command> cmd{
      new commands::raw("test", "/bin/echo bonjour")};
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
TEST_F(SimpleCommand, NewCommandAsync) {
  std::unique_ptr<my_listener> lstnr(new my_listener);
  std::unique_ptr<commands::command> cmd{
      new commands::raw("test", "/bin/echo bonjour")};
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

TEST_F(SimpleCommand, LongCommandAsync) {
  std::unique_ptr<my_listener> lstnr(new my_listener);
  std::unique_ptr<commands::command> cmd{
      new commands::raw("test", "/bin/sleep 10")};
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

TEST_F(SimpleCommand, TooRecentDoubleCommand) {
  commands_logger->set_level(spdlog::level::trace);
  CreateFile("/tmp/TooRecentDoubleCommand.sh",
             "echo -n tutu | tee -a /tmp/TooRecentDoubleCommand;");

  const char* path = "/tmp/TooRecentDoubleCommand";
  ::unlink(path);
  std::unique_ptr<my_listener> lstnr(std::make_unique<my_listener>());
  std::unique_ptr<commands::command> cmd{std::make_unique<commands::raw>(
      "test", "/bin/sh /tmp/TooRecentDoubleCommand.sh")};
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

TEST_F(SimpleCommand, SufficientOldDoubleCommand) {
  commands_logger->set_level(spdlog::level::trace);
  CreateFile("/tmp/TooRecentDoubleCommand.sh",
             "echo -n tutu | tee -a /tmp/TooRecentDoubleCommand;");

  const char* path = "/tmp/TooRecentDoubleCommand";
  ::unlink(path);
  std::unique_ptr<my_listener> lstnr(std::make_unique<my_listener>());
  std::unique_ptr<commands::command> cmd{std::make_unique<commands::raw>(
      "test", "/bin/sh /tmp/TooRecentDoubleCommand.sh")};
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

TEST_F(SimpleCommand, WithOneArgument) {
  auto lstnr = std::make_unique<my_listener>();
  std::unique_ptr<commands::command> cmd{
      std::make_unique<commands::raw>("test", "/bin/echo $ARG1$")};
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
