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

#include <absl/synchronization/mutex.h>
#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>
#include <chrono>
#include <ctime>
#include <thread>

#include "com/centreon/connector/log.hh"
#include "com/centreon/connector/perl/script_child.hh"

using namespace com::centreon::connector::perl;
namespace asio = boost::asio;

extern std::shared_ptr<asio::io_context> g_io_context;

namespace {

// Write a Perl script to a temporary file and return its path.
// The caller is responsible for calling unlink() on the returned path.
std::string write_temp_script(const std::string& content) {
  char path[] = "/tmp/centreon_sc_test_XXXXXX";
  int fd = ::mkstemp(path);
  if (fd < 0) {
    ADD_FAILURE() << "mkstemp failed: " << strerror(errno);
    return {};
  }
  ssize_t written = ::write(fd, content.data(), content.size());
  EXPECT_EQ(written, static_cast<ssize_t>(content.size()));
  ::close(fd);
  ::chmod(path, 0644);
  return path;
}

// Build an Execute message with the given cmd_id and a 60-second deadline.
ConnectorMess make_execute(uint64_t cmd_id) {
  ConnectorMess msg;
  auto* ex = msg.mutable_execute();
  ex->set_cmd_id(cmd_id);
  ex->set_timeout(static_cast<uint32_t>(std::time(nullptr) + 60));
  return msg;
}

// Like make_execute but sets no_child_create=true so the request is queued
// rather than spawning a new check_child when all workers are busy.
ConnectorMess make_execute_no_create(uint64_t cmd_id) {
  ConnectorMess msg = make_execute(cmd_id);
  msg.mutable_execute()->set_no_child_create(true);
  return msg;
}

// Build an Execute message with explicit max_execute limit.
ConnectorMess make_execute_with_max(uint64_t cmd_id, uint32_t max_execute) {
  ConnectorMess msg = make_execute(cmd_id);
  msg.mutable_execute()->set_max_execute(max_execute);
  return msg;
}

}  // namespace

/**
 * @brief Fixture for script_child integration tests.
 *
 * Each test forks a real script_child process that initialises its own Perl
 * interpreter, compiles the Perl check script, and then processes execute
 * requests received over stdin.  Results and termination events are collected
 * through the two callbacks and made available to assertions via the
 * wait_for_messages() / wait_for_end() helpers.
 *
 * Teardown kills the child process if it is still alive and removes any
 * temporary file created by the test.
 */
class ScriptChildTest : public ::testing::Test {
 protected:
  std::vector<ConnectorMess> _received;
  bool _ended = false;
  absl::Mutex _mu;
  std::shared_ptr<script_child> _child;
  std::string _temp_script;
  size_t _wait_count = 0;

  // ---- message-count condition used with absl::Await ----------------------

  bool received_enough() ABSL_SHARED_LOCKS_REQUIRED(_mu) {
    return _received.size() >= _wait_count;
  }
  bool process_ended() ABSL_SHARED_LOCKS_REQUIRED(_mu) { return _ended; }

  // ---- helpers -------------------------------------------------------------

  /**
   * @brief Create and fork a script_child for the given script file.
   *
   * @param script_path     Path to the Perl check script to compile.
   * @param additional_code Optional Perl code appended to the loader.
   */
  void fork_child(const std::string& script_path) {
    _child = std::make_shared<script_child>(
        g_io_context, com::centreon::connector::log::core(), script_path,
        [this](const std::shared_ptr<script_child>&, const ConnectorMess& msg) {
          SPDLOG_LOGGER_DEBUG(com::centreon::connector::log::core(),
                              "main process receive {}",
                              msg.ShortDebugString());
          absl::MutexLock l(&_mu);
          _received.push_back(msg);
        },
        [this](const std::shared_ptr<script_child>&) {
          absl::MutexLock l(&_mu);
          _ended = true;
        },
        config(0, nullptr));
    _child->do_fork(false);
  }

  /**
   * @brief Block until at least @p n messages have arrived or the timeout
   *        expires.
   *
   * @param n              Minimum number of messages to wait for.
   * @param timeout_secs   Seconds before giving up (default 15).
   * @return true if the condition was met before the deadline.
   */
  bool wait_for_messages(size_t n, int timeout_secs = 15) {
    _wait_count = n;
    auto deadline = absl::Now() + absl::Seconds(timeout_secs);
    absl::MutexLock l(&_mu);
    return _mu.AwaitWithDeadline(
        absl::Condition(this, &ScriptChildTest::received_enough), deadline);
  }

