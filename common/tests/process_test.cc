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

#include <gtest/gtest.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <chrono>
#include <thread>

#include "com/centreon/common/process/process.hh"

using namespace com::centreon::common;

#ifdef _WIN32
#define ECHO_PATH "tests\\echo.bat"
#define SLEEP_PATH "tests\\sleep.bat"
#define END_OF_LINE "\r\n"
#else
#define ECHO_PATH "/bin/echo"
#define SLEEP_PATH "/bin/sleep"
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

class process_wait : public process<> {
  std::mutex _cond_m;
  std::condition_variable _cond;
  std::string _stdout;
  std::string _stderr;
  bool _stdout_eof = false;
  bool _stderr_eof = false;
  bool _process_ended = false;

 public:
  void reset_end() {
    std::lock_guard l(_cond_m);
    _stdout_eof = false;
    _stderr_eof = false;
    _process_ended = false;
  }

  void on_stdout_read(const boost::system::error_code& err,
                      size_t nb_read) override {
    if (!err) {
      std::string_view line(_stdout_read_buffer, nb_read);
      _stdout += line;
      SPDLOG_LOGGER_DEBUG(_logger, "read from stdout: {}", line);
    } else if (err == asio::error::eof || err == asio::error::broken_pipe) {
      std::unique_lock l(_cond_m);
      _stdout_eof = true;
      l.unlock();
      _cond.notify_one();
    }
    process::on_stdout_read(err, nb_read);
  }

  void on_stderr_read(const boost::system::error_code& err,
                      size_t nb_read) override {
    if (!err) {
      std::string_view line(_stderr_read_buffer, nb_read);
      _stderr += line;
      SPDLOG_LOGGER_DEBUG(_logger, "read from stderr: {}", line);
    } else if (err == asio::error::eof || err == asio::error::broken_pipe) {
      std::unique_lock l(_cond_m);
      _stderr_eof = true;
      l.unlock();
      _cond.notify_one();
    }
    process::on_stderr_read(err, nb_read);
  }

  void on_process_end(const boost::system::error_code& err,
                      int raw_exit_status) override {
    process::on_process_end(err, raw_exit_status);
    SPDLOG_LOGGER_DEBUG(_logger, "process end");
    std::unique_lock l(_cond_m);
    _process_ended = true;
    l.unlock();
    _cond.notify_one();
  }

  template <typename string_type>
  process_wait(const std::shared_ptr<boost::asio::io_context>& io_context,
               const std::shared_ptr<spdlog::logger>& logger,
               const std::string_view& exe_path,
               const std::initializer_list<string_type>& args)
      : process(io_context, logger, exe_path, args) {}

  process_wait(const std::shared_ptr<boost::asio::io_context>& io_context,
               const std::shared_ptr<spdlog::logger>& logger,
               const std::string_view& cmd_line)
      : process(io_context, logger, cmd_line) {}

  const std::string& get_stdout() const { return _stdout; }
  const std::string& get_stderr() const { return _stderr; }

  void wait() {
    std::unique_lock l(_cond_m);
    _cond.wait(l,
               [this] { return _process_ended && _stderr_eof && _stdout_eof; });
  }
};

TEST_F(process_test, echo) {
  using namespace std::literals;
  std::shared_ptr<process_wait> to_wait(
      new process_wait(g_io_context, _logger, ECHO_PATH, {"hello"s}));
  to_wait->start_process(true);
  to_wait->wait();
  ASSERT_EQ(to_wait->get_exit_status(), 0);
  ASSERT_EQ(to_wait->get_stdout(), "hello" END_OF_LINE);
  ASSERT_EQ(to_wait->get_stderr(), "");
}

TEST_F(process_test, throw_on_error) {
  using namespace std::literals;
  std::shared_ptr<process_wait> to_wait(
      new process_wait(g_io_context, _logger, "turlututu", {"hello"s}));
  ASSERT_THROW(to_wait->start_process(true), std::exception);
}

TEST_F(process_test, script_error) {
  using namespace std::literals;
#ifdef _WIN32
  std::shared_ptr<process_wait> to_wait(
      new process_wait(g_io_context, _logger, "tests\\\\bad_script.bat"));
#else
  std::shared_ptr<process_wait> to_wait(
      new process_wait(g_io_context, _logger, "/bin/sh", {"taratata"s}));
#endif
  to_wait->start_process(true);
  to_wait->wait();
  ASSERT_NE(to_wait->get_exit_status(), 0);
  ASSERT_EQ(to_wait->get_stdout(), "");
  ASSERT_GT(to_wait->get_stderr().length(), 10);
}

TEST_F(process_test, call_start_several_time) {
  std::shared_ptr<process_wait> to_wait(
      new process_wait(g_io_context, _logger, ECHO_PATH, {"hello"}));
  std::string expected;
  for (int ii = 0; ii < 10; ++ii) {
    to_wait->reset_end();
    to_wait->start_process(true);
    to_wait->wait();
    expected += "hello" END_OF_LINE;
  }
  ASSERT_EQ(to_wait->get_exit_status(), 0);
  ASSERT_EQ(to_wait->get_stdout(), expected);
  ASSERT_EQ(to_wait->get_stderr(), "");
}

TEST_F(process_test, call_start_several_time_no_args) {
  std::shared_ptr<process_wait> to_wait(
      new process_wait(g_io_context, _logger, ECHO_PATH " hello"));
  std::string expected;
  for (int ii = 0; ii < 10; ++ii) {
    to_wait->reset_end();
    to_wait->start_process(true);
    to_wait->wait();
    expected += "hello" END_OF_LINE;
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  ASSERT_EQ(to_wait->get_exit_status(), 0);
  ASSERT_EQ(to_wait->get_stdout(), expected);
  ASSERT_EQ(to_wait->get_stderr(), "");
}

#ifndef _WIN32

TEST_F(process_test, stdin_to_stdout) {
  ::remove("toto.sh");
  std::ofstream script("toto.sh");
  script << "while read line ; do echo receive $line ; done" << std::endl;

  std::shared_ptr<process_wait> loopback(
      new process_wait(g_io_context, _logger, "/bin/sh  toto.sh"));

  loopback->start_process(true);

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
  ASSERT_EQ(expected, loopback->get_stdout());
}

TEST_F(process_test, shell_stdin_to_stdout) {
  std::shared_ptr<process_wait> loopback(
      new process_wait(g_io_context, _logger, "/bin/sh"));

  loopback->start_process(true);

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
  ASSERT_EQ(expected, loopback->get_stdout());
}

#endif

TEST_F(process_test, kill_process) {
  std::shared_ptr<process_wait> to_wait(
      new process_wait(g_io_context, _logger, SLEEP_PATH, {"10"}));
  to_wait->start_process(true);

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
