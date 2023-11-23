/*
 * Copyright 2011 - 2023 Centreon (https://www.centreon.com/)
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
#include <absl/strings/match.h>
#include <fmt/format.h>
#include <gtest/gtest.h>
#include "broker/core/misc/filesystem.hh"
#include "com/centreon/broker/file/cfile.hh"
#include "com/centreon/broker/file/disk_accessor.hh"
#include "com/centreon/broker/file/splitter.hh"

using namespace com::centreon::broker;

class FileSplitterSplitLimited : public ::testing::Test {
 public:
  void SetUp() override {
    file::disk_accessor::load(50000);
    std::string p;
    std::list<std::string> files = misc::filesystem::dir_content("/tmp", false);
    for (auto& f : files) {
      if (absl::StartsWith(f, "/tmp/queue")) {
        std::remove(f.c_str());
      }
    }
  }

  void TearDown() override { file::disk_accessor::unload(); }
};

// disk accessor is limited to 50000 bytes. the splitter is limited to files
// of 10000 bytes. Since they are prefixed with a header of 8 bytes, we can
// still add 9992 bytes without risk to each file. In each step, we add a
// such buffer that creates a new file of 10000 bytes.
// The splitter should accept exactly 5 files /tmp/queue, /tmp/queue1,
// /tmp/queue2, /tmp/queue3 of size 10000.
// There is a queue5 file created, but it is empty.
TEST_F(FileSplitterSplitLimited, MultipleFilesCreatedAligned) {
  // Then
  std::string path{"/tmp/queue"};
  auto file = std::make_unique<file::splitter>(path, 10000);
  char buffer[9992];
  for (int i = 0; i < 9992; i++)
    buffer[i] = 'a' + i % 24;
  file->write(buffer, sizeof(buffer));
  file->flush();

  ASSERT_EQ(misc::filesystem::file_size(path), 10000);

  for (int i = 1; i < 4; ++i) {
    std::cout << "i = " << i << std::endl;
    file->write(buffer, sizeof(buffer));
    file->flush();
    std::string p = fmt::format("{}{}", path, i);
    ASSERT_EQ(misc::filesystem::file_size(p), 10000);
  }
  file->write(buffer, sizeof(buffer));
  file->flush();
  ASSERT_EQ(misc::filesystem::file_size("/tmp/queue4"), 10000);
  file->write(buffer, sizeof(buffer));
  file->flush();
  ASSERT_EQ(misc::filesystem::file_size("/tmp/queue5"), 0);
}

// disk accessor is limited to 50000 bytes. the splitter is limited to files
// of 10008 bytes. Since they are prefixed with a header of 8 bytes, we can
// still add 10000 bytes without risk to each file. In each step, we add a
// such buffer that creates a new file of 10008 bytes.
// The splitter should accept exactly 4 files /tmp/queue, /tmp/queue1,
// /tmp/queue2, /tmp/queue3 of size 10008 and one last of size 8 (just the
// header, no place to write the 10000 next bytes).
TEST_F(FileSplitterSplitLimited, MultipleFilesCreated) {
  // Then
  std::string path{"/tmp/queue"};
  auto file = std::make_unique<file::splitter>(path, 10008);
  char buffer[10000];
  for (int i = 0; i < 10000; i++)
    buffer[i] = 'a' + i % 24;
  file->write(buffer, sizeof(buffer));
  file->flush();

  ASSERT_EQ(misc::filesystem::file_size(path), 10008);

  for (int i = 1; i < 4; ++i) {
    std::cout << "i = " << i << std::endl;
    file->write(buffer, sizeof(buffer));
    file->flush();
    std::string p = fmt::format("{}{}", path, i);
    ASSERT_EQ(misc::filesystem::file_size(p), 10008);
  }
  file->write(buffer, sizeof(buffer));
  file->flush();
  ASSERT_EQ(misc::filesystem::file_size("/tmp/queue4"), 8);
  file->write(buffer, sizeof(buffer));
  file->flush();
}

// disk accessor is limited to 50000 bytes. the splitter is limited to files
// of 10000 bytes. Since they are prefixed with a header of 8 bytes, we can
// still add 9992 bytes without risk to each file. In each step, we add a
// such buffer that creates a new file of 10000 bytes.
// 3 files are created with the splitter /tmp/queue, /tmp/queue1, /tmp/queue2
// of size 10000.
// Then an empty file /tmp/queue3 is added. And then we create a new file of
// size 10000 from the splitter. This one should be named /tmp/queue4 with a
// size of 10000.
TEST_F(FileSplitterSplitLimited, MultipleFilesCreatedAlignedWithEmptyFile) {
  // Then
  std::string path{"/tmp/queue"};
  auto file = std::make_unique<file::splitter>(path, 10000);
  char buffer[9992];
  for (int i = 0; i < 9992; i++)
    buffer[i] = 'a' + i % 24;
  file->write(buffer, sizeof(buffer));
  file->flush();

  ASSERT_EQ(misc::filesystem::file_size(path), 10000);

  for (int i = 1; i < 4; ++i) {
    std::cout << "i = " << i << std::endl;
    file->write(buffer, sizeof(buffer));
    file->flush();
    std::string p = fmt::format("{}{}", path, i);
    ASSERT_EQ(misc::filesystem::file_size(p), 10000);
  }

  // Here is the issue, a new empty file.
  FILE* ff = fopen("/tmp/queue4", "w");
  fclose(ff);

  file->write(buffer, sizeof(buffer));
  file->flush();
  ASSERT_EQ(misc::filesystem::file_size("/tmp/queue4"), 10000);
  file->write(buffer, sizeof(buffer));
  file->flush();
  ASSERT_EQ(misc::filesystem::file_size("/tmp/queue5"), 0);
}