  /**
   * @brief Block until the child's end-of-life callback has fired.
   */
  bool wait_for_end(int timeout_secs = 15) {
    auto deadline = absl::Now() + absl::Seconds(timeout_secs);
    absl::MutexLock l(&_mu);
    return _mu.AwaitWithDeadline(
        absl::Condition(this, &ScriptChildTest::process_ended), deadline);
  }

  void TearDown() override {
    if (_child) {
      _child->kill();
      _child.reset();
    }
    if (!_temp_script.empty()) {
      ::unlink(_temp_script.c_str());
    }
  }
};

// ============================================================
//  Startup failure cases
// ============================================================

/**
 * @brief When the check script does not exist, the child sends a
 *        have_to_terminate message whose error field is non-empty.
 *
 * _load_check_script() fails to stat the file, sets _global_error, and the
 * child serialises a have_to_terminate message before exiting.
 */
TEST_F(ScriptChildTest, ScriptFileNotFound) {
  fork_child("/tmp/this_check_script_does_not_exist_99999");

  ASSERT_TRUE(wait_for_messages(1))
      << "Timed out waiting for have_to_terminate";

  absl::MutexLock l(&_mu);
  ASSERT_EQ(_received.size(), 1u);
  EXPECT_TRUE(_received[0].has_have_to_terminate());
  EXPECT_FALSE(_received[0].have_to_terminate().error().empty());
}

/**
 * @brief When the check script contains a Perl syntax error, the child sends
 *        a have_to_terminate message reporting the compilation failure.
 *
 * eval_file wraps the script in a subroutine and evals it; a syntax error
 * propagates as a die that sets ERRSV, which _load_check_script() converts
 * into an exception that populates _global_error.
 */
TEST_F(ScriptChildTest, ScriptSyntaxError) {
  _temp_script = write_temp_script("my $x = {;\n");  // unmatched brace
  ASSERT_FALSE(_temp_script.empty());

  fork_child(_temp_script);

  ASSERT_TRUE(wait_for_messages(1))
      << "Timed out waiting for have_to_terminate";

  absl::MutexLock l(&_mu);
  ASSERT_EQ(_received.size(), 1u);
  EXPECT_TRUE(_received[0].has_have_to_terminate());
  EXPECT_FALSE(_received[0].have_to_terminate().error().empty());
}

// ============================================================
//  Successful execution
// ============================================================

/**
 * @brief A script that prints one line and exits 0 produces a result with
 *        status = 0 and the expected stdout content.
 *
 * The exit() override in the loader captures the exit code and writes it to
 * STDERR.  check_child reads both pipes and assembles the final result.
 */
TEST_F(ScriptChildTest, OkCheck) {
  _temp_script = write_temp_script(
      "print \"OK - test check passed\\n\";\n"
      "exit(0);\n");
  ASSERT_FALSE(_temp_script.empty());

  fork_child(_temp_script);
  _child->write_mess_to_child_stdin(make_execute(1));

  ASSERT_TRUE(wait_for_messages(1)) << "Timed out waiting for result";

  absl::MutexLock l(&_mu);
  ASSERT_EQ(_received.size(), 1u);
  ASSERT_TRUE(_received[0].has_result());
  EXPECT_EQ(_received[0].result().cmd_id(), 1u);
  EXPECT_EQ(_received[0].result().status(), 0);
  EXPECT_NE(_received[0].result().stdout().find("OK"), std::string::npos);
}

/**
 * @brief A script that exits 1 produces a result with status = 1 (WARNING).
 */
TEST_F(ScriptChildTest, WarningCheck) {
  _temp_script = write_temp_script(
      "print \"WARNING - something looks off\\n\";\n"
      "exit(1);\n");
  ASSERT_FALSE(_temp_script.empty());

  fork_child(_temp_script);
  _child->write_mess_to_child_stdin(make_execute(2));

  ASSERT_TRUE(wait_for_messages(1)) << "Timed out waiting for result";

  absl::MutexLock l(&_mu);
  ASSERT_EQ(_received.size(), 1u);
  ASSERT_TRUE(_received[0].has_result());
  EXPECT_EQ(_received[0].result().cmd_id(), 2u);
  EXPECT_EQ(_received[0].result().status(), 1);
}

/**
 * @brief A script that exits 2 produces a result with status = 2 (CRITICAL).
 */
