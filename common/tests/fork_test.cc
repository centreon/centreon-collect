/**
 * Copyright 2026 Centreon
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

#include "com/centreon/common/process/fork.hh"
#include <absl/synchronization/mutex.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <chrono>
#include <thread>

using namespace com::centreon::common;

extern std::shared_ptr<asio::io_context> g_io_context;

static std::shared_ptr<spdlog::logger> _logger =
    spdlog::stdout_color_mt("fork_test");

class fork_test : public ::testing::Test {
 public:
  static void SetUpTestSuite() {
    _logger->set_level(spdlog::level::trace);
    _logger->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%s:%#] [%n] [%l] [%P] %v");
  }
};

/**
 * @brief Testable base: accumulates stdout/stderr and signals completion via
 * an absl::Mutex condition so tests can block until the child finishes.
 *
 * _on_stdout_read() / _on_stderr_read() may be called from different asio
 * threads, so every write to the accumulated strings is done under _mu.
 * _on_process_end() is guaranteed to be the last callback (called only once
 * all reads have produced EOF), so reading _stdout / _stderr after wait()
 * returns is safe.
 */
class fork_wait : public com::centreon::common::fork<true> {
  absl::Mutex _mu;
  bool _completed = false;

  std::string _stdout;
  std::string _stderr;

 protected:
  void _on_stdout_read(const std::string received) override {
    absl::MutexLock l(&_mu);
    _stdout += received;
  }

  void _on_stderr_read(const std::string received) override {
    absl::MutexLock l(&_mu);
    _stderr += received;
  }

  void _on_process_end() override {
    absl::MutexLock l(&_mu);
    _completed = true;
  }

 public:
  fork_wait(const std::shared_ptr<asio::io_context>& io_context,
            const std::shared_ptr<spdlog::logger>& logger)
      : fork<true>(io_context, logger) {}

  const std::string& get_stdout() const { return _stdout; }
  const std::string& get_stderr() const { return _stderr; }

  void wait() {
    absl::MutexLock l(&_mu);
    if (!_completed) {
      _mu.Await(absl::Condition(&_completed));
    }
  }
};

// ---------------------------------------------------------------------------
// Concrete fork subclasses used in tests
// ---------------------------------------------------------------------------

/** Child that exits with a configurable exit code. */
class fork_exit_code : public fork_wait {
  int _code;

 protected:
  int _run(int /*stdin_fd*/, int /*stdout_fd*/, int /*stderr_fd*/) override {
    return _code;
  }

 public:
  fork_exit_code(const std::shared_ptr<asio::io_context>& io_context,
                 const std::shared_ptr<spdlog::logger>& logger,
                 int code)
      : fork_wait(io_context, logger), _code(code) {}
};

/** Child that writes a fixed string to stdout then exits 0. */
class fork_write_stdout : public fork_wait {
  std::string _msg;

 protected:
  int _run(int /*stdin_fd*/, int stdout_fd, int /*stderr_fd*/) override {
    ::write(stdout_fd, _msg.data(), _msg.size());
    return 0;
  }

 public:
  fork_write_stdout(const std::shared_ptr<asio::io_context>& io_context,
                    const std::shared_ptr<spdlog::logger>& logger,
                    std::string msg)
      : fork_wait(io_context, logger), _msg(std::move(msg)) {}
};

/** Child that writes a fixed string to stderr then exits 0. */
class fork_write_stderr : public fork_wait {
  std::string _msg;

 protected:
  int _run(int /*stdin_fd*/, int /*stdout_fd*/, int stderr_fd) override {
    ::write(stderr_fd, _msg.data(), _msg.size());
    return 0;
  }

 public:
  fork_write_stderr(const std::shared_ptr<asio::io_context>& io_context,
                    const std::shared_ptr<spdlog::logger>& logger,
                    std::string msg)
      : fork_wait(io_context, logger), _msg(std::move(msg)) {}
};

/** Child that reads from stdin until EOF and echoes every byte to stdout. */
class fork_echo_stdin : public fork_wait {
 protected:
  int _run(int stdin_fd, int stdout_fd, int /*stderr_fd*/) override {
    char buf[256];
    ssize_t n;
    while ((n = ::read(stdin_fd, buf, sizeof(buf))) > 0) {
      ::write(stdout_fd, buf, n);
    }
    return 0;
  }

 public:
  fork_echo_stdin(const std::shared_ptr<asio::io_context>& io_context,
                  const std::shared_ptr<spdlog::logger>& logger)
      : fork_wait(io_context, logger) {}
};

/** Child that sleeps for a given number of seconds then exits 0. */
class fork_sleep : public fork_wait {
  unsigned _seconds;

 protected:
  int _run(int /*stdin_fd*/, int /*stdout_fd*/, int /*stderr_fd*/) override {
    ::sleep(_seconds);
    return 0;
  }

