/**
 * Copyright 2025 Centreon (https://www.centreon.com/)
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
#include "com/centreon/broker/file/directory_watcher.hh"
#include <gtest/gtest.h>
#include <filesystem>

using namespace com::centreon::broker;

/** Scenario: Watch a directory.
 * When a directory is watched.
 * Then the directory is watched.
 * When the directory is unwatched.
 * Then the directory is not watched.
 */
TEST(DirectoryWatcher, WatchFile) {
  std::filesystem::path dirname("/tmp/to_watch");
  std::filesystem::remove_all(dirname);
  std::filesystem::create_directory(dirname);
  file::directory_watcher watcher(
      "/tmp/to_watch", IN_CREATE | IN_MODIFY | IN_DELETE | IN_DELETE_SELF,
      false);

  // Create file
  std::filesystem::path filename = dirname / "file";
  auto file = std::fstream(filename, std::ios::out);
  file.close();

  auto it = watcher.watch();
  ASSERT_NE(it, watcher.end());
  auto& p = *it;
  ASSERT_EQ(p.first, IN_CREATE);
  ASSERT_EQ(filename.string(), (dirname / p.second).string());
  ++it;
  ASSERT_EQ(it, watcher.end());

  file.open(filename, std::ios::out);
  file << "Centreon is wonderful!";
  file.close();

  it = watcher.watch();
  ASSERT_NE(it, watcher.end());
  p = *it;
  ASSERT_EQ(p.first, IN_MODIFY);
  ASSERT_EQ(filename.string(), (dirname / p.second).string());

  std::filesystem::remove(filename);
  it = watcher.watch();
  p = *it;
  ASSERT_NE(it, watcher.end());
  ASSERT_EQ(p.first, IN_DELETE);
  ASSERT_EQ(filename.string(), (dirname / p.second).string());
}

TEST(DirectoryWatcher, WatchNonBlockingFromAnotherThread) {
  std::filesystem::path dirname("/tmp/to_watch");
  std::filesystem::remove_all(dirname);
  std::filesystem::create_directory(dirname);
  std::filesystem::path filename = dirname / "file";
  file::directory_watcher watcher("/tmp/to_watch", IN_CREATE, true);

  std::unique_ptr<std::thread> thread;
  uint32_t thread_count = 0;
  absl::Mutex thread_count_m;
  std::atomic_uint count = 0;
  thread = std::make_unique<std::thread>([&thread_count_m, &thread_count,
                                          &count, &watcher]() {
    uint32_t thread_id;
    {
      thread_count_m.Lock();
      thread_count++;
      thread_id = thread_count;
      thread_count_m.Unlock();
    }
    for (int i = 0; i < 1000; i++) {
      auto it = watcher.watch();
      for (auto end = watcher.end(); it != end; ++it) {
        std::cout << "File '" << (*it).second
                  << "' creation detected by thread " << thread_id << std::endl;
        count.fetch_add(1);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  });

  auto thread_started = [&thread_count]() { return thread_count == 1; };
  thread_count_m.Lock();
  thread_count_m.Await(absl::Condition(&thread_started));
  thread_count_m.Unlock();
  auto file = std::fstream(filename, std::ios::out);
  std::cout << "file created" << std::endl;
  file.open(filename, std::ios::out);
  file << "Centreon is wonderful!";
  file.close();

  thread->join();

  ASSERT_EQ(count.load(), 1);
}

TEST(DirectoryWatcher, WatchNonBlockingMultipleFiles) {
  std::filesystem::path dirname("/tmp/to_watch");
  std::filesystem::remove_all(dirname);
  std::filesystem::create_directory(dirname);
  file::directory_watcher watcher(
      "/tmp/to_watch", IN_CREATE | IN_MODIFY | IN_DELETE | IN_DELETE_SELF,
      true);

  std::vector<std::string> filenames;
  absl::Mutex filenames_m;
  bool started = false;

  /* Start the watcher in a separate thread. */
  std::thread thread([&started, &filenames_m, &watcher, &filenames]() {
    filenames_m.Lock();
    started = true;
    filenames_m.Unlock();

    auto it = watcher.watch();
    while (it == watcher.end()) {
      sleep(1);
      it = watcher.watch();
    }
    while (it != watcher.end()) {
      while (it != watcher.end()) {
        filenames.emplace_back((*it).second);
        ++it;
      }
      it = watcher.watch();
    }
  });

  /* Wait for the watcher to start. */
  filenames_m.Lock();
  auto wait_watcher = [&started] { return started; };
  filenames_m.Await(absl::Condition(&wait_watcher));
  filenames_m.Unlock();

  /* Create 200 files. */
  for (int count = 0; count < 200; count++) {
    std::filesystem::path filename = dirname / fmt::format("file{}", count);
    std::fstream file(filename, std::ios::out);
    file.close();
  }
  thread.join();

  /* We should have 200 files. file0, file1, file2, file3, etc. */
  std::sort(filenames.begin(), filenames.end());
  std::vector<std::string> expected_filenames;
  for (int count = 0; count < 200; count++) {
    expected_filenames.push_back(fmt::format("file{}", count));
  }
  std::sort(expected_filenames.begin(), expected_filenames.end());
  ASSERT_EQ(filenames.size(), 200);
  ASSERT_EQ(filenames, expected_filenames);
}