TEST_F(ScriptChildTest, CriticalCheck) {
  _temp_script = write_temp_script(
      "print \"CRITICAL - something is wrong\\n\";\n"
      "exit(2);\n");
  ASSERT_FALSE(_temp_script.empty());

  fork_child(_temp_script);
  _child->write_mess_to_child_stdin(make_execute(3));

  ASSERT_TRUE(wait_for_messages(1)) << "Timed out waiting for result";

  absl::MutexLock l(&_mu);
  ASSERT_EQ(_received.size(), 1u);
  ASSERT_TRUE(_received[0].has_result());
  EXPECT_EQ(_received[0].result().cmd_id(), 3u);
  EXPECT_EQ(_received[0].result().status(), 2);
}

/**
 * @brief The cmd_id in the execute request is echoed back unchanged in the
 *        result message.
 *
 * The check_child passes the full execute ConnectorMess to Perl without
 * inspecting cmd_id; the result is synthesised by the check_child and the
 * cmd_id copied from the pending request map via the result message's own
 * field — verifying that the identifier round-trip is intact.
 */
TEST_F(ScriptChildTest, CmdIdRoundTrip) {
  _temp_script = write_temp_script("exit(0);\n");
  ASSERT_FALSE(_temp_script.empty());

  constexpr uint64_t kId = 0xDEADBEEF;
  fork_child(_temp_script);
  _child->write_mess_to_child_stdin(make_execute(kId));

  ASSERT_TRUE(wait_for_messages(1)) << "Timed out waiting for result";

  absl::MutexLock l(&_mu);
  ASSERT_TRUE(_received[0].has_result());
  EXPECT_EQ(_received[0].result().cmd_id(), kId);
}

// ============================================================
//  Sequential multiple checks
// ============================================================

/**
 * @brief Three sequential execute requests each produce exactly one result.
 *
 * Each request is sent only after the previous result has been received, so
 * the test avoids any interaction with the is_running() dispatch flag.
 * The cmd_id of each result must match the corresponding request.
 */
TEST_F(ScriptChildTest, ThreeSequentialChecks) {
  _temp_script = write_temp_script(
      "print \"OK\\n\";\n"
      "exit(0);\n");
  ASSERT_FALSE(_temp_script.empty());

  fork_child(_temp_script);

  for (uint64_t id = 10; id <= 12; ++id) {
    SPDLOG_LOGGER_DEBUG(com::centreon::connector::log::core(), "try {}",
                        id - 9);
    _child->write_mess_to_child_stdin(make_execute(id));
    ASSERT_TRUE(wait_for_messages(id - 9))
        << "Timed out waiting for result " << id;

    absl::MutexLock l(&_mu);
    ASSERT_GE(_received.size(), id - 9);
    const ConnectorMess& last = _received.back();
    ASSERT_TRUE(last.has_result()) << "Expected result for cmd_id=" << id;
    EXPECT_EQ(last.result().cmd_id(), id);
    EXPECT_EQ(last.result().status(), 0);
  }
}

// ============================================================
//  Load metrics in result
// ============================================================

/**
 * @brief After a successful check the result carries non-zero afterfirstcheck
 *        and afterlastcheck load metrics set by check_child::measure_load().
 *
 * Verifies that the "baseline after first check" metrics are populated and
 * that nb_threads > 0 (the child process itself counts as at least one
 * thread).
 */
TEST_F(ScriptChildTest, ResultContainsLoadMetrics) {
  _temp_script = write_temp_script("exit(0);\n");
  ASSERT_FALSE(_temp_script.empty());

  fork_child(_temp_script);
  _child->write_mess_to_child_stdin(make_execute(99));

  ASSERT_TRUE(wait_for_messages(1)) << "Timed out waiting for result";

  absl::MutexLock l(&_mu);
  ASSERT_TRUE(_received[0].has_result());
  const auto& res = _received[0].result();
  EXPECT_GT(res.afterfirstcheck().nb_thread(), 0u);
  EXPECT_GT(res.afterlastcheck().nb_thread(), 0u);
  // Memory usage of a live process is always > 0.
  EXPECT_GT(res.afterfirstcheck().used_memory(), 0u);
}

// ============================================================
//  Concurrent checks (pool creation)
// ============================================================

/**
 * @brief Five execute requests sent without waiting for results each create
 *        their own check_child and all return a result.
 *
 * With _running correctly set to true inside execute(), subsequent requests
 * see every existing check_child as busy and spawn a new one instead of
 * reusing it.  All five results must be delivered and their cmd_ids must
 * round-trip intact.
 */
