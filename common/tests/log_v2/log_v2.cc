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
#include <fstream>

using log_v2 = com::centreon::common::log_v2::log_v2;
using config = com::centreon::common::log_v2::config;

class TestLogV2 : public ::testing::Test {
 public:
  //  void SetUp() override {}
  void TearDown() override { log_v2::unload(true); }
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
  std::remove("/tmp/test.log");
}

TEST_F(TestLogV2, Flush) {
  /* We remove the file if it exists */
  struct stat buffer;
  if (stat("/tmp/test.log", &buffer) == 0)
    std::remove("/tmp/test.log");

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

  std::remove("/tmp/test.log");

  /* The flush is disabled */
  cfg.set_flush_interval(0);
  log_v2::instance().apply(cfg);

  logger->debug("log 1");
  logger->debug("log 2");
  logger->debug("log 3");
  std::this_thread::sleep_for(std::chrono::seconds(1));
  content = read_file("/tmp/test.log");
  ASSERT_TRUE(content.find("log 1") != std::string::npos)
      << "Log flush is disabled, we should not have all the logs, here 'log 1'."
      << std::endl;
  ASSERT_TRUE(content.find("log 2") != std::string::npos)
      << "Log flush is disabled, we should not have all the logs, here 'log 2'."
      << std::endl;
  ASSERT_TRUE(content.find("log 3") != std::string::npos)
      << "Log flush is disabled, we should not have all the logs, here 'log 3'."
      << std::endl;

  std::remove("/tmp/test.log");
}
