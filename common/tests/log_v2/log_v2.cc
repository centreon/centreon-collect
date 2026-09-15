/**
 * Copyright 2023 Centreon (https://www.centreon.com/)
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
#include "common/log_v2/log_v2.hh"
#include <absl/strings/str_split.h>
#include <gtest/gtest.h>
#include <re2/re2.h>
#include <atomic>
#include <fstream>
#include <thread>
#include "common/log_v2/centreon_rotating_file_sink.hh"

using log_v2 = com::centreon::common::log_v2::log_v2;
using config = com::centreon::common::log_v2::config;

class TestLogV2 : public ::testing::Test {
 public:
  //  void SetUp() override {}
  void TearDown() override { log_v2::unload(); }
};

static std::string read_file(const std::string& name) {
  std::ifstream is(name);
  if (is) {
    std::stringstream buffer;
    buffer << is.rdbuf();
    is.close();
    return buffer.str();
  }
  return {};
}

// When the log_v2 is loaded, we can access all its loggers.
TEST_F(TestLogV2, load) {
  log_v2::load("ut_common");
  ASSERT_EQ(log_v2::instance().get(log_v2::CORE)->name(),
            std::string_view("core"));
  ASSERT_EQ(log_v2::instance().get(log_v2::CONFIG)->name(),
            std::string_view("config"));
}

// Given a log_v2 loaded.
// When the level of a logger is info
// Then a log of level debug is not displayed
// When the level of a logger is debug
// Then a log of level debug is also displayed
TEST_F(TestLogV2, LoggerUpdated) {
  log_v2::load("ut_common");
  const auto& core_logger = log_v2::instance().get(log_v2::CORE);
  ASSERT_EQ(core_logger->level(), spdlog::level::info);
  testing::internal::CaptureStdout();
  config cfg("/tmp/test.log", config::logger_type::LOGGER_STDOUT, 0, false,
             false);
  cfg.set_level("core", "info");
  log_v2::instance().apply(cfg);
  core_logger->info("First log");
  core_logger->debug("First debug log");
  cfg.set_level("core", "debug");
  log_v2::instance().apply(cfg);
  ASSERT_EQ(core_logger->level(), spdlog::level::debug);
  core_logger->info("Second log");
  core_logger->debug("Second debug log");
  std::string output = testing::internal::GetCapturedStdout();
  std::cout << "Captured stdout:\n" << output << std::endl;
  /* To match the output, we use regex because of the colored output. */

  std::vector<std::string> lines = absl::StrSplit(output, "\n");
  ASSERT_GE(lines.size(), 3U) << "We should have at least three lines of log";
  ASSERT_TRUE(
      RE2::PartialMatch(lines[0], "\\[core\\] \\[.*info.*\\] First log"))
      << "The first log should be of type 'info'";
  ASSERT_TRUE(
      RE2::PartialMatch(lines[1], "\\[core\\] \\[.*info.*\\] Second log"))
      << "The second log should be of type 'info'";
  ASSERT_TRUE(RE2::PartialMatch(lines[2],
                                "\\[core\\] \\[.*debug.*\\] Second debug log"))
      << "The third log should be of type 'debug'";
  std::filesystem::remove("/tmp/test.log");
}

