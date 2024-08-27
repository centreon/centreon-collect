/**
 * Copyright 2024 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */

#include <gtest/gtest.h>

#include "check_exec.hh"

using namespace com::centreon::agent;

extern std::shared_ptr<asio::io_context> g_io_context;

static const std::string serv("serv");
static const std::string cmd_name("command");
static std::string command_line;

TEST(check_exec_test, echo) {
  command_line = "/bin/echo hello toto";
  int status;
  std::list<std::string> outputs;
  std::mutex mut;
  std::condition_variable cond;
  std::shared_ptr<check_exec> check = check_exec::load(
      g_io_context, spdlog::default_logger(), time_point(), serv, cmd_name,
      command_line, engine_to_agent_request_ptr(),
      [&](const std::shared_ptr<com::centreon::agent::check>& caller,
          int statuss,
          const std::list<com::centreon::common::perfdata>& perfdata,
          const std::list<std::string>& output) {
        {
          std::lock_guard l(mut);
          status = statuss;
          outputs = output;
        }
        cond.notify_one();
      });
  check->start_check(std::chrono::seconds(1));

  std::unique_lock l(mut);
  cond.wait(l);
  ASSERT_EQ(status, 0);
  ASSERT_EQ(outputs.size(), 1);
  ASSERT_EQ(*outputs.begin(), "hello toto");
}

TEST(check_exec_test, timeout) {
  command_line = "/bin/sleep 5";
  int status;
  std::list<std::string> outputs;
  std::condition_variable cond;
  std::shared_ptr<check_exec> check = check_exec::load(
      g_io_context, spdlog::default_logger(), time_point(), serv, cmd_name,
      command_line, engine_to_agent_request_ptr(),
      [&](const std::shared_ptr<com::centreon::agent::check>& caller,
          int statuss,
          const std::list<com::centreon::common::perfdata>& perfdata,
          const std::list<std::string>& output) {
        status = statuss;
        outputs = output;
        cond.notify_one();
      });
  check->start_check(std::chrono::seconds(1));

  std::mutex mut;
  std::unique_lock l(mut);
  cond.wait(l);
  ASSERT_EQ(status, 3);
  ASSERT_EQ(outputs.size(), 1);
  ASSERT_EQ(*outputs.begin(), "Timeout at execution of /bin/sleep 5");
}

TEST(check_exec_test, bad_command) {
  command_line = "/usr/bad_path/turlututu titi toto";
  int status;
  std::list<std::string> outputs;
  std::condition_variable cond;
  std::mutex mut;
  std::shared_ptr<check_exec> check = check_exec::load(
      g_io_context, spdlog::default_logger(), time_point(), serv, cmd_name,
      command_line, engine_to_agent_request_ptr(),
      [&](const std::shared_ptr<com::centreon::agent::check>& caller,
          int statuss,
          const std::list<com::centreon::common::perfdata>& perfdata,
          const std::list<std::string>& output) {
        {
          std::lock_guard l(mut);
          status = statuss;
          outputs = output;
        }
        SPDLOG_INFO("end of {}", command_line);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        cond.notify_one();
      });
  check->start_check(std::chrono::seconds(1));

  std::unique_lock l(mut);
  cond.wait(l);
  ASSERT_EQ(status, 3);
  ASSERT_EQ(outputs.size(), 1);
  ASSERT_EQ(*outputs.begin(),
            "Fail to execute /usr/bad_path/turlututu titi toto : No such file "
            "or directory");
}