TEST_F(ScriptChildTest, ConcurrentChecks) {
  _temp_script = write_temp_script(
      "print \"OK\\n\";\n"
      "exit(0);\n");
  ASSERT_FALSE(_temp_script.empty());

  fork_child(_temp_script);

  constexpr size_t kCount = 5;
  for (uint64_t id = 1; id <= kCount; ++id) {
    _child->write_mess_to_child_stdin(make_execute(id));
  }

  ASSERT_TRUE(wait_for_messages(kCount)) << "Timed out waiting for 5 results";

  absl::MutexLock l(&_mu);
  ASSERT_EQ(_received.size(), kCount);
  std::set<uint64_t> ids;
  for (const auto& msg : _received) {
    ASSERT_TRUE(msg.has_result());
    EXPECT_EQ(msg.result().status(), 0);
    ids.insert(msg.result().cmd_id());
  }
  EXPECT_EQ(ids, (std::set<uint64_t>{1, 2, 3, 4, 5}));
}

// ============================================================
//  Script file update detection
// ============================================================

/**
 * @brief When the check script's mtime is advanced after the child has loaded
 *        it, the every-second timer fires and sends a have_to_terminate message
 *        whose error field mentions "updated".
 *
 * This validates the _start_every_second_timer() call added to _run() and the
 * mtime-comparison logic in _every_second_timer_handler().
 */