TEST_F(TestLogV2, Flush) {
  /* We remove the file if it exists */
  struct stat buffer;
  if (stat("/tmp/test.log", &buffer) == 0)
    std::filesystem::remove("/tmp/test.log");

  log_v2::load("ut_common");
  config cfg("/tmp/test.log", config::logger_type::LOGGER_FILE, 3, false,
             false);
  cfg.set_level("core", "debug");
  log_v2::instance().apply(cfg);
  auto logger = log_v2::instance().get(log_v2::CORE);

  logger->debug("log 1");
  std::string content = read_file("/tmp/test.log");
  ASSERT_TRUE(content.empty())
      << "Log flush is run every 3s, we should not have any file for now."
      << std::endl;

  std::this_thread::sleep_for(std::chrono::seconds(1));
  logger->debug("log 2");
  content = read_file("/tmp/test.log");
  ASSERT_TRUE(content.empty())
      << "Log flush is run every 3s, we should not have any file for now."
      << std::endl;

  logger->debug("log 3");
  std::this_thread::sleep_for(std::chrono::seconds(3));
  content = read_file("/tmp/test.log");
  auto spl = absl::StrSplit(content, '\n');
  auto it = spl.begin();
  for (int i = 1; i <= 3; i++) {
    std::string str(fmt::format("log {}", i));
    ASSERT_TRUE(it != spl.end())
        << "We should at least have three lines of log" << std::endl;
    ASSERT_TRUE(it->find(str) != std::string::npos)
        << "The line '" << *it << "' should contain '" << str << "'"
        << std::endl;
    ++it;
  }

  std::filesystem::remove("/tmp/test.log");

  /* The flush is disabled */
  cfg.set_flush_interval(0);
  log_v2::instance().apply(cfg);

  logger->debug("log 1");
  logger->debug("log 2");
  logger->debug("log 3");
  std::this_thread::sleep_for(std::chrono::seconds(1));
  content = read_file("/tmp/test.log");
  std::cout << "Content of the log file:\n" << content << std::endl;
  ASSERT_TRUE(content.find("log 1") != std::string::npos)
      << "Log flush is disabled, we should not have all the logs, here 'log 1'."
      << std::endl;
  ASSERT_TRUE(content.find("log 2") != std::string::npos)
      << "Log flush is disabled, we should not have all the logs, here 'log 2'."
      << std::endl;
  ASSERT_TRUE(content.find("log 3") != std::string::npos)
      << "Log flush is disabled, we should not have all the logs, here 'log 3'."
      << std::endl;

  std::filesystem::remove("/tmp/test.log");
}

// Given a config whose resolved log type (LOGGER_STDOUT, e.g. log_v2_logger
// == "file" with an empty log_file) happens to match log_v2's constructor
// default, and which registers a custom sink for one logger (as engine does
// to forward its logs to broker).
// When apply() is called for the very first time on this instance.
// Then the custom sink is attached even though the type didn't change,
// because without ever recreating the loggers there would be no other place
// to attach it.
TEST_F(TestLogV2, FirstApplyWithCustomSinksAttachesThemEvenWhenTypeUnchanged) {
  std::filesystem::remove("/tmp/test_custom_sink.log");
  log_v2::load("ut_common");

  auto custom_sink =
      std::make_shared<spdlog::sinks::centreon_rotating_file_sink_mt>(
          "/tmp/test_custom_sink.log");

  config cfg("", config::logger_type::LOGGER_STDOUT, 0, false, false);
  cfg.add_custom_sink(custom_sink);
  cfg.apply_custom_sinks({"core"});
  log_v2::instance().apply(cfg);

  auto core_logger = log_v2::instance().get(log_v2::CORE);
  core_logger->info("hello via custom sink");
  core_logger->flush();

  std::string content = read_file("/tmp/test_custom_sink.log");
  ASSERT_NE(content.find("hello via custom sink"), std::string::npos)
      << "A custom sink registered on the very first apply() must be "
         "attached even when the resolved type matches log_v2's initial "
         "LOGGER_STDOUT default";

  std::filesystem::remove("/tmp/test_custom_sink.log");
}

// Given a logger whose level was explicitly set, different from the level a
// fresh spdlog::logger would default to.
// When apply() is called with a different log type, forcing every logger to
// be recreated, and the new configuration does not reconfigure that specific
// logger's level.
// Then the recreated logger keeps the previous level instead of silently
// falling back to spdlog's default.
TEST_F(TestLogV2, RecreationPreservesUnconfiguredLoggerLevel) {
  std::filesystem::remove("/tmp/test_recreate_level.log");
  log_v2::load("ut_common");

  config cfg1("/tmp/test_recreate_level.log", config::logger_type::LOGGER_FILE,
              0, false, false);
  cfg1.set_level("neb", "debug");
  log_v2::instance().apply(cfg1);
  auto neb_logger = log_v2::instance().get(log_v2::NEB);
  ASSERT_EQ(neb_logger->level(), spdlog::level::debug);

  // Different log type forces recreation; "neb" isn't reconfigured this
  // time, so its level must be carried over from the previous logger.
  config cfg2("/tmp/test_recreate_level.log",
              config::logger_type::LOGGER_STDOUT, 0, false, false);
  log_v2::instance().apply(cfg2);
  neb_logger = log_v2::instance().get(log_v2::NEB);
  ASSERT_EQ(neb_logger->level(), spdlog::level::debug)
      << "Recreating loggers on a type change must preserve the level of "
         "loggers the new config doesn't explicitly reconfigure";

  std::filesystem::remove("/tmp/test_recreate_level.log");
}

