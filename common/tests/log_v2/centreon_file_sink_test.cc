/**
 * Copyright 2026 Centreon
 * Licensed under the Apache License, Version 2.0(the "License");
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

#include "common/log_v2/centreon_file_sink.hh"

#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <regex>
#include <thread>
#include <vector>

using namespace spdlog::sinks;
namespace fs = std::filesystem;

namespace {

const char* kTestDir = "/tmp/centreon_file_sink_test";

std::string read_file(const std::string& path) {
  std::ifstream is(path);
  if (!is)
    return {};
  std::stringstream buffer;
  buffer << is.rdbuf();
  return buffer.str();
}

/* Every non-empty line must fully match "thread <t> msg <i>", which is only
 * possible if writes are never interleaved/torn by a concurrent
 * set_filename(). */
size_t count_and_check_well_formed_lines(const std::string& content) {
  static const std::regex line_re(R"(^thread \d+ msg \d+$)");
  std::istringstream is(content);
  std::string line;
  size_t count = 0;
  while (std::getline(is, line)) {
    if (line.empty())
      continue;
    EXPECT_TRUE(std::regex_match(line, line_re))
        << "Corrupted/torn log line: '" << line << "'";
    ++count;
  }
  return count;
}

}  // namespace

class CentreonFileSinkTest : public ::testing::Test {
 public:
  void SetUp() override {
    fs::remove_all(kTestDir);
    fs::create_directories(kTestDir);
  }
  void TearDown() override { fs::remove_all(kTestDir); }

 protected:
  static std::string path(const std::string& name) {
    return std::string(kTestDir) + "/" + name;
  }
};

TEST_F(CentreonFileSinkTest, ConstructionOpensFile) {
  std::string f = path("a.log");
  auto sink = std::make_shared<centreon_file_sink_mt>(f);
  ASSERT_EQ(sink->filename(), f);
  ASSERT_TRUE(fs::exists(f));
}

TEST_F(CentreonFileSinkTest, WritesGoToFile) {
  std::string f = path("b.log");
  auto sink = std::make_shared<centreon_file_sink_mt>(f);
  spdlog::logger logger("test", sink);
  logger.set_pattern("%v");
  logger.info("hello world");
  sink->flush();
  ASSERT_EQ(read_file(f), "hello world\n");
}

TEST_F(CentreonFileSinkTest, TruncateOnConstruction) {
  std::string f = path("c.log");
  {
    std::ofstream os(f);
    os << "pre-existing content\n";
  }
  auto sink = std::make_shared<centreon_file_sink_mt>(f, true);
  spdlog::logger logger("test", sink);
  logger.set_pattern("%v");
  logger.info("new content");
  sink->flush();
  std::string content = read_file(f);
  ASSERT_EQ(content, "new content\n");
}

TEST_F(CentreonFileSinkTest, NoTruncateOnConstructionKeepsContent) {
  std::string f = path("d.log");
  {
    std::ofstream os(f);
    os << "pre-existing content\n";
  }
  auto sink = std::make_shared<centreon_file_sink_mt>(f, false);
  ASSERT_EQ(read_file(f), "pre-existing content\n");
}

TEST_F(CentreonFileSinkTest, SetFilenameSameNameIsNoop) {
  std::string f = path("e.log");
  auto sink = std::make_shared<centreon_file_sink_mt>(f);
  spdlog::logger logger("test", sink);
  logger.set_pattern("%v");
  logger.info("line1");
  sink->flush();

  ASSERT_FALSE(sink->set_filename(f));

  logger.info("line2");
  sink->flush();
  ASSERT_EQ(read_file(f), "line1\nline2\n");
}

TEST_F(CentreonFileSinkTest, SetFilenameChangesTarget) {
  std::string f1 = path("f1.log");
  std::string f2 = path("f2.log");
  auto sink = std::make_shared<centreon_file_sink_mt>(f1);
  spdlog::logger logger("test", sink);
  logger.set_pattern("%v");
  logger.info("to f1");
  sink->flush();

  ASSERT_TRUE(sink->set_filename(f2));
  ASSERT_EQ(sink->filename(), f2);

  logger.info("to f2");
  sink->flush();
  ASSERT_EQ(read_file(f1), "to f1\n");
  ASSERT_EQ(read_file(f2), "to f2\n");
}

