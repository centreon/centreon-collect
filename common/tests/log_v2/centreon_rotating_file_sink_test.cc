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

#include <charconv>

#include "common/log_v2/centreon_rotating_file_sink-inl.hh"

#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <regex>
#include <thread>
#include <vector>

using namespace spdlog::sinks;
namespace fs = std::filesystem;

namespace {

const char* kTestDir = "/tmp/centreon_rotating_file_sink_test";

std::string read_file(const std::string& path) {
  std::ifstream is(path);
  if (!is)
    return {};
  std::stringstream buffer;
  buffer << is.rdbuf();
  return buffer.str();
}

}  // namespace

class CentreonRotatingFileSinkTest : public ::testing::Test {
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

TEST(CentreonRotatingFileSinkCalcFilename, IndexZeroReturnsFilenameUnchanged) {
  ASSERT_EQ(centreon_rotating_file_sink_mt::calc_filename("base.log", 0),
            "base.log");
}

TEST(CentreonRotatingFileSinkCalcFilename, IndexInsertedBeforeExtension) {
  ASSERT_EQ(centreon_rotating_file_sink_mt::calc_filename("base.log", 3),
            "base.3.log");
  ASSERT_EQ(
      centreon_rotating_file_sink_mt::calc_filename("/tmp/dir/base.log", 1),
      "/tmp/dir/base.1.log");
}

TEST(CentreonRotatingFileSinkCalcFilename, NoExtensionAppendsIndexAtEnd) {
  ASSERT_EQ(centreon_rotating_file_sink_mt::calc_filename("base", 2),
            "base.2");
}

TEST_F(CentreonRotatingFileSinkTest, ConstructionOpensBaseFile) {
  std::string base = path("base.log");
  auto sink =
      std::make_shared<centreon_rotating_file_sink_mt>(base, 1024 * 1024, 3);
  ASSERT_EQ(sink->filename(), base);
  ASSERT_TRUE(fs::exists(base));
}

TEST_F(CentreonRotatingFileSinkTest, ZeroMaxSizeThrows) {
  std::string base = path("zero.log");
  ASSERT_THROW(
      std::make_shared<centreon_rotating_file_sink_mt>(base, 0, 3),
      spdlog::spdlog_ex);
}

TEST_F(CentreonRotatingFileSinkTest, TooManyMaxFilesThrows) {
  std::string base = path("toomany.log");
  ASSERT_THROW(std::make_shared<centreon_rotating_file_sink_mt>(base, 1024,
                                                                 200001),
               spdlog::spdlog_ex);
}

TEST_F(CentreonRotatingFileSinkTest, RotateOnOpenMovesExistingContent) {
  std::string base = path("ro.log");
  {
    std::ofstream os(base);
    os << "old content\n";
  }
  auto sink = std::make_shared<centreon_rotating_file_sink_mt>(base, 1024, 3,
                                                                true);
  ASSERT_TRUE(fs::exists(path("ro.1.log")));
  ASSERT_EQ(read_file(path("ro.1.log")), "old content\n");
  ASSERT_EQ(read_file(base), "");
}

TEST_F(CentreonRotatingFileSinkTest, NoRotateOnOpenWhenDisabled) {
  std::string base = path("noro.log");
  {
    std::ofstream os(base);
    os << "old content\n";
  }
  auto sink = std::make_shared<centreon_rotating_file_sink_mt>(base, 1024, 3,
                                                                false);
  ASSERT_FALSE(fs::exists(path("noro.1.log")));
  ASSERT_EQ(read_file(base), "old content\n");
}

// Given a rotating sink with max_size 25 bytes.
// When writing three 11-byte lines ("0123456789\n").
// Then the third write overflows the limit (22 + 11 > 25), which rotates the
// first two lines into the ".1" file and starts the base file over.
TEST_F(CentreonRotatingFileSinkTest, RotatesWhenSizeExceedsMaxSize) {
  std::string base = path("size.log");
  auto sink = std::make_shared<centreon_rotating_file_sink_mt>(base, 25, 3);
  spdlog::logger logger("test", sink);
  logger.set_pattern("%v");

  for (int i = 0; i < 3; ++i) {
    logger.info("0123456789");
    sink->flush();
  }

  ASSERT_EQ(read_file(base), "0123456789\n");
  ASSERT_EQ(read_file(path("size.1.log")), "0123456789\n0123456789\n");
}

TEST_F(CentreonRotatingFileSinkTest, RotateNowRotatesImmediately) {
  std::string base = path("now.log");
  auto sink =
      std::make_shared<centreon_rotating_file_sink_mt>(base, 1024 * 1024, 3);
  spdlog::logger logger("test", sink);
  logger.set_pattern("%v");
  logger.info("first");
  sink->flush();

  sink->rotate_now();

  logger.info("second");
  sink->flush();
  ASSERT_EQ(read_file(path("now.1.log")), "first\n");
  ASSERT_EQ(read_file(base), "second\n");
}

// Given a rotating sink capped at max_files=2.
// When rotating more times than max_files.
// Then only the two most recent generations survive; older ones are
// discarded (log.3.log is never created).
TEST_F(CentreonRotatingFileSinkTest, RotationRespectsMaxFiles) {
  std::string base = path("cap.log");
  auto sink = std::make_shared<centreon_rotating_file_sink_mt>(base,
                                                                1024 * 1024,
                                                                2);
  spdlog::logger logger("test", sink);
  logger.set_pattern("%v");

  for (int i = 0; i < 5; ++i) {
    logger.info("content {}", i);
    sink->flush();
    sink->rotate_now();
  }

  ASSERT_FALSE(fs::exists(path("cap.3.log")));
  ASSERT_EQ(read_file(path("cap.1.log")), "content 4\n");
  ASSERT_EQ(read_file(path("cap.2.log")), "content 3\n");
}

TEST_F(CentreonRotatingFileSinkTest, SetFilenameSameBaseIsNoop) {
  std::string base = path("same.log");
  auto sink =
      std::make_shared<centreon_rotating_file_sink_mt>(base, 1024 * 1024, 3);
  ASSERT_FALSE(sink->set_filename(base));
  ASSERT_EQ(sink->filename(), base);
}

TEST_F(CentreonRotatingFileSinkTest, SetFilenameChangesTarget) {
  std::string base1 = path("target1.log");
  std::string base2 = path("target2.log");
  auto sink =
      std::make_shared<centreon_rotating_file_sink_mt>(base1, 1024 * 1024, 3);
  spdlog::logger logger("test", sink);
  logger.set_pattern("%v");
  logger.info("to base1");
  sink->flush();

  ASSERT_TRUE(sink->set_filename(base2));
  ASSERT_EQ(sink->filename(), base2);

  logger.info("to base2");
  sink->flush();
  ASSERT_EQ(read_file(base1), "to base1\n");
  ASSERT_EQ(read_file(base2), "to base2\n");
}

// Given a rotating sink that already rotated once (base is back at
// generation 0 after rotate_now(), the rotated content having moved to
// ".1.log" on disk).
// When set_filename() targets a new base.
// Then it opens generation 0 of the new base (not ".1"): the sink only ever
// tracks the currently-open path, which rotate_() always restores to
// generation 0, so there is no non-zero generation left to carry over.
TEST_F(CentreonRotatingFileSinkTest, SetFilenameAfterRotationTargetsGenerationZero) {
  std::string base1 = path("afterrot1.log");
  std::string base2 = path("afterrot2.log");
  auto sink =
      std::make_shared<centreon_rotating_file_sink_mt>(base1, 1024 * 1024, 3);
  spdlog::logger logger("test", sink);
  logger.set_pattern("%v");
  logger.info("first");
  sink->flush();
  sink->rotate_now();
  ASSERT_EQ(sink->filename(), base1);

  ASSERT_TRUE(sink->set_filename(base2));
  ASSERT_EQ(sink->filename(), base2);
  ASSERT_FALSE(fs::exists(path("afterrot2.1.log")));

  logger.info("second");
  sink->flush();
  ASSERT_EQ(read_file(base2), "second\n");
}

TEST_F(CentreonRotatingFileSinkTest, SetFilenameDoesNotTruncateExistingTarget) {
  std::string base1 = path("existing1.log");
  std::string base2 = path("existing2.log");
  {
    std::ofstream os(base2);
    os << "existing\n";
  }
  auto sink =
      std::make_shared<centreon_rotating_file_sink_mt>(base1, 1024 * 1024, 3);
  ASSERT_TRUE(sink->set_filename(base2));

  spdlog::logger logger("test", sink);
  logger.set_pattern("%v");
  logger.info("appended");
  sink->flush();
  ASSERT_EQ(read_file(base2), "existing\nappended\n");
}

// Given a centreon_rotating_file_sink_mt shared by several threads.
// When threads concurrently log while other threads concurrently call
// set_filename() to swap between two bases.
// Then no crash/deadlock occurs and the sink settles on one of the two known
// bases.
TEST_F(CentreonRotatingFileSinkTest, ConcurrentSetFilenameAndLogging) {
  std::string base1 = path("concurrent1.log");
  std::string base2 = path("concurrent2.log");
  auto sink = std::make_shared<centreon_rotating_file_sink_mt>(base1,
                                                                1024 * 1024,
                                                                3);
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
    threads.emplace_back([&sink, &base1, &base2]() {
      for (int i = 0; i < kSwitchesPerThread; ++i)
        sink->set_filename((i % 2) == 0 ? base1 : base2);
    });
  }
  for (auto& th : threads)
    th.join();

  sink->flush();

  std::string final_name = sink->filename();
  ASSERT_TRUE(final_name == base1 || final_name == base2);
}

// Given a centreon_rotating_file_sink_mt.
// When many threads only hammer set_filename() concurrently (no logging).
// Then it never crashes/deadlocks and both target bases end up created.
TEST_F(CentreonRotatingFileSinkTest, ConcurrentSetFilenameOnly) {
  std::string base1 = path("switch1.log");
  std::string base2 = path("switch2.log");
  auto sink = std::make_shared<centreon_rotating_file_sink_mt>(base1,
                                                                1024 * 1024,
                                                                3);

  constexpr int kThreads = 16;
  constexpr int kIterations = 1000;
  std::vector<std::thread> threads;
  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([&sink, &base1, &base2, t]() {
      for (int i = 0; i < kIterations; ++i)
        sink->set_filename(((t + i) % 2) == 0 ? base1 : base2);
    });
  }
  for (auto& th : threads)
    th.join();

  std::string final_name = sink->filename();
  ASSERT_TRUE(final_name == base1 || final_name == base2);
  ASSERT_TRUE(fs::exists(base1));
  ASSERT_TRUE(fs::exists(base2));
}