// Given a log_v2 already switched to a LOGGER_FILE configuration.
// When apply() is called again with the same log type (only atomic changes,
// as happens on every reload after the first one).
// Then the loggers are not recreated: the same logger objects keep being
// used, so callers who cached a shared_ptr via get() keep logging through a
// live, correctly-flushed logger instead of a stale, replaced one.
TEST_F(TestLogV2, TypeUnchangedSkipsRecreation) {
  std::filesystem::remove("/tmp/test_type_unchanged.log");
  log_v2::load("ut_common");

  config cfg("/tmp/test_type_unchanged.log", config::logger_type::LOGGER_FILE,
             0, false, false);
  cfg.set_level("core", "info");
  log_v2::instance().apply(cfg);
  auto logger_after_first = log_v2::instance().get(log_v2::CORE);
  ASSERT_TRUE(logger_after_first);

  cfg.set_level("core", "debug");
  log_v2::instance().apply(cfg);
  auto logger_after_second = log_v2::instance().get(log_v2::CORE);
  ASSERT_EQ(logger_after_first, logger_after_second)
      << "The logger should not be recreated when the log type is unchanged";
  ASSERT_EQ(logger_after_second->level(), spdlog::level::debug);

  std::filesystem::remove("/tmp/test_type_unchanged.log");
}

// Given centengine's own log file already set up (as cbmod does, with
// allow_change_pattern_and_path(false), to avoid a broker-side reload
// stealing centengine's log file).
// When apply() is called with a different log path and
// allow_change_pattern_and_path(false).
// Then the log path is not changed: logging still goes to the original file,
// and the other file is never created.
TEST_F(TestLogV2, AllowChangePatternAndPathFalsePreservesExistingFile) {
  std::filesystem::remove("/tmp/test_path_a.log");
  std::filesystem::remove("/tmp/test_path_b.log");
  log_v2::load("ut_common");

  config cfg_a("/tmp/test_path_a.log", config::logger_type::LOGGER_FILE, 0,
               false, false);
  cfg_a.set_level("core", "info");
  log_v2::instance().apply(cfg_a);
  auto logger = log_v2::instance().get(log_v2::CORE);
  logger->info("to path a");
  logger->flush();
  ASSERT_TRUE(std::filesystem::exists("/tmp/test_path_a.log"));

  config cfg_b("/tmp/test_path_b.log", config::logger_type::LOGGER_FILE, 0,
               false, false);
  cfg_b.allow_only_atomic_changes(true);
  cfg_b.allow_change_pattern_and_path(false);
  cfg_b.set_level("core", "debug");
  log_v2::instance().apply(cfg_b);

  logger->info("still to path a");
  logger->flush();

  ASSERT_FALSE(std::filesystem::exists("/tmp/test_path_b.log"))
      << "allow_change_pattern_and_path(false) must not redirect the log file";
  std::string content = read_file("/tmp/test_path_a.log");
  ASSERT_NE(content.find("still to path a"), std::string::npos);

  std::filesystem::remove("/tmp/test_path_a.log");
}

// Given a log_v2 instance in use.
// When one thread repeatedly calls apply() (forcing sink recreation by
// alternating the log type every time) while another thread concurrently
// logs through a logger shared_ptr obtained via get().
// Then no crash, deadlock, or exception occurs: _loggers_m correctly
// serializes access to the logger array between the two threads.
TEST_F(TestLogV2, ConcurrentApplyAndLogging) {
  const std::string path = "/tmp/test_concurrent_apply.log";
  std::filesystem::remove(path);
  log_v2::load("ut_common");

  std::atomic<bool> stop{false};
  std::atomic<int> reload_count{0};

  std::thread reloader([&] {
    bool use_file = true;
    while (!stop) {
      config cfg(path,
                 use_file ? config::logger_type::LOGGER_FILE
                          : config::logger_type::LOGGER_STDOUT,
                 0, false, false);
      cfg.set_level("core", "info");
      log_v2::instance().apply(cfg);
      use_file = !use_file;
      ++reload_count;
    }
  });

  std::thread logger_thread([&] {
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start <
           std::chrono::milliseconds(200)) {
      auto core_logger = log_v2::instance().get(log_v2::CORE);
      if (core_logger)
        core_logger->info("concurrent log line");
    }
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  stop = true;
  reloader.join();
  logger_thread.join();

  ASSERT_GT(reload_count.load(), 0);
  std::filesystem::remove(path);
}