TEST_F(ScriptChildTest, ScriptFileUpdatedTriggersReload) {
  _temp_script = write_temp_script("exit(0);\n");
  ASSERT_FALSE(_temp_script.empty());

  fork_child(_temp_script);

  // Confirm the child is operational before touching the file.
  _child->write_mess_to_child_stdin(make_execute(1));
  ASSERT_TRUE(wait_for_messages(1)) << "Child not ready in time";

  // Set the script mtime 2 minutes into the future so it differs from the
  // value recorded by _load_check_script() regardless of clock resolution.
  struct timespec times[2];
  clock_gettime(CLOCK_REALTIME, &times[0]);
  times[1].tv_sec = times[0].tv_sec + 120;
  times[1].tv_nsec = 0;
  times[0] = times[1];
  ASSERT_EQ(utimensat(AT_FDCWD, _temp_script.c_str(), times, 0), 0)
      << "utimensat failed: " << strerror(errno);

  // The timer fires every second; give it up to 5 s.
  ASSERT_TRUE(wait_for_messages(2, 5))
      << "No have_to_terminate received after mtime change";

  absl::MutexLock l(&_mu);
  bool found = false;
  for (const auto& msg : _received) {
    if (msg.has_have_to_terminate()) {
      found = true;
      EXPECT_NE(msg.have_to_terminate().error().find("updated"),
                std::string::npos);
    }
  }
  EXPECT_TRUE(found) << "have_to_terminate not found among received messages";
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

// ============================================================
//  Large stdout buffering
// ============================================================

/**
 * @brief A script that emits ~50 KB forces the poll loop in check_child::_run()
 *        to iterate more than once (buffer is 4 096 bytes).
 *
 * Verifies that all output is collected and forwarded correctly.
 */
TEST_F(ScriptChildTest, LargeStdout) {
  _temp_script = write_temp_script(
      "print \"A\" x 50000;\n"
      "print \"\\n\";\n"
      "exit(0);\n");
  ASSERT_FALSE(_temp_script.empty());

  fork_child(_temp_script);
  _child->write_mess_to_child_stdin(make_execute(7));

  ASSERT_TRUE(wait_for_messages(1)) << "Timed out waiting for result";

  absl::MutexLock l(&_mu);
  ASSERT_TRUE(_received[0].has_result());
  EXPECT_EQ(_received[0].result().status(), 0);
  EXPECT_GE(_received[0].result().stdout().size(), 50000u);
}

// ============================================================
//  Queued execution with no_child_create
// ============================================================

// ============================================================
//  check_child crash while queue is non-empty
// ============================================================

/**
 * @brief When a check_child is killed unexpectedly while a request sits in
 *        _execute_queue, a new check_child must be spawned to service it.
 *
 * The Perl script sleeps 2 s then executes "kill 9, $$", which delivers
 * SIGKILL to the check_child process itself.  _on_child_script_end() fires,
 * synthesizes a status-3 result for the in-flight request, and — because the
 * queue is non-empty — immediately creates a replacement check_child for the
 * queued request.  The replacement runs the same script and also dies, so
 * both cmd_ids end up with status 3.  Four messages are expected in total:
 * child_end(A), result(200, 3), child_end(B), result(201, 3).
 */
TEST_F(ScriptChildTest, CheckChildDiesNewOneCreatedForQueue) {
  _temp_script = write_temp_script(
      "sleep(2);\n"
      "kill 9, $$;\n");
  ASSERT_FALSE(_temp_script.empty());

  fork_child(_temp_script);

  // First request: creates check_child A (which will sleep then self-destruct).
  _child->write_mess_to_child_stdin(make_execute(200));
  // Second request: no_child_create=true → queued while A is running.
  _child->write_mess_to_child_stdin(make_execute_no_create(201));

  // child_end(A) + result(200,3) + child_end(B) + result(201,3) = 4 messages.
  ASSERT_TRUE(wait_for_messages(4, 30))
      << "Timed out: check_child death did not trigger queue dispatch";

  absl::MutexLock l(&_mu);
  std::map<uint64_t, int> id_to_status;
  int child_end_count = 0;
  for (const auto& msg : _received) {
    if (msg.has_result())
      id_to_status[msg.result().cmd_id()] = msg.result().status();
    if (msg.has_child_end())
      ++child_end_count;
  }
  EXPECT_EQ(id_to_status.count(200u), 1u) << "No result for cmd_id=200";
  EXPECT_EQ(id_to_status[200], 3) << "cmd_id=200 expected status=3 (died)";
  EXPECT_EQ(id_to_status.count(201u), 1u)
      << "No result for cmd_id=201 (queue was not dispatched)";
  EXPECT_EQ(id_to_status[201], 3) << "cmd_id=201 expected status=3 (died)";
  EXPECT_EQ(child_end_count, 2) << "Expected 2 child_end messages";
}

// ============================================================
//  max_execute limit
// ============================================================

/**
 * @brief When a check_child reaches its max_execute ceiling it is killed and
 *        the main process receives a child_end notification.
 *
 * Two sequential execute requests share the same check_child (max_execute=2).
 * After the second execution the counter equals the limit, so the child is
 * killed.  Three messages arrive in total: result(300, 0), result(301, 0),
 * and child_end.  The result for cmd_id=301 must precede or coincide with the
 * child_end — both are present when the test completes.
 */
TEST_F(ScriptChildTest, CheckChildKilledAfterMaxExecute) {
  _temp_script = write_temp_script(
      "print \"OK\\n\";\n"
      "exit(0);\n");
  ASSERT_FALSE(_temp_script.empty());

  fork_child(_temp_script);

  // First execute: check_child A created, execute_counter → 1, limit=2 → keep.
  _child->write_mess_to_child_stdin(make_execute_with_max(300, 2));
  ASSERT_TRUE(wait_for_messages(1, 15)) << "Timed out waiting for first result";
  {
    absl::MutexLock l(&_mu);
    ASSERT_TRUE(_received[0].has_result());
    EXPECT_EQ(_received[0].result().cmd_id(), 300u);
    EXPECT_EQ(_received[0].result().status(), 0);
  }

  // Second execute: reuses A (idle), execute_counter → 2, limit=2 → kill A.
  _child->write_mess_to_child_stdin(make_execute_with_max(301, 2));
  // Expect result(301) + child_end(A) on top of the already-received
  // result(300).
  ASSERT_TRUE(wait_for_messages(3, 15))
      << "Timed out waiting for second result and child_end";

  absl::MutexLock l(&_mu);
  bool has_result_301 = false;
  bool has_child_end = false;
  for (const auto& msg : _received) {
    if (msg.has_result() && msg.result().cmd_id() == 301u) {
      EXPECT_EQ(msg.result().status(), 0);
      has_result_301 = true;
    }
    if (msg.has_child_end())
      has_child_end = true;
  }
  EXPECT_TRUE(has_result_301) << "Expected result for cmd_id=301";
  EXPECT_TRUE(has_child_end) << "Expected child_end after max_execute reached";
}

// ============================================================
//  Queued execution with no_child_create
// ============================================================

// ============================================================
//  Perl @ARGV parameter passing
// ============================================================

/**
 * @brief Args set in Execute.args() are accessible as @ARGV inside the Perl
 *        check script: @ARGV[0] is the script path and subsequent elements
 *        map to Execute.args() in order.
 *
 * Sends four arguments ("--host", "192.168.1.1", "--port", "8080") and
 * verifies that the script echoes all of them back via stdout.
 */
TEST_F(ScriptChildTest, ScriptReceivesMultipleArgs) {
  _temp_script = write_temp_script(
      "my @args = @ARGV[1..$#ARGV];\n"  // skip script path at ARGV[0]
      "print join(',', @args), \"\\n\";\n"
      "exit(0);\n");
  ASSERT_FALSE(_temp_script.empty());

  fork_child(_temp_script);

  ConnectorMess msg = make_execute(50);
  msg.mutable_execute()->add_args("--host");
  msg.mutable_execute()->add_args("192.168.1.1");
  msg.mutable_execute()->add_args("--port");
  msg.mutable_execute()->add_args("8080");
  _child->write_mess_to_child_stdin(msg);

  ASSERT_TRUE(wait_for_messages(1)) << "Timed out waiting for result";

  absl::MutexLock l(&_mu);
  ASSERT_TRUE(_received[0].has_result());
  EXPECT_EQ(_received[0].result().cmd_id(), 50u);
  EXPECT_EQ(_received[0].result().status(), 0);
  const std::string& out = _received[0].result().stdout();
  EXPECT_NE(out.find("--host"), std::string::npos);
  EXPECT_NE(out.find("192.168.1.1"), std::string::npos);
  EXPECT_NE(out.find("--port"), std::string::npos);
  EXPECT_NE(out.find("8080"), std::string::npos);
}

/**
 * @brief Each sequential execute request receives its own independent @ARGV.
 *
 * Three requests are sent one after the other, each carrying a distinct
 * "--target" value. The test verifies that every result echoes back the correct
 * target for that specific request, proving that @ARGV is reset per call and
 * not shared across invocations of the same check_child.
 */
TEST_F(ScriptChildTest, ScriptArgsDifferPerSequentialCall) {
  _temp_script = write_temp_script(
      "my @args = @ARGV[1..$#ARGV];\n"
      "print join(',', @args), \"\\n\";\n"
      "exit(0);\n");
  ASSERT_FALSE(_temp_script.empty());

  fork_child(_temp_script);

  const std::vector<std::string> targets = {"server-alpha", "server-beta",
                                            "server-gamma"};
  for (uint64_t i = 0; i < targets.size(); ++i) {
    ConnectorMess msg = make_execute(60 + i);
    msg.mutable_execute()->add_args("--target");
    msg.mutable_execute()->add_args(targets[i]);
    _child->write_mess_to_child_stdin(msg);
    ASSERT_TRUE(wait_for_messages(i + 1))
        << "Timed out waiting for result " << i + 1;

    absl::MutexLock l(&_mu);
    const ConnectorMess& last = _received.back();
    ASSERT_TRUE(last.has_result());
    EXPECT_EQ(last.result().cmd_id(), 60 + i);
    EXPECT_EQ(last.result().status(), 0);
    EXPECT_NE(last.result().stdout().find(targets[i]), std::string::npos)
        << "Expected '" << targets[i] << "' in stdout for cmd_id=" << 60 + i;
    EXPECT_EQ(last.result().stdout().find("server-alpha") != std::string::npos,
              i == 0)
        << "server-alpha should only appear in the first result";
  }
}

// ============================================================
//  Queued execution with no_child_create
// ============================================================

/**
 * @brief When no_child_create=true and the only check_child is running, the
 *        request is queued and dispatched once the check_child becomes idle.
 *
 * A 1-second sleep in the script guarantees the second message is processed
 * by script_child while the first check_child is still busy, so the queue
 * code path is always exercised.  Both cmd_ids must appear in the results.
 */
TEST_F(ScriptChildTest, QueuedExecuteWithNoChildCreate) {
  _temp_script = write_temp_script(
      "sleep(1);\n"
      "exit(0);\n");
  ASSERT_FALSE(_temp_script.empty());

  fork_child(_temp_script);

  // First request creates a check_child (_running → true).
  _child->write_mess_to_child_stdin(make_execute(100));
  // Second request arrives while the check_child is sleeping → queued.
  _child->write_mess_to_child_stdin(make_execute_no_create(101));

  ASSERT_TRUE(wait_for_messages(2, 30)) << "Timed out waiting for both results";

  absl::MutexLock l(&_mu);
  std::set<uint64_t> ids;
  for (const auto& msg : _received) {
    if (msg.has_result()) {
      EXPECT_EQ(msg.result().status(), 0);
      ids.insert(msg.result().cmd_id());
    }
  }
  EXPECT_EQ(ids, (std::set<uint64_t>{100, 101}));
}
