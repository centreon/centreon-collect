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

#include <gtest/gtest.h>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <thread>

#include <gtest/gtest.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "com/centreon/common/file_watcher.hh"

using namespace com::centreon::common;

extern std::shared_ptr<asio::io_context> g_io_context;
static std::shared_ptr<spdlog::logger> logger =
    spdlog::stdout_color_mt("file_watcher");

// on_change is fired this long after the last event of a burst (must stay in
// sync with the debounce_delay used by file_watcher)
static const std::chrono::milliseconds debounce_delay(200);

class file_watcher_test : public ::testing::Test {
 protected:
  std::filesystem::path _watched_path;
  std::shared_ptr<file_watcher> _watcher;
  std::atomic<unsigned> _change_count{0};

  static void SetUpTestSuite() {
    logger->set_level(spdlog::level::trace);
    logger->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%s:%#] [%n] [%l] [%P] %v");
  }

  void SetUp() override {
    _watched_path =
        std::filesystem::temp_directory_path() /
        fmt::format(
            "file_watcher_test_{}.ini",
            ::testing::UnitTest::GetInstance()->current_test_info()->name());
    std::filesystem::remove(_watched_path);
  }

  void TearDown() override {
    if (_watcher) {
      stop_watcher();
      _watcher.reset();
    }
    std::filesystem::remove(_watched_path);
  }

  // asio descriptors and timers are not thread safe, so stop must be done in
  // the io_context thread as in production code
  void stop_watcher() {
    asio::post(*g_io_context, [watcher = _watcher]() { watcher->stop(); });
    // let time for the stop and a possibly pending debounce to complete
    std::this_thread::sleep_for(2 * debounce_delay);
  }

  void start_watcher() {
    _watcher = file_watcher::load(g_io_context, logger, _watched_path.string(),
                                  [this]() { ++_change_count; });
    // give the watch time to be established on the io_context thread before
    // the test starts changing the file
    std::this_thread::sleep_for(debounce_delay);
  }

  void write_file(const std::string& content) {
    std::ofstream f(_watched_path);
    f << content;
  }

  bool wait_change_count(unsigned expected,
                         const std::chrono::milliseconds& timeout =
                             std::chrono::milliseconds(3000)) {
    auto limit = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < limit) {
      if (_change_count >= expected) {
        return true;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return _change_count >= expected;
  }
};

TEST_F(file_watcher_test, detect_modification) {
  write_file("check1=/usr/bin/echo one");
  start_watcher();

  write_file("check1=/usr/bin/echo two");
  ASSERT_TRUE(wait_change_count(1));
}

TEST_F(file_watcher_test, no_change_no_notification) {
  write_file("check1=/usr/bin/echo one");
  start_watcher();

  // no file change: no notification
  std::this_thread::sleep_for(10 * debounce_delay);
  ASSERT_EQ(_change_count, 0);
}

TEST_F(file_watcher_test, several_modifications_several_notifications) {
  write_file("check1=/usr/bin/echo one");
  start_watcher();

  write_file("check1=/usr/bin/echo two");
  ASSERT_TRUE(wait_change_count(1));

  // wait for the first burst to be fully coalesced before the next change so it
  // is reported separately
  std::this_thread::sleep_for(2 * debounce_delay);
  write_file("check1=/usr/bin/echo three");
  ASSERT_TRUE(wait_change_count(2));
}

TEST_F(file_watcher_test, no_notification_after_stop) {
  write_file("check1=/usr/bin/echo one");
  start_watcher();

  stop_watcher();

  write_file("check1=/usr/bin/echo two");
  std::this_thread::sleep_for(10 * debounce_delay);
  ASSERT_EQ(_change_count, 0);
}
