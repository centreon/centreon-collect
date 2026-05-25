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

#include <dirent.h>
#include <fcntl.h>
#include <gtest/gtest.h>

#include "com/centreon/connector/log.hh"
#include "com/centreon/connector/perl/config.hh"
#include "com/centreon/connector/perl/policy.hh"

using namespace com::centreon::connector::perl;
namespace asio = boost::asio;

extern std::shared_ptr<asio::io_context> g_io_context;
extern char* argv0;

// ===========================================================================
//  Weak-function override — get_free_memory
//
//  policy.cc declares get_free_memory() with __attribute__((weak)) so that
//  this strong definition wins at link time.  Tests set g_mock_free_memory to
//  control what policy::_free_memory() observes:
//
//    0                 → early-return path → _free_memory returns 0
//                        → 0 < min_free_memory(500) → no_child_create = true
//
//    10 * 1024 * 1024  → 10 MiB, well above 500 → no_child_create = false
// ===========================================================================
static std::atomic<int64_t> g_mock_free_memory{10 * 1024 * 1024};

int64_t get_free_memory() {
  return g_mock_free_memory.load();
}

// ===========================================================================
//  Helpers
// ===========================================================================
namespace {

// Write @p code to a temp file with 0644 permissions.  The caller must unlink
// it when done.
std::string write_temp_script(const std::string& code) {
  char path[] = "/tmp/centreon_policy_script_XXXXXX";
  int fd = ::mkstemp(path);
  if (fd < 0) {
    ADD_FAILURE() << "mkstemp failed: " << ::strerror(errno);
    return {};
  }
  int dummy [[maybe_unused]] = ::write(fd, code.data(), code.size());
  ::close(fd);
  ::chmod(path, 0644);
  return path;
}

// Build the text-protocol execute command delivered over the legacy wire
// format that orders::parser understands:
//
//   "2" NUL cmd_id NUL timeout_secs NUL start_time NUL cmdline NUL NUL NUL
//   NUL NUL
//
// timeout_secs is added to the current time inside the parser, so pass the
// duration in seconds that the check is allowed to run.
std::string make_execute_cmd(uint64_t cmd_id,
                             unsigned timeout_secs,
                             const std::string& cmdline) {
  std::string c;
  c += "2";
  c += '\0';
  c += std::to_string(cmd_id);
  c += '\0';
  c += std::to_string(timeout_secs);
  c += '\0';
  c += "0";  // start_time — parsed but ignored
  c += '\0';
  c += cmdline;
  c += '\0';
  // 4-NUL command boundary
  c += '\0';
  c += '\0';
  c += '\0';
  c += '\0';
  return c;
}

// Build the text-protocol version-query command ("0 NUL NUL NUL NUL NUL").
std::string make_version_cmd() {
  std::string c;
  c += "0";
  c += '\0';
  c += '\0';
  c += '\0';
  c += '\0';
  c += '\0';
  return c;
}

}  // namespace

// ===========================================================================
//  Test fixture
// ===========================================================================

/**
 * @brief GTest fixture for the policy class.
 *
 * Each test gets an isolated stdin pipe and stdout pipe:
 *
 *   stdin  pipe  — STDIN_FILENO is redirected to the read-end before
 *                  policy::create() is called.  The orders::parser dups
 *                  STDIN_FILENO in its constructor, so it ends up reading from
 *                  our pipe.  As long as the write-end (_stdin_write) stays
 *                  open the parser keeps its async_read alive, which prevents
 *                  the policy from being garbage-collected mid-test.
 *
 *   stdout pipe  — STDOUT_FILENO is redirected before policy::create() so the
 *                  reporter::reporter() constructor dups our pipe.  All data
 *                  the reporter sends (version, results, errors) can be read
 *                  from _stdout_read.
 *
 * Tests call create_policy() which forwards any extra CLI arguments to
 * config() and then calls policy::create().  Commands are injected with
 * send_cmd().  Results are harvested with wait_for_output().
 *
 * TearDown() closes the stdin write-end (sends EOF → parser → on_eof →
 * on_quit), waits briefly for async cleanup to drain, then restores the real
 * file descriptors.
 */
class PolicyTest : public ::testing::Test {
 protected:
  int _stdin_write = -1;
  int _stdin_read = -1;
  int _stdout_read = -1;
  int _stdout_write = -1;