TEST_F(CentreonFileSinkTest, SetFilenameDoesNotTruncateExistingTarget) {
  std::string f1 = path("g1.log");
  std::string f2 = path("g2.log");
  {
    std::ofstream os(f2);
    os << "existing\n";
  }
  auto sink = std::make_shared<centreon_file_sink_mt>(f1);
  ASSERT_TRUE(sink->set_filename(f2));

  spdlog::logger logger("test", sink);
  logger.set_pattern("%v");
  logger.info("appended");
  sink->flush();
  ASSERT_EQ(read_file(f2), "existing\nappended\n");
}

TEST_F(CentreonFileSinkTest, ReopenRecreatesRemovedFile) {
  std::string f = path("h.log");
  auto sink = std::make_shared<centreon_file_sink_mt>(f);
  spdlog::logger logger("test", sink);
  logger.set_pattern("%v");
  logger.info("before");
  sink->flush();

  fs::remove(f);
  ASSERT_FALSE(fs::exists(f));

  sink->reopen();
  logger.info("after");
  sink->flush();
  ASSERT_TRUE(fs::exists(f));
  ASSERT_EQ(read_file(f), "after\n");
}

// Given a centreon_file_sink_mt shared by several threads.
// When threads concurrently log messages while other threads concurrently
// call set_filename() to swap between two targets.
// Then no crash/deadlock occurs, the sink settles on one of the two known
// targets, and every line ever written is complete (never interleaved or
// torn by a concurrent reopen).
TEST_F(CentreonFileSinkTest, ConcurrentSetFilenameAndLogging) {
  std::string f1 = path("concurrent1.log");
  std::string f2 = path("concurrent2.log");
  auto sink = std::make_shared<centreon_file_sink_mt>(f1);
  spdlog::logger logger("test", sink);
  logger.set_pattern("%v");

  constexpr int kWriterThreads = 8;
  constexpr int kMessagesPerThread = 500;
  constexpr int kSwitcherThreads = 4;
  constexpr int kSwitchesPerThread = 200;

  std::vector<std::thread> threads;
  for (int t = 0; t < kWriterThreads; ++t) {
    threads.emplace_back([&logger, t]() {
      for (int i = 0; i < kMessagesPerThread; ++i)
        logger.info("thread {} msg {}", t, i);
    });
  }
  for (int t = 0; t < kSwitcherThreads; ++t) {
    threads.emplace_back([&sink, &f1, &f2]() {
      for (int i = 0; i < kSwitchesPerThread; ++i)
        sink->set_filename((i % 2) == 0 ? f1 : f2);
    });
  }
  for (auto& th : threads)
    th.join();

  sink->flush();

  std::string final_name = sink->filename();
  ASSERT_TRUE(final_name == f1 || final_name == f2);

  size_t total_lines = 0;
  total_lines += count_and_check_well_formed_lines(read_file(f1));
  total_lines += count_and_check_well_formed_lines(read_file(f2));
  ASSERT_LE(total_lines,
            static_cast<size_t>(kWriterThreads) * kMessagesPerThread);
}

// Given a centreon_file_sink_mt.
// When many threads only hammer set_filename() concurrently (no logging).
// Then it never crashes/deadlocks and both target files end up created.
TEST_F(CentreonFileSinkTest, ConcurrentSetFilenameOnly) {
  std::string f1 = path("switch1.log");
  std::string f2 = path("switch2.log");
  auto sink = std::make_shared<centreon_file_sink_mt>(f1);

  constexpr int kThreads = 16;
  constexpr int kIterations = 1000;
  std::vector<std::thread> threads;
  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([&sink, &f1, &f2, t]() {
      for (int i = 0; i < kIterations; ++i)
        sink->set_filename(((t + i) % 2) == 0 ? f1 : f2);
    });
  }
  for (auto& th : threads)
    th.join();

  std::string final_name = sink->filename();
  ASSERT_TRUE(final_name == f1 || final_name == f2);
  ASSERT_TRUE(fs::exists(f1));
  ASSERT_TRUE(fs::exists(f2));
}