 public:
  fork_sleep(const std::shared_ptr<asio::io_context>& io_context,
             const std::shared_ptr<spdlog::logger>& logger,
             unsigned seconds)
      : fork_wait(io_context, logger), _seconds(seconds) {}
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

/** Child returns 0: parent sees exit code 0 and normal exit status. */
TEST_F(fork_test, exit_code_zero) {
  auto proc = std::make_shared<fork_exit_code>(g_io_context, _logger, 0);
  proc->do_fork(false);
  proc->wait();
  EXPECT_EQ(proc->get_exit_code(), 0);
  EXPECT_EQ(proc->get_exit_status(), e_exit_status::normal);
  EXPECT_EQ(proc->get_stdout(), "");
  EXPECT_EQ(proc->get_stderr(), "");
}

/** Child returns non-zero: parent sees the exact exit code. */
TEST_F(fork_test, exit_code_nonzero) {
  auto proc = std::make_shared<fork_exit_code>(g_io_context, _logger, 42);
  proc->do_fork(false);
  proc->wait();
  EXPECT_EQ(proc->get_exit_code(), 42);
  EXPECT_EQ(proc->get_exit_status(), e_exit_status::normal);
}

/** PID returned by get_pid() must be > 0 after do_fork(). */
TEST_F(fork_test, pid_valid) {
  auto proc = std::make_shared<fork_exit_code>(g_io_context, _logger, 0);
  proc->do_fork(false);
  EXPECT_GT(proc->get_pid(), 0);
  proc->wait();
}

/** Bytes written to stdout by the child are captured intact. */
TEST_F(fork_test, stdout_capture) {
  const std::string msg = "hello from child stdout\n";
  auto proc = std::make_shared<fork_write_stdout>(g_io_context, _logger, msg);
  proc->do_fork(false);
  proc->wait();
  EXPECT_EQ(proc->get_exit_code(), 0);
  EXPECT_EQ(proc->get_stdout(), msg);
  EXPECT_EQ(proc->get_stderr(), "");
}

/** Bytes written to stderr by the child are captured intact. */
TEST_F(fork_test, stderr_capture) {
  const std::string msg = "error from child stderr\n";
  auto proc = std::make_shared<fork_write_stderr>(g_io_context, _logger, msg);
  proc->do_fork(true);
  proc->wait();
  EXPECT_EQ(proc->get_exit_code(), 0);
  EXPECT_EQ(proc->get_stdout(), "");
  EXPECT_EQ(proc->get_stderr(), msg);
}

/**
 * Data sent by the parent via write_to_child_stdin() is echoed back verbatim on
 * stdout once the child's stdin is closed.
 */
TEST_F(fork_test, stdin_echo) {
  auto proc = std::make_shared<fork_echo_stdin>(g_io_context, _logger);
  proc->do_fork(false);

  std::string expected;
  for (int i = 0; i < 5; ++i) {
    std::string line = fmt::format("line{}\n", i);
    proc->write_to_child_stdin(line);
    expected += line;
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  proc->close_stdin();
  proc->wait();
  EXPECT_EQ(proc->get_exit_code(), 0);
  EXPECT_EQ(proc->get_stdout(), expected);
}

/**
 * is_alive() returns true while the child is sleeping, and the child is no
 * longer visible to the kernel after kill() + wait().
 */
TEST_F(fork_test, is_alive_and_kill) {
  auto proc = std::make_shared<fork_sleep>(g_io_context, _logger, 30);
  proc->do_fork(false);

  // Give the child a moment to be scheduled
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_TRUE(proc->is_alive());

  int pid = proc->get_pid();
  proc->kill();
  proc->wait();

  // After the process has been reaped, sending signal 0 should fail
  EXPECT_EQ(::kill(pid, 0), -1);
  EXPECT_EQ(errno, ESRCH);
}

/**
 * Large stdout (> 4096 bytes): all data crosses the pipe buffer boundary and
 * arrives complete and in order.
 */
TEST_F(fork_test, large_stdout_complete) {
  // Child writes 20000 bytes of cycling digits "0123456789..."
  class fork_large_stdout : public fork_wait {
   protected:
    int _run(int /*stdin_fd*/, int stdout_fd, int /*stderr_fd*/) override {
      char buf[10];
      for (int i = 0; i < 2000; ++i) {
        for (int j = 0; j < 10; ++j)
          buf[j] = static_cast<char>('0' + j);
        ::write(stdout_fd, buf, 10);
      }
      return 0;
    }

   public:
    fork_large_stdout(const std::shared_ptr<asio::io_context>& io,
                      const std::shared_ptr<spdlog::logger>& log)
        : fork_wait(io, log) {}
  };

  auto proc = std::make_shared<fork_large_stdout>(g_io_context, _logger);
  proc->do_fork(false);
  proc->wait();

  const std::string& out = proc->get_stdout();
  ASSERT_EQ(out.size(), 20000u);
  for (size_t i = 0; i < out.size(); ++i) {
    EXPECT_EQ(out[i], static_cast<char>('0' + i % 10))
        << "mismatch at byte " << i;
    if (out[i] != static_cast<char>('0' + i % 10))
      break;
  }
}
