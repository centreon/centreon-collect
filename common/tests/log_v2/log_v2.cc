/*
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
#include <fstream>
#include <regex>

using log_v2 = com::centreon::common::log_v2::log_v2;
using config = com::centreon::common::log_v2::config;

class TestLogV3 : public ::testing::Test {
 public:
  void SetUp() override {}
  void TearDown() override { log_v2::unload(); }
};

static std::string read_file(const std::string& name) {
  std::ifstream is(name);
  std::stringstream buffer;
  buffer << is.rdbuf();
  is.close();
  return buffer.str();
}

TEST_F(TestLogV3, load) {
  log_v2::load({"core", "config"});
  ASSERT_EQ(log_v2::instance().get(0)->name(), std::string_view("core"));
  ASSERT_EQ(log_v2::instance().get(1)->name(), std::string_view("config"));
}

TEST_F(TestLogV3, LoggerUpdated) {
  log_v2::load({"core"});
  ASSERT_EQ(log_v2::instance().get(0)->level(), spdlog::level::info);
  auto core_logger = log_v2::instance().get(0);
  testing::internal::CaptureStdout();
  core_logger->info("First log");
  config cfg("/tmp", "test.log", 10000, 0, false, false);
  cfg.set_level("core", "debug");
  log_v2::instance().apply(cfg);
  ASSERT_EQ(log_v2::instance().get(0)->level(), spdlog::level::debug);
  core_logger->info("Second log with the old core logger version");
  log_v2::instance().get(0)->info("Log with the new version");
  std::string output = testing::internal::GetCapturedStdout();
  std::cout << "Captured stdout:\n" << output << std::endl;
  /* To match the output, we use regex because of the colored output. */
  std::regex re(
      "\\[core\\] \\[.*info.*\\] First log\n.*\\[core\\] \\[.*info.*\\] Second "
      "log with the old core logger version");
  std::smatch match;
  ASSERT_TRUE(std::regex_search(output, match, re) && match.size() > 0);

  std::string content = read_file("/tmp/test.log");
  ASSERT_TRUE(content.find("Log with the new version") != std::string::npos)
      << "The file logged contains '" << content
      << "' and this string does not contain 'Log with the new version'"
      << std::endl;
  ;

  std::remove("/tmp/test.log");
}

TEST_F(TestLogV3, LoggerUpdatedMt) {
  log_v2::load({"core"});
  std::vector<std::thread> v;
  testing::internal::CaptureStdout();
  std::mutex m;
  std::condition_variable cv;
  bool change_done = false;
  for (int i = 0; i < 5; i++) {
    std::thread t([i, &m, &cv, &change_done] {
      auto core_logger = log_v2::instance().get(0);
      for (int j = 0; j < 10; j++) {
        core_logger->info("Log {} from thread {}", j, i);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
      std::unique_lock<std::mutex> lck(m);
      cv.wait(lck, [&change_done] { return change_done; });
      core_logger = log_v2::instance().get(0);
      for (int j = 0; j < 10; j++)
        core_logger->info("New log {} from thread {}", j, i);
    });
    v.push_back(std::move(t));
  }

  config cfg("/tmp", "test.log", 10000, 0, false, false);
  cfg.set_level("core", "debug");
  log_v2::instance().apply(cfg);

  {
    std::lock_guard<std::mutex> lck(m);
    change_done = true;
    cv.notify_all();
  }

  for (auto& t : v)
    t.join();

  std::string output = testing::internal::GetCapturedStdout();
  std::cout << "Captured stdout:\n" << output << std::endl;
  std::string content = read_file("/tmp/test.log");
  auto spl = absl::StrSplit(content, '\n');
  uint32_t count = 0;
  for (auto& sv : spl) {
    count++;
  }
  ASSERT_GE(count, 50);

  spl = absl::StrSplit(output, '\n');
  count = 0;
  for (auto& sv : spl) {
    if (sv.empty())
      continue;
    ASSERT_TRUE(sv.find("] Log ") != std::string::npos)
        << "All the lines from stdout should contain '] Log ', here we have '"
        << sv << "'" << std::endl;
    count++;
  }
  ASSERT_LE(count, 50);

  std::remove("/tmp/test.log");
}

TEST_F(TestLogV3, Flush) {
  log_v2::load({"core"});
  config cfg("/tmp", "test.log", 10000, 3, false, false);
  cfg.set_level("core", "debug");
  log_v2::instance().apply(cfg);
  auto logger = log_v2::instance().get(0);

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
  logger = log_v2::instance().get(0);

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
