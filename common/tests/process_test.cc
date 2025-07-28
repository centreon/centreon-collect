/**
 * Copyright 2023 Centreon
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

#include <absl/synchronization/mutex.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <fstream>
#include "com/centreon/common/process/process_args.hh"

#include "com/centreon/common/process/process.hh"

using namespace com::centreon::common;

#ifdef _WINDOWS
#define ECHO_PATH "tests\\echo.bat"
#define SLEEP_PATH "tests\\sleep.bat"
#define ECHO_ENV1_ENV2 "tests\\echo_env1_env2.bat"
#define END_OF_LINE "\r\n"
#else
#define ECHO_PATH "/bin/echo"
#define SLEEP_PATH "/bin/sleep"
#define ECHO_ENV1_ENV2 "tests/echo_env1_env2.sh"
#define END_OF_LINE "\n"
#endif

extern std::shared_ptr<asio::io_context> g_io_context;

static std::shared_ptr<spdlog::logger> _logger =
    spdlog::stdout_color_mt("process_test");

class process_test : public ::testing::Test {
 public:
  static void SetUpTestSuite() {
    _logger->set_level(spdlog::level::trace);
    _logger->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%s:%#] [%n] [%l] [%P] %v");
  }
};

class process_wait : public process<true> {
  absl::Mutex _waiter;

  std::string _stdout;
  std::string _stderr;
  int _exit_code;
  int _exit_status;
  bool _completed = false;

 public:
  template <typename string_type>
  process_wait(const std::shared_ptr<boost::asio::io_context>& io_context,
               const std::shared_ptr<spdlog::logger>& logger,
               const std::string_view& exe_path,
               const std::initializer_list<string_type>& args)
      : process<true>(io_context, logger, exe_path, true, true, args, nullptr) {
  }

  process_wait(const std::shared_ptr<boost::asio::io_context>& io_context,
               const std::shared_ptr<spdlog::logger>& logger,
               const std::string_view& cmd_line)
      : process<true>(io_context, logger, cmd_line, true, true, nullptr) {}

  process_wait(
      const std::shared_ptr<boost::asio::io_context>& io_context,
      const std::shared_ptr<spdlog::logger>& logger,
      const std::string_view& cmd_line,
      const std::shared_ptr<boost::process::v2::process_environment>& env)
      : process<true>(io_context, logger, cmd_line, true, true, env) {}

  const std::string& get_stdout() const { return _stdout; }
  const std::string& get_stderr() const { return _stderr; }
  int get_exit_code() const { return _exit_code; }
  int get_exit_status() const { return _exit_status; }

  void start_process() {
    process<true>::start_process(
        [this](const process<true>&, int exit_code, int exit_status,
               const std::string& std_out, const std::string& std_err) {
          absl::MutexLock l(&_waiter);
          _completed = true;
          _stdout = std_out;
          _stderr = std_err;
          _exit_code = exit_code;
          _exit_status = exit_status;
        },
        {});
  }

  void start_process(const std::chrono::system_clock::duration& timeout) {
    process<true>::start_process(
        [this](const process<true>&, int exit_code, int exit_status,
               const std::string& std_out, const std::string& std_err) {
          absl::MutexLock l(&_waiter);
          _completed = true;
          _stdout = std_out;
          _stderr = std_err;
          _exit_code = exit_code;
          _exit_status = exit_status;
        },
        timeout);
  }

  void wait() {
    absl::MutexLock l(&_waiter);
    if (!_completed) {
      _waiter.Await(absl::Condition(&_completed));
    }
  }
};

TEST_F(process_test, echo) {
  using namespace std::literals;
  std::shared_ptr<process_wait> to_wait(
      new process_wait(g_io_context, _logger, ECHO_PATH, {"hello"s}));
  to_wait->start_process();
  to_wait->wait();
  EXPECT_EQ(to_wait->get_exit_code(), 0);
  EXPECT_EQ(to_wait->get_exit_status(), e_exit_status::normal);
  EXPECT_EQ(to_wait->get_stdout(), "hello" END_OF_LINE);
  EXPECT_EQ(to_wait->get_stderr(), "");
}

TEST_F(process_test, throw_on_error) {
  using namespace std::literals;
  std::shared_ptr<process_wait> to_wait(
      new process_wait(g_io_context, _logger, "turlututu", {"hello"s}));
  ASSERT_THROW(to_wait->start_process(), std::exception);
}

TEST_F(process_test, script_error) {
  using namespace std::literals;
#ifdef _WINDOWS
  std::shared_ptr<process_wait> to_wait(
      new process_wait(g_io_context, _logger, "tests\\\\bad_script.bat"));
#else
  std::shared_ptr<process_wait> to_wait(
      new process_wait(g_io_context, _logger, "/bin/sh", {"taratata"s}));
#endif
  to_wait->start_process();
  to_wait->wait();
  ASSERT_NE(to_wait->get_exit_code(), 0);
  ASSERT_EQ(to_wait->get_stdout(), "");
  ASSERT_GT(to_wait->get_stderr().length(), 10);
}

#ifndef _WIN32

TEST_F(process_test, stdin_to_stdout) {
  ::remove("toto.sh");
  std::ofstream script("toto.sh");
  script << "while read line ; do echo receive $line ; done" << std::endl;

  std::shared_ptr<process_wait> loopback(
      new process_wait(g_io_context, _logger, "/bin/sh  toto.sh"));

  loopback->start_process();

  std::string expected;
  for (unsigned ii = 0; ii < 10; ++ii) {
    if (ii > 5) {
      // in order to let some async_read_some complete
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    loopback->write_to_stdin(fmt::format("hello{}\n", ii));
    expected += fmt::format("receive hello{}\n", ii);
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  loopback->kill();
  loopback->wait();
  ASSERT_EQ(expected, loopback->get_stdout());
}

TEST_F(process_test, shell_stdin_to_stdout) {
  std::shared_ptr<process_wait> loopback(
      new process_wait(g_io_context, _logger, "/bin/sh"));

  loopback->start_process();

  std::string expected;
  for (unsigned ii = 0; ii < 10; ++ii) {
    if (ii > 5) {
      // in order to let some async_read_some complete
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    loopback->write_to_stdin(fmt::format("echo hello{}\n", ii));
    expected += fmt::format("hello{}\n", ii);
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  loopback->kill();
  loopback->wait();
  ASSERT_EQ(expected, loopback->get_stdout());
}

#endif

TEST_F(process_test, kill_process) {
  std::shared_ptr<process_wait> to_wait(
      new process_wait(g_io_context, _logger, SLEEP_PATH, {"10"}));
  to_wait->start_process();

  // wait process starts
  std::this_thread::sleep_for(std::chrono::seconds(1));
  int pid = to_wait->get_pid();
  // kill process
  to_wait->kill();
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

TEST_F(process_test, timeout) {
  std::shared_ptr<process_wait> to_wait(
      new process_wait(g_io_context, _logger, SLEEP_PATH, {"2"}));
  to_wait->start_process(std::chrono::milliseconds(500));
  to_wait->wait();
  EXPECT_EQ(to_wait->get_exit_status(), e_exit_status::timeout);
#ifndef _WIN32
  EXPECT_EQ(to_wait->get_exit_code(), 9);
  EXPECT_EQ(to_wait->get_stdout(), "");
#endif
  EXPECT_EQ(to_wait->get_stderr(), "");
}

TEST_F(process_test, environment) {
  std::unordered_map<boost::process::v2::environment::key,
                     boost::process::v2::environment::value>
      my_env = {{"env1", "env1_value"}, {"env2", "env2_value"}};

  std::shared_ptr<boost::process::v2::process_environment> env =
      std::make_shared<boost::process::v2::process_environment>(my_env);
  std::shared_ptr<process_wait> to_wait(
      new process_wait(g_io_context, _logger, ECHO_ENV1_ENV2, env));
  to_wait->start_process();
  to_wait->wait();
  EXPECT_EQ(to_wait->get_exit_code(), 0);
  EXPECT_EQ(to_wait->get_exit_status(), e_exit_status::normal);
  EXPECT_EQ(to_wait->get_stdout(), "env1_value env2_value" END_OF_LINE);
  EXPECT_EQ(to_wait->get_stderr(), "");
}

#ifndef _WIN32

TEST_F(process_test, stdout_to_file) {
  using namespace std::literals;
  std::shared_ptr<process_wait> to_wait(new process_wait(
      g_io_context, _logger,
      "/bin/sh -c '/bin/echo \"tototiti tata \" >> toto.txt'"));
  to_wait->start_process();
  to_wait->wait();
  EXPECT_EQ(to_wait->get_exit_code(), 0);
  EXPECT_EQ(to_wait->get_exit_status(), e_exit_status::normal);
  EXPECT_EQ(to_wait->get_stdout(), "");
  EXPECT_EQ(to_wait->get_stderr(), "");
  std::ifstream f("toto.txt");
  std::string first_line;
  std::getline(f, first_line);
  EXPECT_EQ(first_line, "tototiti tata ");
}

#endif

static bool check(std::string const& cmdline,
                  std::vector<std::string_view> const& res) {
  try {
    process_args cmd(cmdline);
    if (cmd.get_c_args().size() != res.size() + 1)  // nullptr at the end
      return false;
    auto parse_iter = cmd.get_c_args().begin();
    auto expected_iter = res.begin();
    for (; expected_iter != res.end(); ++parse_iter, ++expected_iter)
      if (*expected_iter != *parse_iter)
        return false;
  } catch (std::exception const& e) {
    (void)e;
    return false;
  }
  return true;
}

static bool check_invalid_cmdline() {
  try {
    process_args arg("'12 12");
  } catch (std::exception const& e) {
    (void)e;
    return true;
  }
  return false;
}

TEST_F(process_test, parse_commandline) {
  EXPECT_TRUE(check_invalid_cmdline());
  {
    std::string cmdline("\\ echo -n \"test\"");
    std::vector<std::string_view> res;
    res.push_back(" echo");
    res.push_back("-n");
    res.push_back("test");
    EXPECT_TRUE(check(cmdline, res));
  }

  {
    std::string cmdline("test \"|\" \"\" \"|\"");
    std::vector<std::string_view> res;
    res.push_back("test");
    res.push_back("|");
    res.push_back("");
    res.push_back("|");
    EXPECT_TRUE(check(cmdline, res));
  }

  {
    std::string cmdline("");
    std::vector<std::string_view> res;
    EXPECT_FALSE(check(cmdline, res));
  }

  {
    std::string cmdline("        \t\t\t\t\t       ");
    std::vector<std::string_view> res;
    EXPECT_FALSE(check(cmdline, res));
  }

  {
    std::string cmdline("aa\tbbb c\tdd ee");
    std::vector<std::string_view> res;
    res.push_back("aa");
    res.push_back("bbb");
    res.push_back("c");
    res.push_back("dd");
    res.push_back("ee");
    EXPECT_TRUE(check(cmdline, res));
  }

  {
    std::string cmdline(" aa    bbb \t  c  dd  ee    \t");
    std::vector<std::string_view> res;
    res.push_back("aa");
    res.push_back("bbb");
    res.push_back("c");
    res.push_back("dd");
    res.push_back("ee");
    EXPECT_TRUE(check(cmdline, res));
  }

  {
    std::string cmdline("    'aa bbb   cc' dddd   ");
    std::vector<std::string_view> res;
    res.push_back("aa bbb   cc");
    res.push_back("dddd");
    EXPECT_TRUE(check(cmdline, res));
  }

  {
    std::string cmdline("    \" aa bbb   cc \" dddd");
    std::vector<std::string_view> res;
    res.push_back(" aa bbb   cc ");
    res.push_back("dddd");
    EXPECT_TRUE(check(cmdline, res));
  }

  {
    std::string cmdline("  ' \" aa bbb bbb  cc ' ");
    std::vector<std::string_view> res;
    res.push_back(" \" aa bbb bbb  cc ");
    EXPECT_TRUE(check(cmdline, res));
  }

  {
    std::string cmdline(" \" '\\n aaa 42 ' \" 4242 ");
    std::vector<std::string_view> res;
    res.push_back(" '\n aaa 42 ' ");
    res.push_back("4242");
    EXPECT_TRUE(check(cmdline, res));
  }

  {
    std::string cmdline("\\\\\" 1 2 \"");
    std::vector<std::string_view> res;
    res.push_back("\\ 1 2 ");
    EXPECT_TRUE(check(cmdline, res));
  }

  {
    std::string cmdline("\\\\\\\" 1 2 \\\"");
    std::vector<std::string_view> res;
    res.push_back("\\\"");
    res.push_back("1");
    res.push_back("2");
    res.push_back("\"");
    EXPECT_TRUE(check(cmdline, res));
  }

  {
    std::string cmdline(" '12\\t ''34 56' \t \" 12 12 12 \" '99 9 9'");
    std::vector<std::string_view> res;
    res.push_back("12\t 34 56");
    res.push_back(" 12 12 12 ");
    res.push_back("99 9 9");
    EXPECT_TRUE(check(cmdline, res));
  }

  {
    std::string cmdline("\\.\\/\\-\\*\\1");
    std::vector<std::string_view> res;
    res.push_back("./-*1");
    EXPECT_TRUE(check(cmdline, res));
  }
}
