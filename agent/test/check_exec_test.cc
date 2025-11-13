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
#include <memory>
#include "agent.pb.h"
#include "check.hh"

#include "check_exec.hh"

using namespace com::centreon::agent;

#ifdef _WIN32
#define ECHO_PATH "tests\\echo.bat"
#define SLEEP_PATH "tests\\sleep.bat"
#define END_OF_LINE "\r\n"
#define ECHO_MULTILINES "tests\\echo_multilines.bat"
#else
#define ECHO_PATH "/bin/echo"
#define SLEEP_PATH "/bin/sleep"
#define END_OF_LINE "\n"
#define ECHO_MULTILINES "tests/echo_multilines.sh"
#endif

extern std::shared_ptr<asio::io_context> g_io_context;

class check_exec_test : public testing::Test {
 public:
  Service serv;

  check_exec_test() {
    serv.set_service_description("serv");
    serv.set_command_name("command");
    serv.set_command_line("my_command_line");
  }
};

TEST_F(check_exec_test, echo) {
  serv.set_command_line(ECHO_PATH " hello toto");
  int status;
  std::list<std::string> outputs;
  std::mutex mut;
  std::condition_variable cond;
  std::shared_ptr<check_exec> check = check_exec::load(
      g_io_context, spdlog::default_logger(), {}, serv, serv.command_line(),
      engine_to_agent_request_ptr(),
      [&]([[maybe_unused]] const std::shared_ptr<com::centreon::agent::check>&
              caller,
          int statuss,
          [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
              perfdata,
          const std::list<std::string>& output) {
        {
          std::lock_guard l(mut);
          status = statuss;
          outputs = output;
        }
        cond.notify_one();
      },
      std::make_shared<checks_statistics>(), nullptr);
  check->start_check(std::chrono::seconds(1));

  std::unique_lock l(mut);
  cond.wait(l);
  ASSERT_EQ(status, 0);
  ASSERT_EQ(outputs.size(), 1);
  ASSERT_EQ(outputs.begin()->substr(0, 10), "hello toto");
}

TEST_F(check_exec_test, timeout) {
  serv.set_command_line(SLEEP_PATH " 120");
  int status;
  std::list<std::string> outputs;
  std::condition_variable cond;
  std::shared_ptr<check_exec> check = check_exec::load(
      g_io_context, spdlog::default_logger(), {}, serv, serv.command_line(),
      engine_to_agent_request_ptr(),
      [&]([[maybe_unused]] const std::shared_ptr<com::centreon::agent::check>&
              caller,
          int statuss,
          [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
              perfdata,
          const std::list<std::string>& output) {
        status = statuss;
        outputs = output;
        cond.notify_one();
      },
      std::make_shared<checks_statistics>(), nullptr);
  check->start_check(std::chrono::seconds(1));

  int pid = check->get_pid();

  std::mutex mut;
  std::unique_lock l(mut);
  cond.wait(l);
  ASSERT_NE(status, 0);
  ASSERT_EQ(outputs.size(), 1);

  ASSERT_EQ(*outputs.begin(), "Timeout at execution of " SLEEP_PATH " 120");
  ASSERT_GT(pid, 0);
  std::this_thread::sleep_for(std::chrono::seconds(1));

#ifdef _WIN32
  auto process_handle =
      OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  ASSERT_NE(process_handle, nullptr);
  DWORD exit_code;
  ASSERT_EQ(GetExitCodeProcess(process_handle, &exit_code), TRUE);
  ASSERT_NE(exit_code, STILL_ACTIVE);
  CloseHandle(process_handle);
#else
  ASSERT_EQ(kill(pid, 0), -1);
#endif
}

TEST_F(check_exec_test, bad_command) {
  serv.set_command_line("/usr/bad_path/turlututu titi toto");
  int status;
  std::list<std::string> outputs;
  std::condition_variable cond;
  std::mutex mut;
  std::shared_ptr<check_exec> check = check_exec::load(
      g_io_context, spdlog::default_logger(), {}, serv, serv.command_line(),
      engine_to_agent_request_ptr(),
      [&]([[maybe_unused]] const std::shared_ptr<com::centreon::agent::check>&
              caller,
          int statuss,
          [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
              perfdata,
          const std::list<std::string>& output) {
        {
          std::lock_guard l(mut);
          status = statuss;
          outputs = output;
        }
        SPDLOG_INFO("end of {}", serv.command_line());
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        cond.notify_one();
      },
      std::make_shared<checks_statistics>(), nullptr);
  check->start_check(std::chrono::seconds(1));

  std::unique_lock l(mut);
  cond.wait(l);
  ASSERT_EQ(status, 3);
  ASSERT_EQ(outputs.size(), 1);
#ifdef _WIN32
  // message is language dependant
  ASSERT_GE(outputs.begin()->size(), 20);
#else
  ASSERT_EQ(*outputs.begin(),
            "Fail to execute /usr/bad_path/turlututu titi toto : No such file "
            "or directory");
#endif
}

TEST_F(check_exec_test, recurse_not_lock) {
  serv.set_command_line(ECHO_PATH " hello toto");
  std::condition_variable cond;
  unsigned cpt = 0;
  std::shared_ptr<check_exec> check = check_exec::load(
      g_io_context, spdlog::default_logger(), {}, serv, serv.command_line(),
      engine_to_agent_request_ptr(),
      [&](const std::shared_ptr<com::centreon::agent::check>& caller, int,
          [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
              perfdata,
          [[maybe_unused]] const std::list<std::string>& output) {
        if (!cpt) {
          ++cpt;
          caller->start_check(std::chrono::seconds(1));
        } else
          cond.notify_one();
      },
      std::make_shared<checks_statistics>(), nullptr);
  check->start_check(std::chrono::seconds(1));

  std::mutex mut;
  std::unique_lock l(mut);
  cond.wait(l);
}
// test if the output with multiple lines is correctly displayed
TEST_F(check_exec_test, echo_perfdata) {
  Service serv;
  serv.set_command_line(ECHO_MULTILINES);

  int status = -1;
  std::list<std::string> outputs;
  std::list<com::centreon::common::perfdata> perfs;
  std::mutex mut;
  std::condition_variable cond;

  std::shared_ptr<check_exec> check = check_exec::load(
      g_io_context, spdlog::default_logger(), {}, serv, serv.command_line(),
      engine_to_agent_request_ptr(),
      [&]([[maybe_unused]] const std::shared_ptr<com::centreon::agent::check>&
              caller,
          int statuss,
          const std::list<com::centreon::common::perfdata>& perfdata,
          const std::list<std::string>& output) {
        {
          std::lock_guard l(mut);
          status = statuss;
          outputs = output;
          perfs = perfdata;
        }
        cond.notify_one();
      },
      std::make_shared<checks_statistics>(), nullptr);
  check->start_check(std::chrono::seconds(1));

  std::unique_lock l(mut);
  cond.wait(l);

  ASSERT_EQ(status, 0);
  ASSERT_EQ(outputs.size(), 1);

  // Normalize Windows output (strip quotes, convert CRLF → LF)
  std::string out = *outputs.begin();
#ifdef _WIN32
  out.erase(std::remove(out.begin(), out.end(), '\r'), out.end());
  if (!out.empty() && out.front() == '"')
    out.erase(out.begin());
  if (!out.empty() && out.back() == '"')
    out.pop_back();
#endif

  ASSERT_EQ(out.substr(0, 58),
            "OK - load average: 0.00\nOK - load1\nOK - load5\nOK - load15\n");
  ASSERT_EQ(perfs.size(), 3);
}