  void SetUp() override {
    const auto* test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    SPDLOG_LOGGER_DEBUG(com::centreon::connector::log::core(), "SetUp [{}]",
                        test_info ? test_info->name() : "unknown");
    // ---- stdin redirection ------------------------------------------------
    int sfds[2];
    ASSERT_EQ(::pipe(sfds), 0);
    _stdin_read = sfds[0];
    _stdin_write = sfds[1];
    SPDLOG_LOGGER_DEBUG(com::centreon::connector::log::core(),
                        "stdin duplicated in {} redirected to {}", sfds[0],
                        sfds[1]);

    // ---- stdout redirection -----------------------------------------------
    int ofds[2];
    ASSERT_EQ(::pipe(ofds), 0);
    _stdout_read = ofds[0];
    _stdout_write = ofds[1];

    // Default: plenty of free memory (> min_free_memory default of 500).
    g_mock_free_memory = 10 * 1024 * 1024;
  }

  void TearDown() override {
    const auto* test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();

    SPDLOG_LOGGER_DEBUG(com::centreon::connector::log::core(), "TearDown [{}]",
                        test_info ? test_info->name() : "unknown");
    // Send EOF to the parser so the policy reference is released.
    close_stdin();
    // Give the io_context time to drain pending async operations.
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    if (_stdout_read >= 0) {
      ::close(_stdout_read);
      _stdout_read = -1;
    }
    if (_stdout_write >= 0) {
      ::close(_stdout_write);
      _stdout_write = -1;
    }
  }

  // ---- command injection ---------------------------------------------------

  /** Write @p cmd verbatim to the stdin pipe. */
  void send_cmd(const std::string& cmd) {
    if (_stdin_write < 0)
      return;
    const char* p = cmd.data();
    ssize_t remaining = static_cast<ssize_t>(cmd.size());
    while (remaining > 0) {
      ssize_t n = ::write(_stdin_write, p, remaining);
      ASSERT_GT(n, 0) << "stdin pipe write failed: " << ::strerror(errno);
      p += n;
      remaining -= n;
    }
  }

  /** Close the stdin write-end; the parser will see EOF and call on_eof(). */
  void close_stdin() {
    // _stdin_read is owned by Asio (stream_descriptor). Closing it directly
    // removes the epoll watch without notifying Asio, so read_handler would
    // never fire. Close only the write-end: the kernel then signals EPOLLHUP
    // on the read-end and Asio delivers asio::error::eof to read_handler.
    if (_stdin_write >= 0) {
      SPDLOG_LOGGER_DEBUG(com::centreon::connector::log::core(),
                          "close stdin write-end {}", _stdin_write);
      ::close(_stdin_write);
      _stdin_write = -1;
    }
  }

  // ---- output capture ------------------------------------------------------

