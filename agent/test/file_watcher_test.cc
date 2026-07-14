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

#include "file_watcher.hh"

using namespace com::centreon::agent;

extern std::shared_ptr<asio::io_context> g_io_context;

static const std::chrono::milliseconds test_poll_interval(200);

class file_watcher_test : public ::testing::Test {
 protected:
  std::filesystem::path _watched_path;
  std::shared_ptr<file_watcher> _watcher;
  std::atomic<unsigned> _change_count{0};

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

  // timers are not thread safe, so stop must be done in the io_context
  // thread as in production code
  void stop_watcher() {
    asio::post(*g_io_context, [watcher = _watcher]() { watcher->stop(); });
    // let time to the stop and to a possibly pending poll to complete
    std::this_thread::sleep_for(2 * test_poll_interval);
  }

  void start_watcher() {
    _watcher = file_watcher::load(g_io_context, spdlog::default_logger(),
                                  _watched_path.string(), test_poll_interval,
                                  [this]() { ++_change_count; });
  }

  void write_file(const std::string& content) {
    std::ofstream f(_watched_path);
    f << content;
  }

  // writing twice in a row may not change the last write time on file
  // systems with a coarse timestamp granularity, so we force it
  void touch_forward() {
    std::filesystem::last_write_time(
        _watched_path, std::filesystem::last_write_time(_watched_path) +
                           std::chrono::seconds(2));
  }

  bool wait_change_count(unsigned expected,
                         const std::chrono::milliseconds& timeout =
                             std::chrono::milliseconds(2000)) {
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

TEST_F(file_watcher_test, get_last_write_time) {
  ASSERT_FALSE(file_watcher::get_last_write_time(_watched_path));
  write_file("hello");
  ASSERT_TRUE(file_watcher::get_last_write_time(_watched_path));
}

TEST_F(file_watcher_test, detect_modification) {
  write_file("check1=/usr/bin/echo one");
  start_watcher();

  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  write_file("check1=/usr/bin/echo two");
  ASSERT_TRUE(wait_change_count(1));
}

TEST_F(file_watcher_test, detect_creation) {
  start_watcher();

  write_file("check1=/usr/bin/echo one");
  ASSERT_TRUE(wait_change_count(1));
}

TEST_F(file_watcher_test, detect_deletion) {
  write_file("check1=/usr/bin/echo one");
  start_watcher();

  std::filesystem::remove(_watched_path);
  ASSERT_TRUE(wait_change_count(1));
}

TEST_F(file_watcher_test, no_change_no_notification) {
  write_file("check1=/usr/bin/echo one");
  start_watcher();

  // several poll periods without any file change
  std::this_thread::sleep_for(10 * test_poll_interval);
  ASSERT_EQ(_change_count, 0);
}

TEST_F(file_watcher_test, several_modifications_several_notifications) {
  write_file("check1=/usr/bin/echo one");
  start_watcher();

  // sleep before each write so that it gets a distinct last write time
  // whatever the file system time granularity
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  write_file("check1=/usr/bin/echo two");
  ASSERT_TRUE(wait_change_count(1));

  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  write_file("check1=/usr/bin/echo three");
  ASSERT_TRUE(wait_change_count(2));
}

TEST_F(file_watcher_test, no_notification_after_stop) {
  write_file("check1=/usr/bin/echo one");
  start_watcher();

  stop_watcher();

  write_file("check1=/usr/bin/echo two");
  std::this_thread::sleep_for(10 * test_poll_interval);
  ASSERT_EQ(_change_count, 0);
}
