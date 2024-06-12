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

#include "pool.hh"
#include "process.hh"

using namespace com::centreon::common;

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

class process_wait : public process {
  std::condition_variable _cond;
  std::string _stdout;
  std::string _stderr;

 public:
  void on_stdout_read(const boost::system::error_code& err,
                      size_t nb_read) override {
    if (!err) {
      _stdout += std::string_view(_stdout_read_buffer, nb_read);
    }
    process::on_stdout_read(err, nb_read);
  }

  void on_stderr_read(const boost::system::error_code& err,
                      size_t nb_read) override {
    if (!err) {
      _stderr += std::string_view(_stderr_read_buffer, nb_read);
    }
    process::on_stderr_read(err, nb_read);
  }

  void on_process_end(const boost::system::error_code& err,
                      int raw_exit_status) override {
    process::on_process_end(err, raw_exit_status);
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
    std::mutex dummy;
    std::unique_lock l(dummy);
    _cond.wait(l);
  }
};

TEST_F(process_test, echo) {
  using namespace std::literals;
  std::shared_ptr<process_wait> to_wait(
      new process_wait(g_io_context, _logger, "/bin/echo", {"hello"s}));
  to_wait->start_process();
  to_wait->wait();
  ASSERT_EQ(to_wait->get_exit_status(), 0);
  ASSERT_EQ(to_wait->get_stdout(), "hello\n");
  ASSERT_EQ(to_wait->get_stderr(), "");
}

TEST_F(process_test, throw_on_error) {
  using namespace std::literals;
  std::shared_ptr<process_wait> to_wait(
      new process_wait(g_io_context, _logger, "turlututu", {"hello"s}));
  ASSERT_THROW(to_wait->start_process(), std::exception);
}

TEST_F(process_test, script_error) {
  using namespace std::literals;
  std::shared_ptr<process_wait> to_wait(
      new process_wait(g_io_context, _logger, "/bin/sh", {"taratata"s}));
  to_wait->start_process();
  to_wait->wait();
  ASSERT_NE(to_wait->get_exit_status(), 0);
  ASSERT_EQ(to_wait->get_stdout(), "");
  ASSERT_GT(to_wait->get_stderr().length(), 10);
}

TEST_F(process_test, call_start_several_time) {
  std::shared_ptr<process_wait> to_wait(
      new process_wait(g_io_context, _logger, "/bin/echo", {"hello"}));
  std::string expected;
  for (int ii = 0; ii < 10; ++ii) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    to_wait->start_process();
    expected += "hello\n";
  }
  to_wait->wait();
  ASSERT_EQ(to_wait->get_exit_status(), 0);
  ASSERT_EQ(to_wait->get_stdout(), expected);
  ASSERT_EQ(to_wait->get_stderr(), "");
}

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
  ASSERT_EQ(expected, loopback->get_stdout());
}
