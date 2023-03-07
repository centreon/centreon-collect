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
#include <absl/types/span.h>
#include <gtest/gtest.h>

#include "com/centreon/broker/exceptions/shutdown.hh"
#include "com/centreon/broker/file/cfile.hh"
#include "com/centreon/broker/file/splitter.hh"
#include "com/centreon/broker/file/stream.hh"
#include "com/centreon/broker/misc/filesystem.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::file;

#define BIG 50000
#define RETENTION_DIR "/tmp/"
#define RETENTION_FILE "test-concurrent-queue"

class read_thread {
  std::thread _thread;
  file::splitter* _file;
  int _current;
  std::vector<uint8_t> _buf;
  int _size;

 public:
  read_thread(file::splitter* f, int size)
      : _file(f), _current(0), _buf(size, '\0'), _size(size) {
    _thread = std::thread(&read_thread::callback, this);
  }

  void join() { _thread.join(); }
  std::vector<uint8_t> get_result() { return _buf; }

  void callback() {
    int ret = 0;

    do {
      try {
        ret = _file->read(_buf.data() + _current, _size - _current);
        _current += ret;
        ASSERT_TRUE(_current <= _size);
      } catch (...) {
      }
      usleep(100);
    } while (_current < _size);
  }
};

class write_thread {
  std::thread _thread;
  file::splitter* _file;
  int _size;

 public:
  write_thread(file::splitter* f, int size) : _file(f), _size(size) {
    _thread = std::thread(&write_thread::callback, this);
  }

  void join() { _thread.join(); }

  void callback() {
    char* buf = new char[_size];
    for (int i = 0; i < _size; ++i)
      buf[i] = i & 255;

    int wb = 0;
    for (int j = 0; j < _size; j += wb) {
      wb = _file->write(buf + j, 100);
      std::cout << "OK\n";
      usleep(rand() % 100);
    }

    delete[] buf;
  }
};

class FileSplitterConcurrent : public ::testing::Test {
 public:
  void SetUp() override {
    _path = RETENTION_DIR RETENTION_FILE;
    _remove_files();

    _file = std::make_unique<file::splitter>(
        _path, file::fs_file::open_read_write_truncate, 10000, true);
  }

 protected:
  std::unique_ptr<file::splitter> _file;
  std::string _path;

  void _remove_files() {
    std::list<std::string> entries = misc::filesystem::dir_content_with_filter(
        RETENTION_DIR, RETENTION_FILE "*");
    for (std::list<std::string>::iterator it{entries.begin()},
         end{entries.end()};
         it != end; ++it)
      std::remove(it->c_str());
  }
};

// Given a splitter object
// When we write and read at the same time the object
// Then the read buffer contains the same content than the written buffer.
TEST_F(FileSplitterConcurrent, DefaultFile) {
  write_thread wt(_file.get(), 1000);
  read_thread rt(_file.get(), 1000);

  wt.join();
  rt.join();

  std::vector<uint8_t> const& result(rt.get_result());
  std::vector<uint8_t> buffer(1000, '\0');
  for (int i(0); i < 1000; ++i)
    buffer[i] = i & 255;

  // Then
  ASSERT_EQ(buffer.size(), result.size());
  ASSERT_EQ(buffer, result);
}

// Given a splitter object
// When we write and read at the same time the object while data are too
// long to be store in a simple file
// Then the read buffer contains the same content than the written buffer.
// And when we call the remove_all_files() method
// Then all the created files are removed.
TEST_F(FileSplitterConcurrent, MultipleFilesCreated) {
  _file->remove_all_files();
  write_thread wt(_file.get(), BIG);
  read_thread rt(_file.get(), BIG);

  rt.join();
  wt.join();

  std::vector<uint8_t> result(rt.get_result());
  std::vector<uint8_t> buffer(BIG, '\0');
  for (int i(0); i < BIG; ++i)
    buffer[i] = i & 255;

  // Then
  ASSERT_EQ(buffer.size(), result.size());
  ASSERT_EQ(buffer, result);

  // Then
  _file->remove_all_files();
  std::list<std::string> entries = misc::filesystem::dir_content_with_filter(
      RETENTION_DIR, RETENTION_FILE "*");
  ASSERT_EQ(entries.size(), 0u);
}

// Given a splitter object
// When twenty writers write in parallel in the splitter
// Then the reader can read all what they wrote and data are not corrupted.
TEST_F(FileSplitterConcurrent, ConcurrentWriteFile10) {
  constexpr int COUNT = 20;
  constexpr int LENGTH = 1000;
  constexpr int BLOCK = 100;
  std::vector<std::thread> v;
  for (int i = 0; i < COUNT; i++)
    v.emplace_back([file = _file.get()]() {
      char* buf = new char[LENGTH];
      char v = 1;
      int block = BLOCK;
      for (int i = 0; i < LENGTH; ++i) {
        if (i >= block) {
          block += BLOCK;
          v++;
        }
        buf[i] = v;
      }

      int wb = 0;
      for (int j = 0; j < LENGTH; j += wb) {
        wb = file->write(buf + j, BLOCK);
      }

      delete[] buf;
    });

  std::vector<uint8_t> result(COUNT * LENGTH, '\0');

  std::thread rt([file = _file.get(), &result]() {
    int ret = 0;
    int current = 0;
    constexpr int size = COUNT * LENGTH;

    do {
      try {
        ret = file->read(result.data() + current, size - current);
        current += ret;
        ASSERT_TRUE(current <= size);
      } catch (...) {
      }
      usleep(100);
    } while (current < size);
  });

  for (auto& t : v)
    t.join();

  rt.join();

  ASSERT_EQ(result.size(), COUNT * LENGTH);
  ASSERT_EQ(result.size() % BLOCK, 0);
  for (uint32_t delta = 0; delta < result.size(); delta += BLOCK) {
    absl::Span<uint8_t> res(const_cast<uint8_t*>(result.data()) + delta, BLOCK);

    // Then
    char v = res[0];
    for (uint32_t i = 0; i < res.size(); i++) {
      ASSERT_EQ(v, res[i]) << " where res[0] = " << v << " delta = " << delta
                           << " and i = " << i;
    }
  }
}
