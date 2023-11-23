/*
 * Copyright 2011 - 2019 Centreon (https://www.centreon.com/)
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
#include <gtest/gtest.h>
#include "broker/core/misc/filesystem.hh"
#include "com/centreon/broker/file/cfile.hh"
#include "com/centreon/broker/file/disk_accessor.hh"
#include "com/centreon/broker/file/splitter.hh"

using namespace com::centreon::broker;

class FileSplitterMoreThanMaxSize : public ::testing::Test {
 public:
  void SetUp() override {
    file::disk_accessor::load(100000u);
    _path = "/tmp/queue";
    {
      std::list<std::string> parts{
          misc::filesystem::dir_content_with_filter("/tmp/", "queue*")};
      for (std::string const& f : parts)
        std::remove(f.c_str());
    }
    _file = std::make_unique<file::splitter>(_path, 10000, true);
  }

  void TearDown() override { file::disk_accessor::unload(); }

 protected:
  std::unique_ptr<file::splitter> _file;
  std::string _path;
};

// Given a splitter object configured with a max_size of 10000
// And write() was called with 1 byte of data
// When write() is called with 10001 bytes of data
// Then the first file has 1 byte of data
// And the second file is longer than max_size
TEST_F(FileSplitterMoreThanMaxSize, MoreThanMaxSizeToNextFile) {
  // Given
  char buffer[10001];
  memset(buffer, 0, sizeof(buffer));
  _file->write(buffer, 1);

  // When
  _file->write(buffer, sizeof(buffer));

  // We force the writing to be done
  _file->flush();

  // Then
  std::string first_file{_path};
  size_t size1 = misc::filesystem::file_size(first_file);
  ASSERT_EQ(size1, 9u);
  std::string second_file{_path};
  second_file.append("1");
  size_t size2 = misc::filesystem::file_size(second_file);
  ASSERT_EQ(size2, static_cast<int64_t>(8 + sizeof(buffer)));
}

// Given a splitter object configured with a max_size of 10000
// And write() was called with 1 byte of data
// And write() was called with 10001 bytes of data
// When read() is called
// Then 1 byte is read
TEST_F(FileSplitterMoreThanMaxSize, ReadBeforeMoreThanMaxSize) {
  // Given
  char buffer[10001];
  memset(buffer, 0, sizeof(buffer));
  _file->write(buffer, 1);
  _file->write(buffer, sizeof(buffer));

  // When
  long read_bytes(_file->read(buffer, sizeof(buffer)));

  // Then
  ASSERT_EQ(read_bytes, 1);
}

// Given a splitter object configured with a max_size of 10000
// And write() was called with 1 byte of data
// And write() was called with 10001 bytes of data
// And read() is called once
// When read() is called again with a 10010 bytes buffer
// Then 10001 bytes are read
TEST_F(FileSplitterMoreThanMaxSize, ReadMoreThanMaxSize) {
  // Given
  char buffer[10010];
  for (uint32_t i = 0; i < sizeof(buffer); ++i)
    buffer[i] = i % 128;
  _file->write(buffer, 1);
  _file->write(buffer, 10001);
  size_t read_bytes = _file->read(buffer, sizeof(buffer));
  ASSERT_EQ(read_bytes, 1);

  // When
  memset(buffer, 0, sizeof(buffer));
  read_bytes = _file->read(buffer, sizeof(buffer));

  // Then
  ASSERT_EQ(read_bytes, 10001);
  for (size_t i = 0; i < read_bytes; ++i)
    ASSERT_EQ(buffer[i], i % 128);
  try {
    _file->read(buffer, sizeof(buffer));
  } catch (const std::exception& e) {
  }
}