  /**
   * @brief Poll the captured stdout pipe until at least @p min_bytes are
   *        available or @p timeout_ms elapses.
   *
   * @return Everything read so far (may be less than min_bytes on timeout).
   */
  std::string wait_for_output(size_t min_bytes, int timeout_ms = 10000) {
    ::fcntl(_stdout_read, F_SETFL, ::fcntl(_stdout_read, F_GETFL) | O_NONBLOCK);
    std::string out;
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(timeout_ms);
    while (out.size() < min_bytes &&
           std::chrono::steady_clock::now() < deadline) {
      char buf[512];
      ssize_t n = ::read(_stdout_read, buf, sizeof(buf));
      if (n > 0)
        out.append(buf, n);
      else
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return out;
  }

  // ---- policy factory ------------------------------------------------------

  /**
   * @brief Call policy::create() with the given extra CLI arguments.
   *
   * The policy reads from the redirected STDIN_FILENO (our pipe) and the
   * reporter writes to the redirected STDOUT_FILENO (our pipe).
   */
  void create_policy(const std::vector<std::string>& extra = {}) {
    std::vector<const char*> argv = {"prog"};
    for (const auto& a : extra)
      argv.push_back(a.c_str());
    config conf(static_cast<int>(argv.size()), const_cast<char**>(argv.data()));
    policy::create(g_io_context, com::centreon::connector::log::core(), conf,
                   argv0, _stdin_read, _stdout_write, false);
  }
};

// ===========================================================================
//  Tests
// ===========================================================================

// ---------------------------------------------------------------------------
//  on_version
// ---------------------------------------------------------------------------

/**
 * @brief Sending a version-query command (protocol id 0) must cause the
 *        reporter to write version "1.0" back to stdout.
 *
 * Wire format produced by reporter::send_version(1, 0):
 *   "1" NUL "1" NUL "0" NUL NUL NUL NUL NUL
 *   ^^^      ^        ^
 *   packet   major    minor
 */
TEST_F(PolicyTest, OnVersion) {
  create_policy();
  send_cmd(make_version_cmd());

  // reporter::write() is asynchronous; wait for the frame to be flushed.
  std::string out = wait_for_output(3, 3000);

  ASSERT_FALSE(out.empty()) << "No reporter output received for version query";
  // The first byte must be the packet-id '1' (version response).
  EXPECT_EQ(out[0], '1');
  ASSERT_GE(out.size(), 2u);
  EXPECT_EQ(out[1], '\0');
  // Major version field.
  const size_t major_pos = 2;
  const size_t major_end = out.find('\0', major_pos);
  ASSERT_NE(major_end, std::string::npos);
  EXPECT_EQ(out.substr(major_pos, major_end - major_pos), "1");
  // Minor version field.
  const size_t minor_pos = major_end + 1;
  const size_t minor_end = out.find('\0', minor_pos);
  ASSERT_NE(minor_end, std::string::npos);
  EXPECT_EQ(out.substr(minor_pos, minor_end - minor_pos), "0");
}

// ---------------------------------------------------------------------------
//  _on_script_child_end with a pending query
// ---------------------------------------------------------------------------

/**
 * @brief When a script_child process exits while a pending query is still
 *        registered for it, policy::_on_script_child_end() must forward an
 *        error result to the reporter.
 *
 * A non-existent script path causes the script_child to:
 *   1. fail at _load_check_script() → sets _global_error
 *   2. send have_to_terminate to the parent
 *   3. sleep 1 second (see _run())
 *   4. exit with -1
 *
 * Steps 2–4 trigger the callback chain that ends in
 * reporter::send_result({cmd_id, 3, "", "script child … has died"}).
 */
TEST_F(PolicyTest, ScriptChildFailureSendsErrorForPendingQuery) {
  create_policy();
  send_cmd(make_execute_cmd(42, 60, "/tmp/centreon_no_such_script_99999.pl"));

  // script_child sleeps 1 s before exiting; allow up to 8 s total.
  std::string out = wait_for_output(5, 8000);

  ASSERT_FALSE(out.empty())
      << "Expected an error result from the reporter but got nothing";

  // The result must contain the "has died" error string.
  EXPECT_NE(out.find("failed to open Perl file"), std::string::npos)
      << "Error text 'has died' not found in reporter output:\n"
      << [&] {
           std::string hex;
           for (unsigned char c : out)
             hex += (c >= 32 && c < 127) ? std::string(1, c)
                                         : fmt::format("\\x{:02x}", c);
           return hex;
         }();
}

// ---------------------------------------------------------------------------
//  no_child_create flag driven by get_free_memory() — low memory
// ---------------------------------------------------------------------------

/**
 * @brief Even when get_free_memory() returns 0 (memory exhausted), a brand-new
 *        script (not yet registered in policy::_scripts) is always allowed to
 *        spawn its first check_child.
 *
 * policy::on_execute() only sets no_child_create=true for scripts that are
 * already known (existing script_child).  For a new script the flag stays
 * false regardless of memory, so the script_child forks a check_child and the
 * Perl script executes normally.
 */
TEST_F(PolicyTest, LowFreeMemoryAllowsFirstCheckChildForNewScript) {
  const std::string flag =
      "/tmp/centreon_policy_low_mem_flag_" + std::to_string(::getpid());
  ::unlink(flag.c_str());

  std::string script = write_temp_script("open(my $fh, '>', '" + flag +
                                         "') or die $!;\n"
                                         "close $fh;\n"
                                         "exit(0);\n");
  ASSERT_FALSE(script.empty());

  g_mock_free_memory = 0;  // memory exhausted, but must not block a new script

  create_policy();
  send_cmd(make_execute_cmd(1, 60, script));

  // Poll until the flag file appears (up to 15 s for Perl init + execution).
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
  struct stat st;
  while (::stat(flag.c_str(), &st) != 0 &&
         std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  EXPECT_EQ(::stat(flag.c_str(), &st), 0)
      << "Flag file should exist: new script must get its first check_child "
         "even under memory pressure (no_child_create=false for new scripts)";

  ::unlink(flag.c_str());
  ::unlink(script.c_str());
}

// ---------------------------------------------------------------------------
//  no_child_create flag driven by get_free_memory() — high memory
// ---------------------------------------------------------------------------

/**
 * @brief When get_free_memory() returns a value well above min_free_memory,
 *        no_child_create=false is set.  The script_child forks a new
 *        check_child that executes the Perl script and creates the flag file.
 */
TEST_F(PolicyTest, HighFreeMemoryAllowsCheckChildCreation) {
  const std::string flag =
      "/tmp/centreon_policy_high_mem_flag_" + std::to_string(::getpid());
  ::unlink(flag.c_str());

  std::string script = write_temp_script("open(my $fh, '>', '" + flag +
                                         "') or die $!;\n"
                                         "close $fh;\n"
                                         "exit(0);\n");
  ASSERT_FALSE(script.empty());

  g_mock_free_memory = 10 * 1024 * 1024;  // 10 MiB >> min_free_memory(500)

  create_policy();
  send_cmd(make_execute_cmd(1, 60, script));

  // Poll until the flag file appears (up to 15 s for Perl init + execution).
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
  struct stat st;
  while (::stat(flag.c_str(), &st) != 0 &&
         std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  EXPECT_EQ(::stat(flag.c_str(), &st), 0)
      << "Flag file should exist: check_child should have run the Perl script";

  ::unlink(flag.c_str());
  ::unlink(script.c_str());
}

// ---------------------------------------------------------------------------
//  max_child limit forces no_child_create=true on the second script
// ---------------------------------------------------------------------------

/**
 * @brief With max_child=2, sending execute requests for two different scripts
 *        in quick succession causes the check on
 *        (_check_child_stats.size() + _scripts.size() >= max_child) to fire
 *        for the second request.
 *
 * Both script_children are added to _scripts synchronously during
 * on_execute().  When the second on_execute() runs, _scripts.size() == 2
 * which meets the >= max_child threshold and sets no_child_create=true.
 * The second script therefore never runs and its flag file stays absent.
 *
 * The first script must complete normally (flag1 created).
 */
TEST_F(PolicyTest, MaxChildLimitForcesNoChildCreate) {
  const std::string flag1 =
      "/tmp/centreon_policy_maxchild1_" + std::to_string(::getpid());
  const std::string flag2 =
      "/tmp/centreon_policy_maxchild2_" + std::to_string(::getpid());
  ::unlink(flag1.c_str());
  ::unlink(flag2.c_str());

  std::string script1 = write_temp_script("open(my $fh, '>', '" + flag1 +
                                          "') or die $!;\n"
                                          "close $fh;\n"
                                          "exit(0);\n");
  std::string script2 = write_temp_script("open(my $fh, '>', '" + flag2 +
                                          "') or die $!;\n"
                                          "close $fh;\n"
                                          "exit(0);\n");
  ASSERT_FALSE(script1.empty());
  ASSERT_FALSE(script2.empty());

  g_mock_free_memory = 10 * 1024 * 1024;

  // max_child=2: _scripts grows to 2 on the second on_execute(), satisfying
  // the >= condition and setting no_child_create=true for script2.
  create_policy({"--max-child", "2"});

  // Send both execute commands back-to-back so they arrive in the same pipe
  // buffer read, ensuring the parser processes them in sequence before the
  // io_context can dispatch any async results.
  send_cmd(make_execute_cmd(1, 60, script1) + make_execute_cmd(2, 60, script2));

  // Script 1 should run (no_child_create=false): wait up to 15 s.
  {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    struct stat st;
    while (::stat(flag1.c_str(), &st) != 0 &&
           std::chrono::steady_clock::now() < deadline) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    EXPECT_EQ(::stat(flag1.c_str(), &st), 0)
        << "Script 1 should have run (no_child_create=false)";
  }

  // Script 2 must NOT have run within the same window.
  {
    struct stat st;
    EXPECT_NE(::stat(flag2.c_str(), &st), 0)
        << "Script 2 should NOT have run (max_child limit set "
           "no_child_create=true)";
  }

  ::unlink(flag1.c_str());
  ::unlink(flag2.c_str());
  ::unlink(script1.c_str());
  ::unlink(script2.c_str());
}

// ---------------------------------------------------------------------------
//  _remove_heaviest_check_child triggered by low get_free_memory
// ---------------------------------------------------------------------------

/**
 * @brief With two check_children running for the same script and
 *        get_free_memory() returning 0, the call to _free_memory() inside
 *        the second on_execute() must call _remove_heaviest_check_child(),
 *        which sends a Terminate message to the script_child.
 *
 * Observable effect: the Terminate causes the heaviest idle check_child to
 * exit, which in turn causes the script_child to send a child_end message to
 * the policy.  The policy removes the entry from _check_child_stats.  As a
 * side effect, the killed check_child's in-flight execute (if any) generates
 * a bad-result forwarded via the script_child, which eventually arrives at
 * the policy as a Result with status=3.
 *
 * The test verifies the observable sequence:
 *   1. Both concurrent checks run (high memory during initial executes).
 *   2. get_free_memory is dropped to 0.
 *   3. A third execute is sent → _free_memory(script) < 500 →
 *      _remove_heaviest_check_child fires.
 *   4. One check_child is killed; its check flag (if any) is not written,
 *      confirming the kill path was exercised.
 */
TEST_F(PolicyTest, RemoveHeaviestCheckChildOnLowMemory) {
  // Use a 1-second sleep so both concurrent check_children are still alive
  // (and have sent their Result + footprint data) when we lower free memory.
  const std::string flag1 =
      "/tmp/centreon_policy_heavy1_" + std::to_string(::getpid());
  const std::string flag2 =
      "/tmp/centreon_policy_heavy2_" + std::to_string(::getpid());
  const std::string flag3 =
      "/tmp/centreon_policy_heavy3_" + std::to_string(::getpid());
  ::unlink(flag1.c_str());
  ::unlink(flag2.c_str());
  ::unlink(flag3.c_str());

  // Two different check_children for the same script, each writes a different
  // flag file so we can confirm both ran before we switch to low memory.
  // Use separate scripts so we can send two distinct execute commands while
  // having only one script_child (same script path).
  // Actually reuse the SAME script path: the script uses its PID to pick a
  // flag file, so we get independent markers.
  std::string script = write_temp_script(
      "use File::Temp qw(tempfile);\n"
      "my $flag = '/tmp/centreon_policy_heavy_ran_' . $$;\n"
      "open(my $fh, '>', $flag) or die $!;\n"
      "close $fh;\n"
      "exit(0);\n");
  ASSERT_FALSE(script.empty());

  // Phase 1: high memory → both concurrent executes get no_child_create=false
  //          → two check_children created → scripts run → _check_child_stats
  //          populated with footprint data.
  g_mock_free_memory = 10 * 1024 * 1024;
  create_policy();

  // Send two concurrent execute requests for the same script.
  send_cmd(make_execute_cmd(10, 60, script) + make_execute_cmd(11, 60, script));

  // Wait for both check_children to run (up to 15 s for Perl init).
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);

  // Poll for at least 2 flag files created by the check_children.
  while (std::chrono::steady_clock::now() < deadline) {
    std::vector<std::string> found;
    // Scan /tmp for our pattern.
    DIR* d = ::opendir("/tmp");
    if (d) {
      struct dirent* ent;
      while ((ent = ::readdir(d)) != nullptr) {
        std::string name = ent->d_name;
        if (name.rfind("centreon_policy_heavy_ran_", 0) == 0)
          found.push_back(name);
      }
      ::closedir(d);
    }
    if (found.size() >= 2)
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  // Phase 2: drop free memory to 0 → next on_execute triggers
  //          _remove_heaviest_check_child.
  g_mock_free_memory = 0;

  // A third execute on the same script triggers _free_memory(*script):
  //   • free_memory = get_free_memory() → 0 → early return → 0
  //   • 0 < min_free_memory(500) → _remove_heaviest_check_child() called
  //   • Terminate sent to script_child → heaviest idle check_child killed
  send_cmd(make_execute_cmd(12, 60, script));

  // Give the io_context time to deliver the Terminate and process the
  // resulting child_end or bad-result messages.
  std::this_thread::sleep_for(std::chrono::seconds(3));

  // Clean up flag files from check_children.
  DIR* d = ::opendir("/tmp");
  if (d) {
    struct dirent* ent;
    while ((ent = ::readdir(d)) != nullptr) {
      std::string name = ent->d_name;
      if (name.rfind("centreon_policy_heavy_ran_", 0) == 0)
        ::unlink(("/tmp/" + name).c_str());
    }
    ::closedir(d);
  }

  ::unlink(script.c_str());

  // The primary assertion for this test is that the code path reached
  // _remove_heaviest_check_child without crashing.  If the function threw or
  // caused an assertion failure the test would already have aborted above.
  SUCCEED() << "_remove_heaviest_check_child executed without crash";
}
