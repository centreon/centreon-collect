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

#include <memory>
#include <gtest/gtest.h>
#include "../timeperiod/utils.hh"
#include "com/centreon/engine/commands/raw.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/process_manager.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::commands;

extern configuration::state* config;

class SimpleCommand : public ::testing::Test {
 public:
  void SetUp() override {
//    set_time(20);
    if (config == NULL)
      config = new configuration::state;
    process_manager::load();
    configuration::applier::state::load();  // Needed to store commands
  }

  void TearDown() override {
    configuration::applier::state::unload();  // Needed to store commands
    process_manager::unload();
    delete config;
    config = NULL;
  }
};

class my_listener : public commands::command_listener {
 public:
  result const& get_result() const {
    return _res;
  }

  void finished(result const& res) throw () override {
    _res = res;
  }

 private:
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
  std::unique_ptr<commands::command> cmd{new commands::raw("test", "/bin/echo bonjour")};
  nagios_macros mac;
  commands::result res;
  std::string cc(cmd->process_cmd(&mac));
  ASSERT_EQ(cc, "/bin/echo bonjour");
  cmd->run(cc, mac, 2, res);
  ASSERT_EQ(res.output, "bonjour\n");
}

// Given a name and a command line
// When the add_command method is called
// Then a new raw command is built
// When async executed
// Then we have the output in the result class.
TEST_F(SimpleCommand, NewCommandAsync) {
  std::unique_ptr<my_listener> lstnr(new my_listener);
  std::unique_ptr<commands::command> cmd{new commands::raw("test", "/bin/echo bonjour")};
  cmd->set_listener(lstnr.get());
  nagios_macros mac;
  std::string cc(cmd->process_cmd(&mac));
  ASSERT_EQ(cc, "/bin/echo bonjour");
  cmd->run(cc, mac, 2);
  int timeout{0};
  while (timeout < 50 && lstnr->get_result().output == "") {
    usleep(100000);
    ++timeout;
  }
  ASSERT_TRUE(timeout < 50);
  ASSERT_EQ(lstnr->get_result().output, "bonjour\n");
}
